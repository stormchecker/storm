#include "SignalHandler.h"

#include <csignal>
#include <cstddef>
#include <iostream>

namespace storm {
namespace utility {
namespace resources {

// Maximal waiting time after abort signal before terminating
static int maxWaitTime = 0;

namespace {
// Async-signal-safe: writes the decimal representation of value to fd using only write() and stack-local arithmetic.
void writeIntSignalSafe(int fd, int value) {
    char buffer[16];
    int pos = static_cast<int>(sizeof(buffer));
    bool negative = value < 0;
    unsigned int uvalue = negative ? static_cast<unsigned int>(-value) : static_cast<unsigned int>(value);
    if (uvalue == 0) {
        buffer[--pos] = '0';
    }
    while (uvalue > 0) {
        buffer[--pos] = static_cast<char>('0' + (uvalue % 10));
        uvalue /= 10;
    }
    if (negative) {
        buffer[--pos] = '-';
    }
    ssize_t written = write(fd, buffer + pos, static_cast<size_t>(sizeof(buffer) - pos));
    (void)written;
}

void writeLiteralSignalSafe(int fd, char const* literal, size_t length) {
    ssize_t written = write(fd, literal, length);
    (void)written;
}
}  // namespace

#define SIGNAL_SAFE_WRITE_LITERAL(fd, literal) writeLiteralSignalSafe((fd), (literal), sizeof(literal) - 1)

SignalInformation::SignalInformation() : terminate(false), lastSignal(0) {}

SignalInformation::~SignalInformation() {
    // Intentionally left empty.
}

SignalInformation& SignalInformation::infos() {
    static SignalInformation signalInfo;
    return signalInfo;
}

#if defined(__clang__)
// Suppress warning in thread safety analysis: the lock_guard below is scoped to just the copy, not the invocation, so there is no actual deadlock.
#define STORM_NO_THREAD_SAFETY_ANALYSIS __attribute__((no_thread_safety_analysis))
#else
#define STORM_NO_THREAD_SAFETY_ANALYSIS
#endif

bool SignalInformation::isTerminate() STORM_NO_THREAD_SAFETY_ANALYSIS {
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

std::function<bool()> SignalInformation::installCallback(std::function<bool()> callback) STORM_NO_THREAD_SAFETY_ANALYSIS {
    std::lock_guard<std::mutex> lock(callbackMutex);
    std::function<bool()> previous = std::move(currentCallback);
    currentCallback = std::move(callback);
    return previous;
}

void SignalInformation::restoreCallback(std::function<bool()> previous) STORM_NO_THREAD_SAFETY_ANALYSIS {
    std::lock_guard<std::mutex> lock(callbackMutex);
    currentCallback = std::move(previous);
}

#undef STORM_NO_THREAD_SAFETY_ANALYSIS

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
        SIGNAL_SAFE_WRITE_LITERAL(STDERR_FILENO, "ERROR: The program received signal ");
        writeIntSignalSafe(STDERR_FILENO, signal);
        SIGNAL_SAFE_WRITE_LITERAL(STDERR_FILENO, " and will be aborted in ");
        writeIntSignalSafe(STDERR_FILENO, maxWaitTime);
        SIGNAL_SAFE_WRITE_LITERAL(STDERR_FILENO, "s.\n");
        SignalInformation::infos().setTerminate(true);
        // Remember original signal such that the program returns the correct original signal
        SignalInformation::infos().setErrorCode(signal);
        // Trigger a new signal after waitTime
        setTimeoutAlarm(maxWaitTime);
    } else {
        // Second time we get a signal
        // We now definitely have to terminate as fast as possible
        if (SignalInformation::infos().getErrorCode() == SIGXCPU) {
            SIGNAL_SAFE_WRITE_LITERAL(STDERR_FILENO, "TIMEOUT.\n");
        } else if (SignalInformation::infos().getErrorCode() == SIGABRT || SignalInformation::infos().getErrorCode() == SIGINT) {
            SIGNAL_SAFE_WRITE_LITERAL(STDERR_FILENO, "ABORT.\n");
        } else {
            SIGNAL_SAFE_WRITE_LITERAL(STDERR_FILENO, "Received signal ");
            writeIntSignalSafe(STDERR_FILENO, signal);
            SIGNAL_SAFE_WRITE_LITERAL(STDERR_FILENO, "\n");
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
