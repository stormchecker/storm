#pragma once

#include <cstdint>

#include "storm-parsers/storm-parsers-api.h"

namespace storm {
namespace utility {
namespace cstring {

/*!
 *	@brief Parses integer and checks, if something has been parsed.
 */
STORM_PARSERS_API uint_fast64_t checked_strtol(const char* str, char const** end);

/*!
 *	@brief Parses floating point and checks, if something has been parsed.
 */
STORM_PARSERS_API double checked_strtod(const char* str, char const** end);

/*!
 * @brief Skips all non whitespace characters until the next whitespace.
 */
STORM_PARSERS_API char const* skipWord(char const* buf);

/*!
 *	@brief Skips common whitespaces in a string.
 */
STORM_PARSERS_API char const* trimWhitespaces(char const* buf);

/*!
 * @brief Encapsulates the usage of function @strcspn to forward to the end of the line (next char is the newline character).
 */
STORM_PARSERS_API char const* forwardToLineEnd(char const* buffer);

/*!
 * @brief Encapsulates the usage of function @strchr to forward to the next line
 *
 * Note: All lines after the current, which do not contain any characters are skipped.
 */
STORM_PARSERS_API char const* forwardToNextLine(char const* buffer);

}  // namespace cstring
}  // namespace utility
}  // namespace storm
