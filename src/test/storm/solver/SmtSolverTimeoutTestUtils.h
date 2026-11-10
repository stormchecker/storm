#pragma once

#include "storm-config.h"
#include "test/storm_gtest.h"

#include <chrono>

#include "storm/expressions/ExpressionManager.h"
#include "storm/expressions/Expressions.h"
#include "storm/solver/SmtSolver.h"

namespace storm::test {

/*!
 * Create the (unsatisfiable) pigeonhole formula for the given number of holes and one more pigeon.
 * Proving its unsatisfiability takes solvers a very long time, which makes it suitable for testing timeouts.
 */
inline storm::expressions::Expression pigeonholeFormula(storm::expressions::ExpressionManager& manager, uint64_t holes) {
    uint64_t const pigeons = holes + 1;
    std::vector<std::vector<storm::expressions::Variable>> inHole(pigeons);
    for (uint64_t p = 0; p < pigeons; ++p) {
        for (uint64_t h = 0; h < holes; ++h) {
            inHole[p].push_back(manager.declareBooleanVariable("p" + std::to_string(p) + "h" + std::to_string(h)));
        }
    }
    storm::expressions::Expression formula = manager.boolean(true);
    // Each pigeon sits in some hole.
    for (uint64_t p = 0; p < pigeons; ++p) {
        storm::expressions::Expression somewhere = manager.boolean(false);
        for (uint64_t h = 0; h < holes; ++h) {
            somewhere = somewhere || inHole[p][h];
        }
        formula = formula && somewhere;
    }
    // No two pigeons share a hole.
    for (uint64_t h = 0; h < holes; ++h) {
        for (uint64_t p1 = 0; p1 < pigeons; ++p1) {
            for (uint64_t p2 = p1 + 1; p2 < pigeons; ++p2) {
                formula = formula && !(inHole[p1][h] && inHole[p2][h]);
            }
        }
    }
    return formula;
}

/*!
 * Checks that the given solver aborts a hard query once the timeout is exceeded and remains usable afterwards.
 */
inline void checkSmtSolverTimeout(storm::solver::SmtSolver& solver, storm::expressions::ExpressionManager& manager) {
    solver.setTimeout(10);
    solver.add(pigeonholeFormula(manager, 12));
    auto const start = std::chrono::steady_clock::now();
    EXPECT_EQ(storm::solver::SmtSolver::CheckResult::Unknown, solver.check());
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));

    // After unsetting the timeout, the solver can be used again.
    solver.unsetTimeout();
    solver.reset();
    storm::expressions::Variable x = manager.declareBooleanVariable("x");
    solver.add(x);
    EXPECT_EQ(storm::solver::SmtSolver::CheckResult::Sat, solver.check());
}

}  // namespace storm::test
