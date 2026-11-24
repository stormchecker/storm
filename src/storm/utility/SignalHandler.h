#pragma once

#include <unistd.h>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

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
     * The callback is never executed concurrently. If another thread is currently executing the callback,
     * this call does not wait for it but simply returns the current value of the terminate flag instead.
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
     * Clears the terminate flag (and the information whether a signal was received),
     * without touching any pending alarm or the termination callback.
     */
    inline void resetTerminate() {
        terminate.store(false, std::memory_order_relaxed);
        signalReceived.store(false, std::memory_order_relaxed);
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
     * Check whether a signal was already received (and not reset since).
     *
     * @return True iff a signal was received.
     */
    inline bool isSignalReceived() const {
        return signalReceived.load(std::memory_order_relaxed);
    }

    /*!
     * Installs `callback` as the new (innermost) termination callback, used only by ScopedTerminationCallback.
     *
     * @return Unique id of the installed callback, to be handed to removeCallback() when the corresponding guard is destroyed.
     */
    uint64_t installCallback(std::function<bool()> callback);

    /*!
     * Removes the callback with the given id.
     * The most recently installed callback which is still present becomes active.
     * Guards can be destroyed in arbitrary order.
     *
     * @param id Id of the callback as returned by installCallback().
     */
    void removeCallback(uint64_t id);

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
    // Flag whether a signal was received.
    std::atomic<bool> signalReceived;
    // Store last signal code
    int lastSignal;

    // Guards callbacks and nextCallbackId
    std::mutex callbackMutex;
    // Ensures that the termination callback is never executed concurrently
    std::mutex invocationMutex;
    // Stack of installed termination callbacks together with their ids. The last entry is the active callback.
    std::vector<std::pair<uint64_t, std::function<bool()>>> callbacks;
    // Id for the next installed callback
    uint64_t nextCallbackId;
};

/*!
 * RAII (Resource Acquisition Is Initialization) guard that installs a pluggable termination callback for its lifetime.
 * Function isTerminate() additionally invokes `callback` (on top of checking the flag set by OS signal handling).
 *
 * When nesting two guards the innermost guard's callback is invoked.
 * Destroying the inner guard restores the outer guard's callback.
 * Guards may be destroyed in any order; destroying a guard only removes its own callback.
 */
class ScopedTerminationCallback {
   public:
    explicit ScopedTerminationCallback(std::function<bool()> callback) : callbackId(SignalInformation::infos().installCallback(std::move(callback))) {}

    ~ScopedTerminationCallback() {
        SignalInformation::infos().removeCallback(callbackId);
    }

    ScopedTerminationCallback(ScopedTerminationCallback const&) = delete;
    ScopedTerminationCallback& operator=(ScopedTerminationCallback const&) = delete;
    ScopedTerminationCallback(ScopedTerminationCallback&&) = delete;
    ScopedTerminationCallback& operator=(ScopedTerminationCallback&&) = delete;

   private:
    // Id of the installed callback
    uint64_t callbackId;
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
