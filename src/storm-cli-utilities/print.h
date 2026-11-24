#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "storm/utility/macros.h"

/*!
 * Define the macros that print information to stdout and optionally also log it.
 * These are CLI-only: library code must not write directly to stdout, so that
 * downstream consumers (e.g. Python bindings) can control output purely through logging.
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

namespace storm {
namespace cli {

/*!
 * For a command-line argument, returns a quoted version
 * with single quotes if it contains unsafe characters.
 * Otherwise, just returns the unquoted argument.
 */
std::string shellQuoteSingleIfNecessary(const std::string& arg);

void printHeader(std::string const& name, const int argc, const char** argv);

void printVersion();

void printTimeAndMemoryStatistics(uint64_t wallclockMilliseconds = 0);

}  // namespace cli
}  // namespace storm
