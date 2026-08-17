#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/solver/MinMaxLinearEquationSolver.h"

TEST(MinMaxMethod, Simple) {
    storm::solver::MinMaxMethodSelection ts = storm::solver::MinMaxMethodSelection::PolicyIteration;
    storm::solver::MinMaxMethod t = storm::solver::MinMaxMethod::PolicyIteration;
    ASSERT_EQ(convert(ts), t);
}
