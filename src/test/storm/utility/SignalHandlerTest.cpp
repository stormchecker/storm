#include "test/storm_gtest.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <thread>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "storm/utility/SignalHandler.h"

namespace {

// Waiting time (in seconds) that we configure for the signal handler in the tests.
constexpr int signalWaitTime = 1;

//! Forks a process that just idles until it is killed. Returns the pid of the child (or -1 on failure).
pid_t forkPausedProcess() {
    pid_t pid = fork();
    if (pid == 0) {
        // Child: idle until killed.
        while (true) {
            pause();
        }
        _exit(0);
    }
    return pid;
}

//! Waits for the given process to exit, killing and reaping it if it does not exit within the timeout.
bool waitForExitWithTimeout(pid_t pid, int& status, int timeoutSeconds) {
    for (int i = 0; i < timeoutSeconds * 100; ++i) {
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            return true;
        }
        if (result == -1 && errno != EINTR) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    kill(pid, SIGKILL);
    while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {
    }
    return false;
}

//! Checks whether the given process still exists (including zombie state).
bool isProcessAlive(pid_t pid) {
    return kill(pid, 0) == 0;
}

//! Kills the given process (if it is our child, it is also reaped).
void killProcess(pid_t pid) {
    if (pid <= 0) {
        return;
    }
    kill(pid, SIGKILL);
    while (waitpid(pid, nullptr, 0) == -1 && errno == EINTR) {
    }
}

}  // namespace

// Regression test for https://github.com/stormchecker/storm/issues/74:
// installing the signal handler must not hijack unrelated signals. Previously, ENOMEM was (incorrectly) passed to
// sigaction which, depending on the platform, corresponds to an unrelated signal such as SIGUSR2.
TEST(SignalHandlerTest, DoesNotHijackUnrelatedSignals) {
    pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        // Child: install an own handler for SIGUSR2 and make sure that it survives installing Storm's signal handler.
        struct sigaction ownHandler;
        ownHandler.sa_handler = [](int) {};
        sigemptyset(&ownHandler.sa_mask);
        ownHandler.sa_flags = 0;
        sigaction(SIGUSR2, &ownHandler, nullptr);

        storm::utility::resources::installSignalHandler(signalWaitTime);

        raise(SIGUSR2);
        // If Storm hijacked SIGUSR2, we would be terminated by the (alarm-driven) signal handler before reaching this.
        std::this_thread::sleep_for(std::chrono::seconds(signalWaitTime + 1));
        _exit(42);
    }

    int status = 0;
    ASSERT_TRUE(waitForExitWithTimeout(pid, status, signalWaitTime + 5)) << "The forked child did not terminate in time.";
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 42) << "SIGUSR2 was hijacked by the Storm signal handler.";
}

// Regression test for https://github.com/stormchecker/storm/issues/74:
// registered child processes are killed when Storm aborts.
TEST(SignalHandlerTest, TerminateChildProcesses) {
    pid_t child = forkPausedProcess();
    ASSERT_GE(child, 0);

    storm::utility::resources::registerChildProcess(child);
    storm::utility::resources::terminateChildProcesses();

    int status = 0;
    ASSERT_TRUE(waitForExitWithTimeout(child, status, 5)) << "The registered child process was not terminated.";
    EXPECT_TRUE(WIFSIGNALED(status));
    EXPECT_EQ(WTERMSIG(status), SIGKILL);

    storm::utility::resources::unregisterChildProcess(child);
}

// Unregistered child processes must not be killed.
TEST(SignalHandlerTest, UnregisterChildProcess) {
    pid_t child = forkPausedProcess();
    ASSERT_GE(child, 0);

    storm::utility::resources::registerChildProcess(child);
    storm::utility::resources::unregisterChildProcess(child);
    storm::utility::resources::terminateChildProcesses();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(isProcessAlive(child)) << "An unregistered child process must not be killed.";

    killProcess(child);
}

// Regression test for https://github.com/stormchecker/storm/issues/74:
// a child process that was spawned by Storm is killed when Storm terminates due to a signal.
TEST(SignalHandlerTest, SignalHandlerKillsChildProcesses) {
    int pipeFds[2];
    ASSERT_EQ(pipe(pipeFds), 0);

    pid_t subject = fork();
    ASSERT_GE(subject, 0);
    if (subject == 0) {
        // Child acting as "Storm".
        close(pipeFds[0]);

        storm::utility::resources::installSignalHandler(signalWaitTime);

        pid_t grandchild = forkPausedProcess();
        if (grandchild < 0) {
            _exit(3);
        }
        storm::utility::resources::registerChildProcess(grandchild);

        // Report the grandchild's pid to the test process.
        ssize_t written = write(pipeFds[1], &grandchild, sizeof(grandchild));
        if (written != static_cast<ssize_t>(sizeof(grandchild))) {
            _exit(5);
        }
        close(pipeFds[1]);

        // Trigger the abort. The signal handler will kill the registered grandchild and terminate this process.
        kill(getpid(), SIGINT);
        while (true) {
            pause();
        }
        _exit(4);
    }

    close(pipeFds[1]);
    pid_t grandchild = -1;
    ssize_t bytesRead = read(pipeFds[0], &grandchild, sizeof(grandchild));
    while (bytesRead == -1 && errno == EINTR) {
        bytesRead = read(pipeFds[0], &grandchild, sizeof(grandchild));
    }
    close(pipeFds[0]);
    ASSERT_EQ(bytesRead, static_cast<ssize_t>(sizeof(grandchild))) << "Did not receive the grandchild's pid.";

    int status = 0;
    ASSERT_TRUE(waitForExitWithTimeout(subject, status, signalWaitTime + 5)) << "The forked process did not terminate in time.";
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), SIGINT);

    // The grandchild was reparented when the subject terminated. Give the OS a moment to reap it.
    bool alive = isProcessAlive(grandchild);
    for (int i = 0; i < 100 && alive; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        alive = isProcessAlive(grandchild);
    }
    if (alive) {
        kill(grandchild, SIGKILL);
    }
    EXPECT_FALSE(alive) << "The signal handler did not terminate the registered child process.";
}

// Regression test for https://github.com/stormchecker/storm/issues/74:
// a registered child process is killed as soon as the first termination signal is received (and not only after the
// configured waiting time has elapsed).
TEST(SignalHandlerTest, SignalHandlerKillsChildProcessesPromptly) {
    // Use a waiting time that is large enough to distinguish a prompt kill from termination via the alarm.
    constexpr int longWaitTime = 30;

    int pipeFds[2];
    ASSERT_EQ(pipe(pipeFds), 0);

    pid_t subject = fork();
    ASSERT_GE(subject, 0);
    if (subject == 0) {
        close(pipeFds[0]);

        storm::utility::resources::installSignalHandler(longWaitTime);

        pid_t grandchild = forkPausedProcess();
        if (grandchild < 0) {
            _exit(3);
        }
        storm::utility::resources::registerChildProcess(grandchild);

        ssize_t written = write(pipeFds[1], &grandchild, sizeof(grandchild));
        if (written != static_cast<ssize_t>(sizeof(grandchild))) {
            _exit(5);
        }
        close(pipeFds[1]);

        // Trigger the abort. This must kill the registered grandchild right away.
        kill(getpid(), SIGTERM);

        // Simulate a graceful termination: as soon as we observe the termination request, we exit.
        if (!storm::utility::resources::isTerminate()) {
            _exit(6);
        }
        _exit(7);
    }

    close(pipeFds[1]);
    pid_t grandchild = -1;
    ssize_t bytesRead = read(pipeFds[0], &grandchild, sizeof(grandchild));
    while (bytesRead == -1 && errno == EINTR) {
        bytesRead = read(pipeFds[0], &grandchild, sizeof(grandchild));
    }
    close(pipeFds[0]);
    ASSERT_EQ(bytesRead, static_cast<ssize_t>(sizeof(grandchild))) << "Did not receive the grandchild's pid.";

    int status = 0;
    ASSERT_TRUE(waitForExitWithTimeout(subject, status, 10)) << "The forked process did not terminate promptly.";
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 7);

    bool alive = isProcessAlive(grandchild);
    for (int i = 0; i < 100 && alive; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        alive = isProcessAlive(grandchild);
    }
    if (alive) {
        kill(grandchild, SIGKILL);
    }
    EXPECT_FALSE(alive) << "The signal handler did not terminate the registered child process promptly.";
}
