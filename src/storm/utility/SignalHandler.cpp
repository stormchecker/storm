#include "SignalHandler.h"

#include <csignal>
#include <iostream>
#include <mutex>

namespace storm {
namespace utility {
namespace resources {

// Maximal waiting time after abort signal before terminating
static int maxWaitTime = 0;

// Registry of child processes that were spawned by Storm and should be killed whenever Storm is terminated by a
// signal. The registry is deliberately kept as a plain array of integral values so that it can be safely accessed
// from within the signal handler (where no locks may be acquired).
namespace {

constexpr std::size_t maxNumberOfChildProcesses = 64;
volatile sig_atomic_t childProcesses[maxNumberOfChildProcesses] = {};
volatile sig_atomic_t numberOfChildProcesses = 0;
std::mutex childProcessMutex;

int indexOfChildProcess(pid_t pid) {
    for (sig_atomic_t i = 0; i < numberOfChildProcesses; ++i) {
        if (childProcesses[i] == static_cast<sig_atomic_t>(pid)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace

void registerChildProcess(pid_t pid) {
    if (pid <= 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(childProcessMutex);
    if (indexOfChildProcess(pid) != -1) {
        return;
    }
    if (numberOfChildProcesses < static_cast<sig_atomic_t>(maxNumberOfChildProcesses)) {
        childProcesses[numberOfChildProcesses] = static_cast<sig_atomic_t>(pid);
        ++numberOfChildProcesses;
    }
}

void unregisterChildProcess(pid_t pid) {
    std::lock_guard<std::mutex> lock(childProcessMutex);
    int index = indexOfChildProcess(pid);
    if (index >= 0) {
        sig_atomic_t lastIndex = numberOfChildProcesses - 1;
        childProcesses[index] = childProcesses[lastIndex];
        childProcesses[lastIndex] = 0;
        numberOfChildProcesses = lastIndex;
    }
}

void terminateChildProcesses() {
    // Note: This function is called from within the signal handler and therefore must not acquire any locks.
    for (sig_atomic_t i = 0; i < numberOfChildProcesses; ++i) {
        pid_t pid = static_cast<pid_t>(childProcesses[i]);
        if (pid > 0) {
            kill(pid, SIGKILL);
        }
    }
}

ChildProcessGuard::ChildProcessGuard(pid_t pid) : pid(pid) {
    registerChildProcess(pid);
}

ChildProcessGuard::~ChildProcessGuard() {
    unregisterChildProcess(pid);
}

pid_t ChildProcessGuard::getPid() const {
    return pid;
}

SignalInformation::SignalInformation() : terminate(false), lastSignal(0) {}

SignalInformation::~SignalInformation() {
    // Intentionally left empty.
}

SignalInformation& SignalInformation::infos() {
    static SignalInformation signalInfo;
    return signalInfo;
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
    if (!isTerminate()) {
        // First time we get an abort signal
        // We give the program a number of seconds to print results obtained so far before termination
        std::cerr << "ERROR: The program received signal " << signal << " and will be aborted in " << maxWaitTime << "s.\n";
        SignalInformation::infos().setTerminate(true);
        // Remember original signal such that the program returns the correct original signal
        SignalInformation::infos().setErrorCode(signal);
        // Kill spawned subprocesses right away as they are no longer needed and might otherwise block termination.
        terminateChildProcesses();
        // Trigger a new signal after waitTime
        setTimeoutAlarm(maxWaitTime);
    } else {
        // Second time we get a signal
        // We now definitely have to terminate as fast as possible
        if (SignalInformation::infos().getErrorCode() == SIGXCPU) {
            std::cerr << "TIMEOUT.\n";
        } else if (SignalInformation::infos().getErrorCode() == SIGABRT || SignalInformation::infos().getErrorCode() == SIGINT) {
            std::cerr << "ABORT.\n";
        } else {
            std::cerr << "Received signal " << signal << '\n';
        }
        // Make sure that we do not leave any spawned subprocesses behind before terminating.
        terminateChildProcesses();
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
    // Note: There is no signal that is raised when the process runs out of memory (the OOM killer uses SIGKILL,
    // which cannot be caught). Hence, ENOMEM must not be registered as a signal.
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
