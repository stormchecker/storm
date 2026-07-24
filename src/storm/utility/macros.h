#pragma once

#include "storm/utility/logging.h"

#include <cassert>
#include <iosfwd>

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
#else
#define STORM_LOG_ASSERT(cond, message)
#define STORM_LOG_WARN_COND_DEBUG(cond, message)
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

/*!
 * Define the macros that print information and optionally also log it.
 */
#define STORM_PRINT(message)  \
    do {                      \
        std::cout << message; \
        std::cout.flush();    \
    } while (false)

#define STORM_PRINT_AND_LOG(message) \
    do {                             \
        STORM_LOG_INFO(message);     \
        STORM_PRINT(message);        \
    } while (false)
