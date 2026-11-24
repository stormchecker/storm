#include "storm-config.h"
#include "test/storm_gtest.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "storm/utility/SignalHandler.h"

using storm::utility::resources::ScopedTerminationCallback;

// SignalInformation is a process-wide singleton, so every test must reset its flag state.
class SignalHandlerTest : public ::testing::Test {
   protected:
    void SetUp() override {
        storm::utility::resources::resetTerminate();
    }
    void TearDown() override {
        storm::utility::resources::resetTerminate();
    }
};

TEST_F(SignalHandlerTest, NoCallbackDefaultsToFalse) {
    EXPECT_FALSE(storm::utility::resources::isTerminate());
}

TEST_F(SignalHandlerTest, CallbackEventuallyTriggersTermination) {
    int invocationCount = 0;
    constexpr int triggerAfter = 3;
    ScopedTerminationCallback guard([&invocationCount]() { return ++invocationCount >= triggerAfter; });

    bool observedTerminate = false;
    for (int i = 0; i < triggerAfter; ++i) {
        if (storm::utility::resources::isTerminate()) {
            observedTerminate = true;
            break;
        }
    }
    EXPECT_TRUE(observedTerminate);
    EXPECT_EQ(invocationCount, triggerAfter);
}

TEST_F(SignalHandlerTest, FlagLatchesAfterCallbackReturnsTrue) {
    int invocationCount = 0;
    ScopedTerminationCallback guard([&invocationCount]() {
        ++invocationCount;
        return true;
    });
    ASSERT_TRUE(storm::utility::resources::isTerminate());
    int const countAfterFirstTrue = invocationCount;
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(storm::utility::resources::isTerminate());
    }
    EXPECT_EQ(invocationCount, countAfterFirstTrue);
}

TEST_F(SignalHandlerTest, ResetTerminateClearsFlag) {
    storm::utility::resources::SignalInformation::infos().setTerminate(true);
    ASSERT_TRUE(storm::utility::resources::isTerminate());
    storm::utility::resources::resetTerminate();
    EXPECT_FALSE(storm::utility::resources::isTerminate());
}

TEST_F(SignalHandlerTest, DestroyingGuardStopsInvocation) {
    int invocationCount = 0;
    {
        ScopedTerminationCallback guard([&invocationCount]() {
            ++invocationCount;
            return false;
        });
        EXPECT_FALSE(storm::utility::resources::isTerminate());
        EXPECT_GE(invocationCount, 1);
    }  // guard destroyed: callback uninstalled
    int const countAtDestruction = invocationCount;
    EXPECT_FALSE(storm::utility::resources::isTerminate());
    EXPECT_EQ(invocationCount, countAtDestruction);
}

TEST_F(SignalHandlerTest, CallbackReturningFalseNeverTerminates) {
    int invocationCount = 0;
    ScopedTerminationCallback guard([&invocationCount]() {
        ++invocationCount;
        return false;
    });
    for (int i = 0; i < 3; ++i) {
        EXPECT_FALSE(storm::utility::resources::isTerminate());
    }
}

// Test a callback that calls back into isTerminate() while still installed.
TEST_F(SignalHandlerTest, CallbackMayCallIsTerminateReentrantly) {
    int invocationCount = 0;
    bool nestedResult = true;  // overwritten by the callback below
    ScopedTerminationCallback guard([&invocationCount, &nestedResult]() {
        ++invocationCount;
        nestedResult = storm::utility::resources::isTerminate();
        return false;
    });
    EXPECT_FALSE(storm::utility::resources::isTerminate());
    // The reentrant, nested isTerminate() call is short-circuited by the reentrancy guard rather than
    // invoking the callback a second time, so the callback body only ever runs once.
    EXPECT_EQ(invocationCount, 1);
    EXPECT_FALSE(nestedResult);
}

TEST_F(SignalHandlerTest, StatefulCapturingCallbackNeedsNoGlobalState) {
    struct FakeInterruptSource {
        int pollCount = 0;
        bool interruptRequested = false;
        bool poll() {
            ++pollCount;
            return interruptRequested;
        }
    };
    FakeInterruptSource interruptSource;
    ScopedTerminationCallback guard([&interruptSource]() { return interruptSource.poll(); });

    EXPECT_FALSE(storm::utility::resources::isTerminate());
    interruptSource.interruptRequested = true;
    EXPECT_TRUE(storm::utility::resources::isTerminate());
    EXPECT_GE(interruptSource.pollCount, 2);
}

// Nesting two guards: while both are alive, only the innermost guard's callback is invoked. Destroying
// the inner guard restores exactly the outer guard's callback.
TEST_F(SignalHandlerTest, NestedGuardReplacesAndRestoresOuterCallback) {
    int outerInvocations = 0;
    int innerInvocations = 0;
    ScopedTerminationCallback outerGuard([&outerInvocations]() {
        ++outerInvocations;
        return false;
    });
    {
        ScopedTerminationCallback innerGuard([&innerInvocations]() {
            ++innerInvocations;
            return false;
        });
        EXPECT_FALSE(storm::utility::resources::isTerminate());
        EXPECT_GE(innerInvocations, 1);
        EXPECT_EQ(outerInvocations, 0);
    }  // innerGuard destroyed: outerGuard's callback is installed again
    EXPECT_FALSE(storm::utility::resources::isTerminate());
    EXPECT_GE(outerInvocations, 1);
}

TEST_F(SignalHandlerTest, OutOfOrderGuardDestructionKeepsLiveCallback) {
    int firstInvocations = 0;
    int secondInvocations = 0;
    auto firstGuard = std::make_unique<ScopedTerminationCallback>([&firstInvocations]() {
        ++firstInvocations;
        return false;
    });
    auto secondGuard = std::make_unique<ScopedTerminationCallback>([&secondInvocations]() {
        ++secondInvocations;
        return false;
    });
    // Destroy the outer guard first
    firstGuard.reset();
    EXPECT_FALSE(storm::utility::resources::isTerminate());
    EXPECT_EQ(firstInvocations, 0);
    EXPECT_EQ(secondInvocations, 1);
    secondGuard.reset();
    EXPECT_FALSE(storm::utility::resources::isTerminate());
    EXPECT_EQ(firstInvocations, 0);
    EXPECT_EQ(secondInvocations, 1);
}

TEST_F(SignalHandlerTest, CallbackIsNeverExecutedConcurrently) {
    std::atomic<int> activeInvocations{0};
    std::atomic<bool> overlapDetected{false};
    int invocationCount = 0;  // Non-atomic: only modified while the callback runs exclusively
    ScopedTerminationCallback guard([&]() {
        if (activeInvocations.fetch_add(1) != 0) {
            overlapDetected = true;
        }
        ++invocationCount;
        std::this_thread::yield();
        activeInvocations.fetch_sub(1);
        return false;
    });
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([]() {
            for (int i = 0; i < 1000; ++i) {
                storm::utility::resources::isTerminate();
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_FALSE(overlapDetected);
    EXPECT_GE(invocationCount, 1);
    EXPECT_FALSE(storm::utility::resources::isTerminate());
}
