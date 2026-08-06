#include "test/storm_gtest.h"

#ifdef STORM_HAVE_Z3
#include <z3.h>
#endif

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/GurobiLicenseException.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GurobiSettings.h"
#include "storm/solver/GurobiLpSolver.h"
#include "storm/utility/solver.h"

namespace testing {
namespace internal {

GTEST_API_ AssertionResult DoubleNearPredFormat(const char* expr1, const char* expr2, const char* abs_error_expr, storm::RationalNumber val1,
                                                storm::RationalNumber val2, storm::RationalNumber abs_error) {
    const storm::RationalNumber diff = storm::utility::abs<storm::RationalNumber>(val1 - val2);
    if (diff <= abs_error) {
        return AssertionSuccess();
    }
    return AssertionFailure() << "The difference between " << expr1 << " and " << expr2 << " is " << diff << " (approx. "
                              << storm::utility::convertNumber<double>(diff) << "), which exceeds " << abs_error_expr << ", where\n"
                              << expr1 << " evaluates to " << val1 << " (approx. " << storm::utility::convertNumber<double>(val1) << "),\n"
                              << expr2 << " evaluates to " << val2 << " (approx. " << storm::utility::convertNumber<double>(val2) << "),\n"
                              << abs_error_expr << " evaluates to " << abs_error << " (approx. " << storm::utility::convertNumber<double>(abs_error) << ").";
}
}  // namespace internal
}  // namespace testing

namespace storm::test {
bool noGurobi = false;

void initialize(int* argc, char** argv) {
    // GoogleTest-specific commandline arguments should already be processed before and removed from argc/argv
    storm::utility::initializeLogger();
    // Only enable error output by default.
    storm::utility::setLogLevel(l3pp::LogLevel::ERR);
    noGurobi = !storm::test::testGurobiLicense();
    for (int i = 1; i < *argc; ++i) {
        if (std::string(argv[i]) == "--nogurobi") {
            noGurobi = true;
        } else {
            STORM_LOG_WARN("Unknown argument: " << argv[i]);
        }
    }
}

bool testGurobiLicense() {
#ifdef STORM_HAVE_GUROBI
    if (!storm::settings::hasModule<storm::settings::modules::GurobiSettings>()) {
        return true;  // Gurobi not relevant for this test suite
    }
    try {
        storm::Environment env;
        auto lpSolver = storm::utility::solver::getLpSolver<double>(env, "test", storm::solver::LpSolverTypeSelection::Gurobi);
    } catch (storm::exceptions::GurobiLicenseException const&) {
        return false;
    }
    return true;
#else
    return false;
#endif
}

// Some tests have to be skipped for specific z3 versions because of a bug that was present in z3.
#ifdef STORM_HAVE_Z3
bool z3AtLeastVersion(unsigned expectedMajor, unsigned expectedMinor, unsigned expectedBuildNumber) {
    std::vector<unsigned> actual(4), expected({expectedMajor, expectedMinor, expectedBuildNumber, 0u});
    Z3_get_version(&actual[0], &actual[1], &actual[2], &actual[3]);
    for (uint64_t i = 0; i < 4; ++i) {
        if (actual[i] > expected[i]) {
            return true;
        }
        if (actual[i] < expected[i]) {
            return false;
        }
    }
    return true;  // Equal versions
}
#endif

}  // namespace storm::test
