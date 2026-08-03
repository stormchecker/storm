#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundef"
#include "gtest/gtest.h"
#pragma clang diagnostic pop

#include <boost/optional/optional_io.hpp>

#include "storm/adapters/RationalNumberForward.h"
#include "storm/utility/constants.h"
#include "storm/utility/initialize.h"

#define STORM_SILENT_ASSERT_THROW(statement, expected_exception) \
    storm::test::disableOutput();                                \
    ASSERT_THROW(statement, expected_exception);                 \
    storm::test::enableErrorOutput()

#define STORM_SILENT_EXPECT_THROW(statement, expected_exception) \
    storm::test::disableOutput();                                \
    EXPECT_THROW(statement, expected_exception);                 \
    storm::test::enableErrorOutput()

namespace testing {
namespace internal {

GTEST_API_ AssertionResult DoubleNearPredFormat(const char* expr1, const char* expr2, const char* abs_error_expr, storm::RationalNumber val1,
                                                storm::RationalNumber val2, storm::RationalNumber abs_error);
}  // namespace internal
}  // namespace testing

namespace storm {
namespace test {
extern bool noGurobi;

void initialize(int* argc, char** argv);

inline void enableErrorOutput() {
    // Only decrease the log level
    if (storm::utility::getLogLevel() > l3pp::LogLevel::ERR) {
        storm::utility::setLogLevel(l3pp::LogLevel::ERR);
    }
}

inline void disableOutput() {
    storm::utility::setLogLevel(l3pp::LogLevel::OFF);
}

// Check for valid Gurobi license
bool testGurobiLicense();

// Some tests have to be skipped for specific z3 versions because of a bug that was present in z3.
#ifdef STORM_HAVE_Z3
bool z3AtLeastVersion(unsigned expectedMajor, unsigned expectedMinor, unsigned expectedBuildNumber);
#endif
}  // namespace test
}  // namespace storm
