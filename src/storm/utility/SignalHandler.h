#pragma once

#include <cstdint>
#include <cstdlib>

#include <unistd.h>

#include "storm-config.h"
#include "storm/utility/OsDetection.h"

namespace storm {
namespace utility {
namespace resources {

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
     * Check whether the program should terminate (due to some abort signal).
     *
     * @return True iff program should terminate.
     */
    inline bool isTerminate() const {
        return terminate;
    }

    /*!
     * Set whether the program should terminate.
     *
     * @param terminate True iff program should terminate.
     */
    inline void setTerminate(bool terminate) {
        this->terminate = terminate;
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
    /*!
     * Constructs new signal information. This constructor is private to forbid instantiation of this class. The only
     * way to create a new instance is by calling the static infos() method.
     */
    SignalInformation();

    /*!
     * This destructor is private, since we need to forbid explicit destruction of the signal information.
     */
    virtual ~SignalInformation();

    // Flag whether the program should terminate
    bool terminate;
    // Store last signal code
    int lastSignal;
};

inline void resetTimeoutAlarm() {
    SignalInformation::infos().setTerminate(false);
    alarm(0);
}

/*!
 * Check whether the program should terminate (due to some abort signal).
 *
 * @return True iff program should terminate.
 */
inline bool isTerminate() {
    return SignalInformation::infos().isTerminate();
}

/*!
 * Register some signal handlers to detect and correctly handle abortion (due to timeout for example).
 */
void installSignalHandler(int maximalWaitTime);

}  // namespace resources
}  // namespace utility
}  // namespace storm
