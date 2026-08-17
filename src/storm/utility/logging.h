#pragma once

// Load streaming operator from CARL
#include <carl/io/streamingOperators.h>
namespace l3pp {
using carl::operator<<;
}

#include <l3pp.h>

#include <sstream>
#include <type_traits>

#if !defined(STORM_LOG_DISABLE_DEBUG) && !defined(STORM_LOG_DISABLE_TRACE)
#define STORM_LOG_TRACE(message) L3PP_LOG_TRACE(l3pp::Logger::getRootLogger(), message)
#else
#define STORM_LOG_TRACE(message) (void)(0)
#endif

#if !defined(STORM_LOG_DISABLE_DEBUG)
#define STORM_LOG_DEBUG(message) L3PP_LOG_DEBUG(l3pp::Logger::getRootLogger(), message)
#else
#define STORM_LOG_DEBUG(message) (void)(0)
#endif

// Define STORM_LOG_WARN, STORM_LOG_ERROR and STORM_LOG_INFO to log the given message with the corresponding log levels.
#define STORM_LOG_INFO(message) L3PP_LOG_INFO(l3pp::Logger::getRootLogger(), message)
#define STORM_LOG_WARN(message) L3PP_LOG_WARN(l3pp::Logger::getRootLogger(), message)
#define STORM_LOG_ERROR(message) L3PP_LOG_ERROR(l3pp::Logger::getRootLogger(), message)

namespace storm {
namespace utility {
// Named log channels whose level can be toggled independently of the root logger's level.
constexpr const char* STATISTICS_LOG_CHANNEL = "storm.statistics";
constexpr const char* PROGRESS_LOG_CHANNEL = "storm.progress";
}  // namespace utility
}  // namespace storm

// STORM_LOG_STATISTICS and STORM_LOG_PROGRESS log at INFO level on their own channel, so their
// visibility can be enabled/disabled independently of general INFO-level verbosity.
#define STORM_LOG_STATISTICS(message) L3PP_LOG_INFO(storm::utility::STATISTICS_LOG_CHANNEL, message)
#define STORM_LOG_PROGRESS(message) L3PP_LOG_INFO(storm::utility::PROGRESS_LOG_CHANNEL, message)

// Runs `callback` and logs the result on `channel` at `level`, but only if that channel would
// actually emit at that level, avoiding message construction whenever nothing would be printed.
// `callback` must have signature `bool(std::ostream&)`: write into the stream, then return
// whether it should be logged.
//
// `callback` is taken as a variadic parameter rather than a plain one because it is typically a
// lambda, and a lambda's capture list ("[a, b, c]") contains top-level commas. The preprocessor
// only tracks parentheses to decide where macro arguments split, not square or curly brackets, so
// a plain parameter would misparse such a lambda as multiple arguments; __VA_ARGS__ folds them
// back into one.
#define STORM_LOG_LAZY_ON_CHANNEL(channel, level, ...)                                                                                                        \
    do {                                                                                                                                                      \
        auto storm_log_lazy_channel = l3pp::Logger::getLogger(channel);                                                                                       \
        if (storm_log_lazy_channel->getLevel() <= (level)) {                                                                                                  \
            auto storm_log_lazy_callback = __VA_ARGS__;                                                                                                       \
            static_assert(std::is_invocable_r_v<bool, decltype(storm_log_lazy_callback), std::ostream&>, "callback must have signature bool(std::ostream&)"); \
            std::ostringstream storm_log_lazy_stream;                                                                                                         \
            if (storm_log_lazy_callback(storm_log_lazy_stream)) {                                                                                             \
                storm_log_lazy_channel->log(level, __L3PP_LOG_RECORD) << storm_log_lazy_stream.str();                                                         \
            }                                                                                                                                                 \
        }                                                                                                                                                     \
    } while (false)

// Same contract as STORM_LOG_LAZY_ON_CHANNEL: `callback` must be `bool(std::ostream&)`, return true to emit.
#define STORM_LOG_STATISTICS_LAZY(...) STORM_LOG_LAZY_ON_CHANNEL(storm::utility::STATISTICS_LOG_CHANNEL, l3pp::LogLevel::INFO, __VA_ARGS__)
// Same contract as STORM_LOG_LAZY_ON_CHANNEL: `callback` must be `bool(std::ostream&)`, return true to emit
#define STORM_LOG_PROGRESS_LAZY(...) STORM_LOG_LAZY_ON_CHANNEL(storm::utility::PROGRESS_LOG_CHANNEL, l3pp::LogLevel::INFO, __VA_ARGS__)
