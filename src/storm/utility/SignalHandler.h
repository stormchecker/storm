#pragma once

#include <unistd.h>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <mutex>

#include "storm-config.h"
#include "storm/utility/OsDetection.h"

namespace storm {
namespace utility {
namespace resources {

// Forward declarations
class ScopedTerminationCallback;

void signalHandler(int signal);

class SignalInformation {
   public:
    // Explicitly delete copy constructor
    SignalInformation(SignalInformation const&) = delete;

    // Explicitly delete copy constructor
    void operator=(SignalInformation const&) = delete;

    /*!
     * Retrieves the only existing instance of the signal information.
     *
     * @return The only existing instance of the signal information.
     */
    static SignalInformation& infos();

    /*!
     * Check whether the program should terminate (due to some abort signal, a timeout, or a registered
     * termination callback, see ScopedTerminationCallback). If a callback is registered, it is invoked.
     *
     * Reentrancy: if the registered callback (transitively) calls isTerminate() again on the same thread,
     * the nested call does not re-invoke the callback. It is detected via a thread-local flag and simply
     * returns the current value of the terminate flag instead.
     *
     * @return True iff program should terminate.
     */
    bool isTerminate();

    /*!
     * Set whether the program should terminate. Safe to call from the (async) signal handler.
     *
     * @param terminate True iff program should terminate.
     */
    inline void setTerminate(bool terminate) {
        this->terminate.store(terminate, std::memory_order_relaxed);
    }

    /*!
     * Clears the terminate flag, without touching any pending alarm or the termination callback.
     */
    inline void resetTerminate() {
        terminate.store(false, std::memory_order_relaxed);
    }

    /*! Get the error code.
     * Default is 0. If a signal was handled, the corresponding signal code is returned.
     *
     * @return Error code.
     */
    inline int getErrorCode() const {
        return lastSignal;
    }

    /*! Set the error code.
     *
     * @param errorCode Error code.
     */
    inline void setErrorCode(int errorCode) {
        lastSignal = errorCode;
    }

   private:
    friend class ScopedTerminationCallback;
    friend void signalHandler(int signal);

    /*!
     * Check whether the program should terminate, without ever invoking the termination callback.
     * This is the only variant that may be called from the asynchronous signal handler itself, since the callback
     * is not guaranteed to be async-signal-safe.
     *
     * @return True iff the termination flag is set.
     */
    inline bool isTerminateFlagOnly() const {
        return terminate.load(std::memory_order_relaxed);
    }

    /*!
     * Installs `callback` as the new termination callback, used only by ScopedTerminationCallback.
     *
     * @return Whatever callback was previously installed (possibly empty), to be handed back to
     *         restoreCallback() when the corresponding guard is destroyed.
     */
    std::function<bool()> installCallback(std::function<bool()> callback);

    /*!
     * Restores the callback that was installed before the corresponding installCallback() call.
     */
    void restoreCallback(std::function<bool()> previous);

    /*!
     * Constructs new signal information. This constructor is private to forbid instantiation of this class. The only
     * way to create a new instance is by calling the static infos() method.
     */
    SignalInformation();

    /*!
     * This destructor is private, since we need to forbid explicit destruction of the signal information.
     */
    virtual ~SignalInformation();

    static_assert(std::atomic<bool>::is_always_lock_free,
                  "std::atomic<bool> must be lock-free for the terminate flag to remain safely writable from an async signal handler.");

    // Flag whether the program should terminate.
    std::atomic<bool> terminate;
    // Store last signal code
    int lastSignal;

    // Guards currentCallback
    std::mutex callbackMutex;
    // Pluggable termination callback
    std::function<bool()> currentCallback;
};

/*!
 * RAII (Resource Acquisition Is Initialization) guard that installs a pluggable termination callback for its lifetime.
 * Function isTerminate() additionally invokes `callback` (on top of checking the flag set by OS signal handling).
 *
 * When nesting two guards the innermost guard's callback is invoked.
 * Destroying the inner guard restores the outer guard's callback.
 */
class ScopedTerminationCallback {
   public:
    explicit ScopedTerminationCallback(std::function<bool()> callback) : previousCallback(SignalInformation::infos().installCallback(std::move(callback))) {}

    ~ScopedTerminationCallback() {
        SignalInformation::infos().restoreCallback(std::move(previousCallback));
    }

    ScopedTerminationCallback(ScopedTerminationCallback const&) = delete;
    ScopedTerminationCallback& operator=(ScopedTerminationCallback const&) = delete;
    ScopedTerminationCallback(ScopedTerminationCallback&&) = delete;
    ScopedTerminationCallback& operator=(ScopedTerminationCallback&&) = delete;

   private:
    std::function<bool()> previousCallback;
};

/*!
 * Exit without cleanup.
 * @param errorCode Error code to return.
 */
inline void quickest_exit(int errorCode) {
#if defined LINUX
    std::quick_exit(errorCode);
#elif defined MACOS
    std::_Exit(errorCode);
#else
    std::abort();
#endif
}

/*!
 * Set timeout by raising an alarm after timeout seconds.
 * @param timeout Timeout in seconds.
 */
inline void setTimeoutAlarm(uint_fast64_t timeout) {
    alarm(timeout);
}

inline void resetTimeoutAlarm() {
    SignalInformation::infos().resetTerminate();
    alarm(0);
}

/*!
 * Check whether the program should terminate (due to some abort signal, a timeout, or a registered termination callback).
 *
 * @return True iff program should terminate.
 */
inline bool isTerminate() {
    return SignalInformation::infos().isTerminate();
}

/*!
 * Clears the terminate flag, without touching any pending alarm.
 * See resetTimeoutAlarm() for a version that also cancels a pending alarm().
 */
inline void resetTerminate() {
    SignalInformation::infos().resetTerminate();
}

/*!
 * Register some signal handlers to detect and correctly handle abortion (due to timeout for example).
 */
void installSignalHandler(int maximalWaitTime);

}  // namespace resources
}  // namespace utility
}  // namespace storm
