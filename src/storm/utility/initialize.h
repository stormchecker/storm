#pragma once

#include "storm/utility/logging.h"

namespace storm {
namespace utility {
/*!
 * Initializes the logging framework and sets up logging to console.
 */
void initializeLogger();
/*!
 * Performs some necessary initializations.
 */
void setUp();

/*!
 * Performs some necessary clean-up.
 */
void cleanUp();

/*!
 * Set number of digits for printing output from given precision requirement.
 * For a precision of 1e-n we output at least n digits.
 * @param precision General precision.
 */
void setOutputDigitsFromGeneralPrecision(double precision);

/*!
 * Gets the global log level
 */
l3pp::LogLevel getLogLevel();

/*!
 * Set the global log level
 */
void setLogLevel(l3pp::LogLevel level);

/*!
 * Set the log level for the statistics channel, independently of the global log level.
 */
void setStatisticsLogLevel(l3pp::LogLevel level);

/*!
 * Set the log level for the progress channel, independently of the global log level.
 */
void setProgressLogLevel(l3pp::LogLevel level);

/*!
 * Sets up the logging to file.
 */
void initializeFileLogging(std::string const& logfileName);

}  // namespace utility
}  // namespace storm
