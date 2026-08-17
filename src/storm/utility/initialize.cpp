#include "initialize.h"

#include <math.h>
#include <fstream>
#include <iostream>

namespace storm {
namespace utility {

void initializeLogger() {
    l3pp::Logger::initialize();
    // By default output to std::cout
    l3pp::SinkPtr sink = l3pp::StreamSink::create(std::cout);
    l3pp::Logger::getRootLogger()->addSink(sink);
    // Default to warn, set by user to something else
    l3pp::Logger::getRootLogger()->setLevel(l3pp::LogLevel::WARN);

    l3pp::FormatterPtr fptr =
        l3pp::makeTemplateFormatter(l3pp::FieldStr<l3pp::Field::LogLevel, 5, l3pp::Justification::LEFT>(), " (", l3pp::FieldStr<l3pp::Field::FileName>(), ':',
                                    l3pp::FieldStr<l3pp::Field::Line>(), "): ", l3pp::FieldStr<l3pp::Field::Message>(), '\n');
    sink->setFormatter(fptr);
}

/*!
 * Set number of digits for printing output.
 * @param digits Number of digits to print.
 */
void setOutputDigits(int digits) {
    std::cout.precision(digits);
}

void setUp() {
    initializeLogger();
    setOutputDigits(10);
}

void cleanUp() {
    // Intentionally left empty.
}

void setOutputDigitsFromGeneralPrecision(double precision) {
    if (precision >= 1 || precision < 0) {
        setOutputDigits(10);
    } else {
        int digits = ceil(-log10(precision)) + 4;
        // Ensure at least 10 digits for now
        if (digits < 10) {
            setOutputDigits(10);
        } else {
            setOutputDigits(digits);
        }
    }
}

l3pp::LogLevel getLogLevel() {
    return l3pp::Logger::getRootLogger()->getLevel();
}

void setLogLevel(l3pp::LogLevel level) {
    l3pp::Logger::getRootLogger()->setLevel(level);
    if (level <= l3pp::LogLevel::DEBUG) {
#ifdef STORM_LOG_DISABLE_DEBUG
        std::cout << "***** warning ***** requested loglevel is not compiled\n";
#endif
    }
}

void setStatisticsLogLevel(l3pp::LogLevel level) {
    l3pp::Logger::getLogger(storm::utility::STATISTICS_LOG_CHANNEL)->setLevel(level);
}

void setProgressLogLevel(l3pp::LogLevel level) {
    l3pp::Logger::getLogger(storm::utility::PROGRESS_LOG_CHANNEL)->setLevel(level);
}

void initializeFileLogging(std::string const& logfileName) {
    l3pp::SinkPtr sink = l3pp::StreamSink::create(logfileName);
    l3pp::Logger::getRootLogger()->addSink(sink);
}

}  // namespace utility
}  // namespace storm
