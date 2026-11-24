#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundef"
#include <gtest/gtest.h>
#pragma clang diagnostic pop

#include <type_traits>

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

namespace storm::utility {
// Only declared here so that the overloads below can be, which keeps this header from pulling in the rational number
// adapter into every test.
template<typename ValueType>
class ExtendedNumber;
}  // namespace storm::utility

namespace storm::test {
template<typename T>
struct IsExtendedNumber : std::false_type {};

template<typename T>
struct IsExtendedNumber<storm::utility::ExtendedNumber<T>> : std::true_type {};

/// The value type underlying a possibly extended one.
template<typename T>
struct FiniteValueTypeOf {
    typedef T type;
};

template<typename T>
struct FiniteValueTypeOf<storm::utility::ExtendedNumber<T>> {
    typedef T type;
};

template<typename T>
bool isInfiniteValue(storm::utility::ExtendedNumber<T> const& value) {
    return value.isInfinite();
}

template<typename T>
    requires(!IsExtendedNumber<T>::value)
bool isInfiniteValue(T const&) {
    return false;
}

template<typename T>
T const& finiteValue(storm::utility::ExtendedNumber<T> const& value) {
    return value.getFinite();
}

template<typename T>
    requires(!IsExtendedNumber<T>::value)
T const& finiteValue(T const& value) {
    return value;
}
}  // namespace storm::test

namespace testing {
namespace internal {

GTEST_API_ AssertionResult DoubleNearPredFormat(const char* expr1, const char* expr2, const char* abs_error_expr, storm::RationalNumber val1,
                                                storm::RationalNumber val2, storm::RationalNumber abs_error);

/*!
 * Check results hold their values in the type extended with the infinities, so an EXPECT_NEAR on one of them lands
 * here. An infinite value is never near anything, which is what makes the failure message say so rather than the
 * comparison failing to compile.
 */
template<typename T1, typename T2>
    requires(storm::test::IsExtendedNumber<T1>::value || storm::test::IsExtendedNumber<T2>::value)
AssertionResult DoubleNearPredFormat(const char* expr1, const char* expr2, const char* abs_error_expr, T1 const& val1, T2 const& val2,
                                     typename storm::test::FiniteValueTypeOf<T1>::type const& absError) {
    if (storm::test::isInfiniteValue(val1) || storm::test::isInfiniteValue(val2)) {
        // An infinity is within any distance of itself and of nothing else.
        if (val1 == val2) {
            return AssertionSuccess();
        }
        return AssertionFailure() << "The difference between " << expr1 << " and " << expr2 << " is not within " << abs_error_expr << ", where\n"
                                  << expr1 << " evaluates to " << val1 << ",\n"
                                  << expr2 << " evaluates to " << val2 << ".";
    }
    return DoubleNearPredFormat(expr1, expr2, abs_error_expr, storm::test::finiteValue(val1), storm::test::finiteValue(val2), absError);
}
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
