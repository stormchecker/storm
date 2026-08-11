#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundef"
#include <gtest/gtest.h>
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

// Annotate test cases that are too expensive to run in every CI run (identified via profiling) with STORM_EXPENSIVE_*.
// These macros expand to gtest's DISABLED_ prefix, so the tests are skipped by default and can be re-enabled at run time
// without reconfiguring the build, either via --gtest_also_run_disabled_tests or the GTEST_ALSO_RUN_DISABLED_TESTS=1
// environment variable.
#define STORM_EXPENSIVE_TEST(test_suite_name, test_name) TEST(test_suite_name, DISABLED_##test_name)
#define STORM_EXPENSIVE_TEST_F(test_suite_name, test_name) TEST_F(test_suite_name, DISABLED_##test_name)
#define STORM_EXPENSIVE_TEST_P(test_suite_name, test_name) TEST_P(test_suite_name, DISABLED_##test_name)
#define STORM_EXPENSIVE_TYPED_TEST(test_suite_name, test_name) TYPED_TEST(test_suite_name, DISABLED_##test_name)

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
