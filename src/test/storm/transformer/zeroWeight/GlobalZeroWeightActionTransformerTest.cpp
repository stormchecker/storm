#include "storm-config.h"
#include "test/storm_gtest.h"

#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/Environment.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/zeroWeight/GlobalZeroWeightActionTransformer.h"
#include "storm/utility/constants.h"
#include "test/storm/transformer/zeroWeight/ZeroWeightTestModels.h"

namespace {

template<typename ValueType>
struct ExitModel {
    storm::storage::SparseMatrix<ValueType> matrix;
    std::vector<ValueType> weights;
    storm::storage::BitVector targets;
    storm::storage::BitVector initials;
};

template<typename ValueType>
ExitModel<ValueType> buildExitModel() {
    auto const one = storm::utility::one<ValueType>();
    auto const zero = storm::utility::zero<ValueType>();
    ValueType const half = one / storm::utility::convertNumber<ValueType>(uint64_t{2});
    ValueType const eighth = one / storm::utility::convertNumber<ValueType>(uint64_t{8});
    storm::storage::SparseMatrixBuilder<ValueType> builder(12, 7, 17, true, true, 7);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, one);
    builder.addNextValue(1, 2, one);
    builder.addNextValue(2, 3, one);
    builder.newRowGroup(3);
    builder.addNextValue(3, 5, half);
    builder.addNextValue(3, 6, half);
    builder.addNextValue(4, 4, eighth);
    builder.addNextValue(4, 5, eighth);
    builder.addNextValue(4, 6, one - eighth - eighth);
    builder.addNextValue(5, 2, one);
    builder.newRowGroup(6);
    builder.addNextValue(6, 5, half);
    builder.addNextValue(6, 6, half);
    builder.addNextValue(7, 3, one);
    builder.newRowGroup(8);
    builder.addNextValue(8, 4, half);
    builder.addNextValue(8, 6, half);
    builder.newRowGroup(9);
    builder.addNextValue(9, 4, one);
    builder.newRowGroup(10);
    builder.addNextValue(10, 5, one);
    builder.newRowGroup(11);
    builder.addNextValue(11, 6, one);

    ExitModel<ValueType> model{builder.build(), std::vector<ValueType>(12, zero), storm::storage::BitVector(7, false), storm::storage::BitVector(7, false)};
    model.weights[0] = one;
    model.weights[1] = one;
    model.weights[2] = one;
    model.targets.set(4);
    model.targets.set(5);
    model.targets.set(6);
    model.initials.set(0);
    return model;
}

template<typename ValueType>
void expectDistribution(storm::storage::SparseMatrix<ValueType> const& matrix, uint64_t row, std::vector<ValueType> const& expected) {
    std::vector<ValueType> actual(expected.size(), storm::utility::zero<ValueType>());
    for (auto const& transition : matrix.getRow(row)) {
        ASSERT_LT(transition.getColumn(), expected.size());
        actual[transition.getColumn()] = transition.getValue();
    }
    for (uint64_t state = 0; state < expected.size(); ++state) {
        if constexpr (std::is_same_v<ValueType, double>) {
            EXPECT_NEAR(expected[state], actual[state], 1e-8);
        } else {
            EXPECT_EQ(expected[state], actual[state]);
        }
    }
}

template<typename ValueType>
class GlobalZeroWeightActionTransformerTest : public ::testing::Test {};

using TestedValueTypes = ::testing::Types<double, storm::RationalNumber>;
TYPED_TEST_SUITE(GlobalZeroWeightActionTransformerTest, TestedValueTypes, );

TYPED_TEST(GlobalZeroWeightActionTransformerTest, DiscoversNonAxisVertexSegmentAndPoint) {
#if !defined(STORM_HAVE_GLPK) && !defined(STORM_HAVE_GUROBI) && !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    GTEST_SKIP() << "No LP solver available.";
#endif
#if !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    if constexpr (std::is_same_v<TypeParam, storm::RationalNumber>) {
        GTEST_SKIP() << "No exact LP solver available.";
    }
#endif
    using ValueType = TypeParam;
    using Transformer = storm::transformer::GlobalZeroWeightActionTransformer<ValueType>;
    auto const model = buildExitModel<ValueType>();
    storm::Environment environment;
    auto const result = Transformer::collapse(environment, model.matrix, model.weights, model.targets, model.initials);
    uint64_t const invalidIndex = std::numeric_limits<uint64_t>::max();
    EXPECT_EQ((std::vector<uint64_t>{0, invalidIndex, invalidIndex, invalidIndex, 1, 2, 3}), result.oldToNewStateMapping);
    EXPECT_EQ((std::vector<uint64_t>{0, 4, 5, 6}), result.newToOldStateMapping);
    EXPECT_EQ(9ull, result.transitionMatrix.getRowCount());
    EXPECT_EQ((std::vector<uint64_t>{0, 6, 7, 8, 9}), result.transitionMatrix.getRowGroupIndices());
    EXPECT_EQ((std::vector<uint64_t>{0, 0, 0, 1, 1, 2, 9, 10, 11}), result.newToOldRowMapping);
    EXPECT_EQ((std::vector<uint64_t>{0, 1, 2}), result.oldToNewRowMapping[0]);
    EXPECT_EQ((std::vector<uint64_t>{3, 4}), result.oldToNewRowMapping[1]);
    EXPECT_EQ((std::vector<uint64_t>{5}), result.oldToNewRowMapping[2]);

    auto const zero = storm::utility::zero<ValueType>();
    auto const one = storm::utility::one<ValueType>();
    ValueType const half = one / storm::utility::convertNumber<ValueType>(uint64_t{2});
    ValueType const eighth = one / storm::utility::convertNumber<ValueType>(uint64_t{8});
    std::vector<std::vector<ValueType>> expectedRows{{zero, zero, half, half}, {zero, eighth, eighth, one - eighth - eighth},
                                                     {zero, half, zero, half}, {zero, zero, half, half},
                                                     {zero, half, zero, half}, {zero, half, zero, half}};
    for (uint64_t row = 0; row < expectedRows.size(); ++row) {
        expectDistribution(result.transitionMatrix, row, expectedRows[row]);
        EXPECT_EQ(one, result.actionWeights[row]);
    }
    for (uint64_t vertex = 0; vertex < 3; ++vertex) {
        EXPECT_EQ((std::vector<typename Transformer::EntryLawSelection>{{1, vertex}}), result.entryLawSelections[vertex]);
    }
    EXPECT_EQ((std::vector<typename Transformer::EntryLawSelection>{{2, 0}}), result.entryLawSelections[3]);
    EXPECT_EQ((std::vector<typename Transformer::EntryLawSelection>{{2, 1}}), result.entryLawSelections[4]);
    EXPECT_EQ((std::vector<typename Transformer::EntryLawSelection>{{3, 0}}), result.entryLawSelections[5]);
    ASSERT_EQ(3ull, result.entryExitLaws.size());
    EXPECT_EQ(1ull, result.entryExitLaws[0].entryState);
    EXPECT_EQ((std::vector<uint64_t>{1, 2, 3}), result.entryExitLaws[0].boundaryStates);
    EXPECT_EQ(3ull, result.entryExitLaws[0].laws.size());
    EXPECT_EQ(2ull, result.entryExitLaws[1].entryState);
    EXPECT_EQ(2ull, result.entryExitLaws[1].laws.size());
    EXPECT_EQ(3ull, result.entryExitLaws[2].entryState);
    EXPECT_EQ(1ull, result.entryExitLaws[2].laws.size());
    storm::storage::BitVector expectedTargets(4, false);
    expectedTargets.set(1);
    expectedTargets.set(2);
    expectedTargets.set(3);
    storm::storage::BitVector expectedInitials(4, false);
    expectedInitials.set(0);
    EXPECT_EQ(expectedTargets, result.targetStates);
    EXPECT_EQ(expectedInitials, result.initialStates);
}

TEST(GlobalZeroWeightActionTransformerTest, ComposesTwoEntriesOfOneComponentAndAnUnchangedBranch) {
#if !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    GTEST_SKIP() << "No exact LP solver available.";
#endif
    using ValueType = storm::RationalNumber;
    using Transformer = storm::transformer::GlobalZeroWeightActionTransformer<ValueType>;
    auto const one = storm::utility::one<ValueType>();
    auto const zero = storm::utility::zero<ValueType>();
    ValueType const quarter = one / storm::utility::convertNumber<ValueType>(uint64_t{4});
    ValueType const half = one / storm::utility::convertNumber<ValueType>(uint64_t{2});
    storm::storage::SparseMatrixBuilder<ValueType> builder(9, 6, 11, true, true, 6);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, half);
    builder.addNextValue(0, 2, quarter);
    builder.addNextValue(0, 3, quarter);
    builder.newRowGroup(1);
    builder.addNextValue(1, 4, one);
    builder.addNextValue(2, 5, one);
    builder.addNextValue(3, 2, one);
    builder.newRowGroup(4);
    builder.addNextValue(4, 4, one);
    builder.addNextValue(5, 5, one);
    builder.newRowGroup(6);
    builder.addNextValue(6, 3, one);
    builder.newRowGroup(7);
    builder.addNextValue(7, 4, one);
    builder.newRowGroup(8);
    builder.addNextValue(8, 5, one);
    auto const matrix = builder.build();
    auto const weights = std::vector<ValueType>{storm::utility::convertNumber<ValueType>(uint64_t{7}), zero, zero, zero, zero, zero, zero, zero, zero};
    storm::storage::BitVector targets(6, false);
    targets.set(3);
    targets.set(4);
    targets.set(5);
    storm::storage::BitVector initials(6, false);
    initials.set(0);
    storm::Environment environment;
    auto const result = Transformer::collapse(environment, matrix, weights, targets, initials);
    EXPECT_EQ((std::vector<uint64_t>{0, 3, 4, 5}), result.newToOldStateMapping);
    EXPECT_EQ((std::vector<uint64_t>{0, 1, 2, 3}), result.oldToNewRowMapping[0]);
    EXPECT_EQ((std::vector<uint64_t>{0, 0, 0, 0, 6, 7, 8}), result.newToOldRowMapping);
    ASSERT_EQ(2ull, result.entryExitLaws.size());
    EXPECT_EQ((std::vector<uint64_t>{2, 3}), result.entryExitLaws[0].boundaryStates);
    EXPECT_EQ(result.entryExitLaws[0].boundaryStates, result.entryExitLaws[1].boundaryStates);
    EXPECT_EQ((std::vector<std::vector<ValueType>>{{zero, one}, {one, zero}}), result.entryExitLaws[0].laws);
    EXPECT_EQ(result.entryExitLaws[0].laws, result.entryExitLaws[1].laws);
    expectDistribution(result.transitionMatrix, 0, {zero, quarter, zero, quarter + half});
    expectDistribution(result.transitionMatrix, 1, {zero, quarter, quarter, half});
    expectDistribution(result.transitionMatrix, 2, {zero, quarter, half, quarter});
    expectDistribution(result.transitionMatrix, 3, {zero, quarter, quarter + half, zero});
    for (uint64_t row = 0; row < 4; ++row) {
        EXPECT_EQ(weights[0], result.actionWeights[row]);
        EXPECT_EQ((std::vector<typename Transformer::EntryLawSelection>{{1, row / 2}, {2, row % 2}}), result.entryLawSelections[row]);
    }
}

TYPED_TEST(GlobalZeroWeightActionTransformerTest, KeepsAnUnreachablePredecessorValid) {
#if !defined(STORM_HAVE_GLPK) && !defined(STORM_HAVE_GUROBI) && !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    GTEST_SKIP() << "No LP solver available.";
#endif
#if !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    if constexpr (std::is_same_v<TypeParam, storm::RationalNumber>) {
        GTEST_SKIP() << "No exact LP solver available.";
    }
#endif
    using ValueType = TypeParam;
    using Transformer = storm::transformer::GlobalZeroWeightActionTransformer<ValueType>;
    auto const one = storm::utility::one<ValueType>();
    auto const zero = storm::utility::zero<ValueType>();
    storm::storage::SparseMatrixBuilder<ValueType> builder(5, 5, 5, true, true, 5);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, one);
    builder.newRowGroup(1);
    builder.addNextValue(1, 2, one);
    builder.newRowGroup(2);
    builder.addNextValue(2, 4, one);
    builder.newRowGroup(3);
    builder.addNextValue(3, 2, one);
    builder.newRowGroup(4);
    builder.addNextValue(4, 4, one);
    auto const matrix = builder.build();
    auto const weights = std::vector<ValueType>{one, zero, zero, zero, zero};
    storm::storage::BitVector targets(5, false);
    targets.set(4);
    storm::storage::BitVector initials(5, false);
    initials.set(0);
    storm::Environment environment;
    auto const result = Transformer::collapse(environment, matrix, weights, targets, initials);
    EXPECT_EQ((std::vector<uint64_t>{0, 3, 4}), result.newToOldStateMapping);
    EXPECT_EQ((std::vector<uint64_t>{0, 3, 4}), result.newToOldRowMapping);
    expectDistribution(result.transitionMatrix, 0, {zero, zero, one});
    expectDistribution(result.transitionMatrix, 1, {zero, zero, one});
    ASSERT_EQ(2ull, result.entryExitLaws.size());
    EXPECT_EQ((std::vector<uint64_t>{2}), result.entryExitLaws[0].boundaryStates);
    EXPECT_EQ((std::vector<typename Transformer::EntryLawSelection>{{2, 0}}), result.entryLawSelections[1]);
}

TYPED_TEST(GlobalZeroWeightActionTransformerTest, KeepsMixedShellAndPositiveReturn) {
#if !defined(STORM_HAVE_GLPK) && !defined(STORM_HAVE_GUROBI) && !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    GTEST_SKIP() << "No LP solver available.";
#endif
#if !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    if constexpr (std::is_same_v<TypeParam, storm::RationalNumber>) {
        GTEST_SKIP() << "No exact LP solver available.";
    }
#endif
    using ValueType = TypeParam;
    auto const model = storm::test::zeroWeight::buildMixedModel<ValueType>();
    storm::Environment environment;
    auto const result =
        storm::transformer::GlobalZeroWeightActionTransformer<ValueType>::collapse(environment, model.matrix, model.weights, model.targets, model.initials);
    EXPECT_EQ((std::vector<uint64_t>{0, 3, 1, 2}), result.newToOldStateMapping);
    EXPECT_EQ((std::vector<uint64_t>{0, 2, 3, 1}), result.oldToNewStateMapping);
    ASSERT_EQ(2ull, result.entryExitLaws.size());
    EXPECT_EQ((std::vector<uint64_t>{1, 2, 3}), result.entryExitLaws[0].boundaryStates);
    ASSERT_EQ(3ull, result.oldToNewRowMapping[2].size());
    auto const zero = storm::utility::zero<ValueType>();
    auto const one = storm::utility::one<ValueType>();
    ValueType const half = one / storm::utility::convertNumber<ValueType>(uint64_t{2});
    ValueType const quarter = half / storm::utility::convertNumber<ValueType>(uint64_t{2});
    std::vector<std::vector<ValueType>> const expectedRows{{zero, half, half, zero}, {zero, one - quarter, zero, quarter}, {zero, one, zero, zero}};
    for (uint64_t index = 0; index < expectedRows.size(); ++index) {
        uint64_t const row = result.oldToNewRowMapping[2][index];
        EXPECT_EQ(storm::utility::convertNumber<ValueType>(uint64_t{3}), result.actionWeights[row]);
        EXPECT_GE(row, result.transitionMatrix.getRowGroupIndices()[2]);
        EXPECT_LT(row, result.transitionMatrix.getRowGroupIndices()[3]);
        expectDistribution(result.transitionMatrix, row, expectedRows[index]);
    }
}

}  // namespace
