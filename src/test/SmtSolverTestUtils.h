#pragma once

#include <memory>
#include <string>
#include <vector>
#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/solver/SolverSelectionOptions.h"
#include "storm/utility/solver.h"

namespace storm::test {

// The SMT solvers that tests are run against.
inline std::vector<storm::solver::SmtSolverType> smtSolversUnderTest() {
    std::vector<storm::solver::SmtSolverType> solvers;
#if defined STORM_HAVE_Z3
    solvers.push_back(storm::solver::SmtSolverType::Z3);
#endif
#if defined STORM_HAVE_MATHSAT
    solvers.push_back(storm::solver::SmtSolverType::Mathsat);
#endif
#if defined STORM_HAVE_CVC5
    solvers.push_back(storm::solver::SmtSolverType::Cvc5);
#endif
    return solvers;
}

// A factory that always creates a solver of the given type.
inline std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory(storm::solver::SmtSolverType type) {
    switch (type) {
        case storm::solver::SmtSolverType::Z3:
            return std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();
        case storm::solver::SmtSolverType::Mathsat:
            return std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();
        case storm::solver::SmtSolverType::Cvc5:
            return std::make_shared<storm::utility::solver::Cvc5SmtSolverFactory>();
    }
    return nullptr;
}

inline std::string smtSolverName(testing::TestParamInfo<storm::solver::SmtSolverType> const& info) {
    return storm::solver::toString(info.param);
}

}  // namespace storm::test
