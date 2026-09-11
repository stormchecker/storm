#pragma once

#include "storm/utility/logging.h"

#include <cassert>

// Define STORM_LOG_ASSERT which is only checked when NDEBUG is not set.
#ifndef NDEBUG
#define STORM_LOG_ASSERT(cond, message) \
    do {                                \
        if (!(cond)) {                  \
            STORM_LOG_ERROR(message);   \
            assert(cond);               \
        }                               \
    } while (false)
#define STORM_LOG_WARN_COND_DEBUG(cond, message) \
    do {                                         \
        if (!(cond)) {                           \
            STORM_LOG_WARN(message);             \
        }                                        \
    } while (false)
// The warning is emitted from the initializer of a function-local static, whose initialization is thread-safe and
// happens exactly once. A plain flag would be read and written without synchronization by concurrent first calls.
#define STORM_LOG_DEPRECATED(message)                        \
    do {                                                     \
        static bool const storm_deprecation_reported = [&] { \
            STORM_LOG_WARN("Deprecated: " << message);       \
            return true;                                     \
        }();                                                 \
        (void)storm_deprecation_reported;                    \
    } while (false)
#else
#define STORM_LOG_ASSERT(cond, message)
#define STORM_LOG_WARN_COND_DEBUG(cond, message)
#define STORM_LOG_DEPRECATED(message)
#endif

// Define STORM_LOG_THROW to always throw the exception with the given message if the condition fails to hold.
#define STORM_LOG_THROW(cond, exception, message) \
    do {                                          \
        if (!(cond)) {                            \
            STORM_LOG_ERROR(message);             \
            throw exception() << message;         \
        }                                         \
    } while (false)

#define STORM_LOG_WARN_COND(cond, message) \
    do {                                   \
        if (!(cond)) {                     \
            STORM_LOG_WARN(message);       \
        }                                  \
    } while (false)

#define STORM_LOG_INFO_COND(cond, message) \
    do {                                   \
        if (!(cond)) {                     \
            STORM_LOG_INFO(message);       \
        }                                  \
    } while (false)

#define STORM_LOG_ERROR_COND(cond, message) \
    do {                                    \
        if (!(cond)) {                      \
            STORM_LOG_ERROR(message);       \
        }                                   \
    } while (false)
