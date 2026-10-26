#pragma once

#include <string>
#include <vector>

#include "storm-config.h"
#include "test/storm_gtest.h"

#include <gtest/gtest.h>

#include "storm/environment/Environment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm::pars::scp::testutils {

// The LP solvers that the SCP tests are run against. Each entry is only included when the
// corresponding solver was compiled in, so the set adapts to the configuration. Z3 is deliberately
// excluded: its LP backend is an SMT encoding that is not intended as a general-purpose LP solver
// (and is considerably slower).
inline std::vector<storm::solver::LpSolverType> lpSolversUnderTest() {
    std::vector<storm::solver::LpSolverType> solvers;
#if defined STORM_HAVE_GLPK
    solvers.push_back(storm::solver::LpSolverType::Glpk);
#endif
#if defined STORM_HAVE_GUROBI
    solvers.push_back(storm::solver::LpSolverType::Gurobi);
#endif
#if defined STORM_HAVE_HIGHS
    solvers.push_back(storm::solver::LpSolverType::Highs);
#endif
#if defined STORM_HAVE_SOPLEX
    solvers.push_back(storm::solver::LpSolverType::Soplex);
#endif
    return solvers;
}

// Base fixture for the SCP tests that run against every available LP solver. Subclasses define the
// actual test suites; env() hands out an environment wired to the solver for the current
// instantiation.
class LpSolverTest : public testing::TestWithParam<storm::solver::LpSolverType> {
   public:
    void SetUp() override {
#ifdef STORM_HAVE_GUROBI
        // Gurobi can be compiled in without a license being available (as in some CI configurations). The test
        // harness probes for this at startup (see storm::test::initialize), so skip the Gurobi instantiation if
        // the solver cannot actually be used here.
        if (GetParam() == storm::solver::LpSolverType::Gurobi && storm::test::noGurobi) {
            GTEST_SKIP() << "No Gurobi license available.";
        }
#endif
    }

    storm::Environment env() const {
        storm::Environment environment;
        environment.solver().setLpSolverType(GetParam());
        return environment;
    }

    static std::string solverName(testing::TestParamInfo<storm::solver::LpSolverType> const& info) {
        return storm::solver::toString(info.param);
    }
};

}  // namespace storm::pars::scp::testutils
