#include "storm-config.h"
#include "test/storm_gtest.h"

#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/zeroWeight/MixedWeightStateSplitter.h"
#include "storm/utility/constants.h"

namespace {

template<typename ValueType>
struct SplitterModel {
    storm::storage::SparseMatrix<ValueType> matrix;
    std::vector<ValueType> weights;
    storm::storage::BitVector targets;
    storm::storage::BitVector initials;
};

template<typename ValueType>
SplitterModel<ValueType> buildMixedModel() {
    auto const one = storm::utility::one<ValueType>();
    ValueType const half = one / (one + one);
    storm::storage::SparseMatrixBuilder<ValueType> builder(9, 4, 12, true, true, 4);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, half);
    builder.addNextValue(0, 3, half);
    builder.addNextValue(1, 2, one);
    builder.newRowGroup(2);
    builder.addNextValue(2, 1, half);
    builder.addNextValue(2, 2, half);
    builder.addNextValue(3, 2, half);
    builder.addNextValue(3, 3, half);
    builder.addNextValue(4, 3, one);
    builder.newRowGroup(5);
    builder.addNextValue(5, 1, one);
    builder.addNextValue(6, 3, one);
    builder.newRowGroup(7);
    builder.addNextValue(7, 3, one);
    builder.addNextValue(8, 3, one);

    SplitterModel<ValueType> model;
    model.matrix = builder.build();
    for (uint64_t weight : std::vector<uint64_t>{1, 5, 3, 0, 7, 0, 2, 0, 4}) {
        model.weights.push_back(storm::utility::convertNumber<ValueType>(weight));
    }
    model.targets = storm::storage::BitVector(4, false);
    model.targets.set(3);
    model.initials = storm::storage::BitVector(4, false);
    model.initials.set(0);
    return model;
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> buildExpectedSplitMatrix() {
    auto const one = storm::utility::one<ValueType>();
    ValueType const half = one / (one + one);
    storm::storage::SparseMatrixBuilder<ValueType> builder(11, 6, 14, true, true, 6);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, half);
    builder.addNextValue(0, 3, half);
    builder.addNextValue(1, 2, one);
    builder.newRowGroup(2);
    builder.addNextValue(2, 2, half);
    builder.addNextValue(2, 3, half);
    builder.addNextValue(3, 4, one);
    builder.newRowGroup(4);
    builder.addNextValue(4, 1, one);
    builder.addNextValue(5, 5, one);
    builder.newRowGroup(6);
    builder.addNextValue(6, 3, one);
    builder.addNextValue(7, 3, one);
    builder.newRowGroup(8);
    builder.addNextValue(8, 1, half);
    builder.addNextValue(8, 2, half);
    builder.addNextValue(9, 3, one);
    builder.newRowGroup(10);
    builder.addNextValue(10, 3, one);
    return builder.build();
}

template<typename ValueType>
class MixedWeightStateSplitterTest : public ::testing::Test {};

using TestedValueTypes = ::testing::Types<double, storm::RationalNumber>;
TYPED_TEST_SUITE(MixedWeightStateSplitterTest, TestedValueTypes, );

TYPED_TEST(MixedWeightStateSplitterTest, SplitsMixedStatesAndUpdatesAnalysis) {
    using ValueType = TypeParam;
    auto const model = buildMixedModel<ValueType>();
    auto const result = storm::transformer::MixedWeightStateSplitter<ValueType>::split(model.matrix, model.weights, model.targets, model.initials);
    uint64_t const invalidIndex = std::numeric_limits<uint64_t>::max();

    EXPECT_EQ(buildExpectedSplitMatrix<ValueType>(), result.transitionMatrix);
    EXPECT_EQ((std::vector<uint64_t>{invalidIndex, 4, 5, invalidIndex}), result.positiveShells);
    EXPECT_EQ((std::vector<uint64_t>{0, 1, 2, 3, 1, 2}), result.newToOldStateMapping);
    EXPECT_EQ((std::vector<uint64_t>{0, 1, 8, 2, 9, 4, 10, 6, 7}), result.oldToNewRowMapping);
    EXPECT_EQ((std::vector<uint64_t>{0, 1, 3, invalidIndex, 5, invalidIndex, 7, 8, 2, 4, 6}), result.newToOldRowMapping);
    EXPECT_EQ((std::vector<ValueType>{1, 5, 0, 0, 0, 0, 0, 4, 3, 7, 2}), result.actionWeights);

    auto expectedTargets = model.targets;
    expectedTargets.resize(6, false);
    auto expectedInitials = model.initials;
    expectedInitials.resize(6, false);
    EXPECT_EQ(expectedTargets, result.targetStates);
    EXPECT_EQ(expectedInitials, result.initialStates);
    auto const expected = storm::transformer::ZeroWeightActionAnalysis<ValueType>::analyze(result.transitionMatrix, result.actionWeights, result.targetStates);

    EXPECT_EQ(expected.zeroWeightChoices, result.analysis.zeroWeightChoices);
    EXPECT_EQ(expected.statesWithZeroWeightChoices, result.analysis.statesWithZeroWeightChoices);
    EXPECT_EQ(expected.pureZeroWeightStates, result.analysis.pureZeroWeightStates);
    EXPECT_EQ(expected.mixedZeroWeightStates, result.analysis.mixedZeroWeightStates);
    EXPECT_EQ(expected.positiveOnlyStates, result.analysis.positiveOnlyStates);
    EXPECT_EQ(expected.weakComponents, result.analysis.weakComponents);
    EXPECT_EQ(expected.stateToWeakComponent, result.analysis.stateToWeakComponent);
}

TYPED_TEST(MixedWeightStateSplitterTest, LeavesPositiveOnlyModelsUnchanged) {
    using ValueType = TypeParam;
    auto model = buildMixedModel<ValueType>();
    model.weights.assign(9, storm::utility::one<ValueType>());
    auto const result = storm::transformer::MixedWeightStateSplitter<ValueType>::split(model.matrix, model.weights, model.targets, model.initials);
    std::vector<uint64_t> identityRows(9);
    std::iota(identityRows.begin(), identityRows.end(), uint64_t{0});

    EXPECT_EQ(model.matrix, result.transitionMatrix);
    EXPECT_EQ(model.weights, result.actionWeights);
    EXPECT_EQ(model.targets, result.targetStates);
    EXPECT_EQ(model.initials, result.initialStates);
    EXPECT_EQ(identityRows, result.oldToNewRowMapping);
    EXPECT_EQ(identityRows, result.newToOldRowMapping);
    EXPECT_EQ((std::vector<uint64_t>{0, 1, 2, 3}), result.newToOldStateMapping);
    EXPECT_EQ((std::vector<uint64_t>(4, std::numeric_limits<uint64_t>::max())), result.positiveShells);
}

TYPED_TEST(MixedWeightStateSplitterTest, RequiresPositiveWeightNonTargetInitialStates) {
    using ValueType = TypeParam;
    auto model = buildMixedModel<ValueType>();
    model.initials.set(1);
    EXPECT_THROW(storm::transformer::MixedWeightStateSplitter<ValueType>::split(model.matrix, model.weights, model.targets, model.initials),
                 storm::exceptions::InvalidArgumentException);

    model.initials.set(1, false);
    model.initials.set(2);
    model.weights[6] = storm::utility::zero<ValueType>();
    EXPECT_THROW(storm::transformer::MixedWeightStateSplitter<ValueType>::split(model.matrix, model.weights, model.targets, model.initials),
                 storm::exceptions::InvalidArgumentException);

    model.initials.set(2, false);
    model.initials.set(3);
    auto const result = storm::transformer::MixedWeightStateSplitter<ValueType>::split(model.matrix, model.weights, model.targets, model.initials);

    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), result.positiveShells[3]);
    EXPECT_TRUE(result.initialStates.get(3));
}

}  // namespace
