#include "SignalHandler.h"

#include <csignal>
#include <cstddef>
#include <iostream>

namespace storm {
namespace utility {
namespace resources {

// Maximal waiting time after abort signal before terminating
static int maxWaitTime = 0;

SignalInformation::SignalInformation() : terminate(false), lastSignal(0) {}

SignalInformation::~SignalInformation() {
    // Intentionally left empty.
}

SignalInformation& SignalInformation::infos() {
    static SignalInformation signalInfo;
    return signalInfo;
}

bool SignalInformation::isTerminate() {
    // Fast path: flag already latched by a signal, a timeout, or a previous callback invocation.
    if (terminate.load(std::memory_order_relaxed)) {
        return true;
    }

    // Reentrancy guard: if a registered callback (transitively) calls isTerminate() again on this
    // thread, do not re-invoke the callback a second time. Just report the current flag value.
    thread_local bool invokingCallback = false;
    if (invokingCallback) {
        return terminate.load(std::memory_order_relaxed);
    }

    // Copy the callback out from under the lock before invoking it: arbitrary user code (e.g. acquiring
    // the GIL, constructing/destroying further ScopedTerminationCallback guards) must never run while
    // callbackMutex is held.
    std::function<bool()> callback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex);
        callback = currentCallback;
    }
    if (!callback) {
        return false;
    }

    invokingCallback = true;
    bool result;
    try {
        result = callback();
    } catch (...) {
        invokingCallback = false;
        throw;
    }
    invokingCallback = false;

    if (result) {
        terminate.store(true, std::memory_order_relaxed);
        return true;
    }
    return false;
}

std::function<bool()> SignalInformation::installCallback(std::function<bool()> callback) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    std::function<bool()> previous = std::move(currentCallback);
    currentCallback = std::move(callback);
    return previous;
}

void SignalInformation::restoreCallback(std::function<bool()> previous) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    currentCallback = std::move(previous);
}

/*!
 * Signal handler for aborts, etc.
 * After the first signal the handler waits a number of seconds to let the program print preliminary results
 * which were computed up this point. If the waiting time is exceeded or if a second signal was raised
 * in the mean time, the program immediately terminates.
 *
 * @param signal Exit code of signal.
 */
void signalHandler(int signal) {
    // Use isTerminateFlagOnly() to not invoke the registered termination callback.
    if (!SignalInformation::infos().isTerminateFlagOnly()) {
        // First time we get an abort signal
        // We give the program a number of seconds to print results obtained so far before termination
        std::cerr << "ERROR: The program received signal " << signal << " and will be aborted in " << maxWaitTime << "s.\n";
        SignalInformation::infos().setTerminate(true);
        // Remember original signal such that the program returns the correct original signal
        SignalInformation::infos().setErrorCode(signal);
        // Trigger a new signal after waitTime
        setTimeoutAlarm(maxWaitTime);
    } else {
        // Second time we get a signal
        // We now definitely have to terminate as fast as possible
        if (SignalInformation::infos().getErrorCode() == SIGXCPU) {
            std::cerr << "TIMEOUT.\n";
        } else if (SignalInformation::infos().getErrorCode() == ENOMEM) {
            std::cerr << "OUT OF MEMORY.\n";
        } else if (SignalInformation::infos().getErrorCode() == SIGABRT || SignalInformation::infos().getErrorCode() == SIGINT) {
            std::cerr << "ABORT.\n";
        } else {
            std::cerr << "Received signal " << signal << '\n';
        }
        quickest_exit(SignalInformation::infos().getErrorCode());
    }
}

void installSignalHandler(int maximalWaitTime) {
    // Set the waiting time
    maxWaitTime = maximalWaitTime;

    // We use the newer sigaction instead of signal
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    // CPU Limit
    if (sigaction(SIGXCPU, &sa, nullptr) == -1) {
        std::cerr << "FATAL: Installing a signal handler failed.\n";
    }
    // Memory out
    if (sigaction(ENOMEM, &sa, nullptr) == -1) {
        std::cerr << "FATAL: Installing a signal handler failed.\n";
    }
    if (sigaction(SIGSEGV, &sa, nullptr) == -1) {
        std::cerr << "FATAL: Installing a signal handler failed.\n";
    }
    if (sigaction(SIGABRT, &sa, nullptr) == -1) {
        std::cerr << "FATAL: Installing a signal handler failed.\n";
    }
    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        std::cerr << "FATAL: Installing a signal handler failed.\n";
    }
    if (sigaction(SIGTERM, &sa, nullptr) == -1) {
        std::cerr << "FATAL: Installing a signal handler failed.\n";
    }
    if (sigaction(SIGALRM, &sa, nullptr) == -1) {
        std::cerr << "FATAL: Installing a signal handler failed.\n";
    }
}

}  // namespace resources
}  // namespace utility
}  // namespace storm
