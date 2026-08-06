#include "storm-config.h"
#include "test/storm_gtest.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-negative"
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#pragma clang diagnostic ignored "-Wunused-template"
#include <carl/formula/Constraint.h>
#pragma clang diagnostic pop
#include <string>

#include "storm-pars/api/region.h"
#include "storm-pars/modelchecker/region/SparseDtmcParameterLiftingModelChecker.h"
#include "storm-pars/transformer/IntervalEndComponentPreserver.h"
#include "storm-pars/transformer/RobustParameterLifter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/builder.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/solver/IterativeMinMaxLinearEquationSolver.h"
#include "storm/storage/SparseMatrix.h"

class IntervalEndComponentPreserverTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(IntervalEndComponentPreserverTest, Simple) {
    storm::storage::SparseMatrixBuilder<storm::Interval> builder(3, 3);
    //                     0       1       2
    //         ---- group 0/2 ----
    // 0       (       [0, 1]  [0, 1]  0               )       0
    //         ---- group 1/2 ----
    // 1       (       0       0       [1, 1]          )       1
    //         ---- group 2/2 ----
    // 2       (       0       0       0               )       2
    //                 0       1       2
    builder.addNextValue(0, 0, storm::Interval(0, 1));
    builder.addNextValue(0, 1, storm::Interval(0, 1));
    builder.addNextValue(1, 2, storm::Interval(1, 1));
    storm::storage::SparseMatrix<storm::Interval> matrix = builder.build();

    std::vector<storm::Interval> vector = {storm::Interval(0, 0), storm::Interval(1, 1), storm::Interval(0, 0)};

    storm::transformer::IntervalEndComponentPreserver preserver;
    auto newMatrix = preserver.eliminateMECs(matrix, vector);

    // Should be this now
    //                     0       1       2       3
    //         ---- group 0/3 ----
    // 0       (       0       [0, 1]  0       [0, 1]          )       0
    //         ---- group 1/3 ----
    // 1       (       0       0       [1, 1]  0               )       1
    //         ---- group 2/3 ----
    // 2       (       0       0       0       0               )       2
    //         ---- group 3/3 ----
    // 3       (       0       0       0       0               )       3
    //                 0       1       2       3

    ASSERT_EQ(newMatrix->getRowCount(), 4);
    ASSERT_EQ(newMatrix->getColumnCount(), 4);
    ASSERT_EQ(newMatrix->getEntryCount(), 3);

    ASSERT_EQ(newMatrix->getRow(0).getNumberOfEntries(), 2);
    ASSERT_EQ(newMatrix->getRow(0).begin()->getColumn(), 1);
    ASSERT_EQ(newMatrix->getRow(0).begin()->getValue(), storm::Interval(0, 1));
    ASSERT_EQ((newMatrix->getRow(0).begin() + 1)->getColumn(), 3);
    ASSERT_EQ((newMatrix->getRow(0).begin() + 1)->getValue(), storm::Interval(0, 1));

    ASSERT_EQ(newMatrix->getRow(1).getNumberOfEntries(), 1);
    ASSERT_EQ(newMatrix->getRow(1).begin()->getColumn(), 2);
    ASSERT_EQ(newMatrix->getRow(1).begin()->getValue(), storm::Interval(1, 1));

    ASSERT_EQ(newMatrix->getRow(2).getNumberOfEntries(), 0);
    ASSERT_EQ(newMatrix->getRow(3).getNumberOfEntries(), 0);
}
