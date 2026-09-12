#include "storm-config.h"
#include "test/storm_gtest.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/zeroWeight/ZeroWeightActionAnalysis.h"
#include "storm/utility/constants.h"

namespace {

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> buildSelfLoopMatrix(std::vector<uint64_t> const& choicesPerState) {
    uint64_t numberOfChoices = 0;
    for (auto const choices : choicesPerState) {
        numberOfChoices += choices;
    }

    storm::storage::SparseMatrixBuilder<ValueType> builder(numberOfChoices, choicesPerState.size(), numberOfChoices, true, true, choicesPerState.size());
    uint64_t row = 0;
    for (uint64_t state = 0; state < choicesPerState.size(); ++state) {
        builder.newRowGroup(row);
        for (uint64_t choice = 0; choice < choicesPerState[state]; ++choice, ++row) {
            builder.addNextValue(row, state, storm::utility::one<ValueType>());
        }
    }
    return builder.build();
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> buildWeakComponentMatrix() {
    storm::storage::SparseMatrixBuilder<ValueType> builder(9, 7, 9, true, true, 7);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, storm::utility::one<ValueType>());
    builder.newRowGroup(1);
    builder.addNextValue(1, 1, storm::utility::one<ValueType>());
    builder.newRowGroup(2);
    builder.addNextValue(2, 0, storm::utility::one<ValueType>());
    builder.newRowGroup(3);
    builder.addNextValue(3, 4, storm::utility::one<ValueType>());
    builder.addNextValue(4, 0, storm::utility::one<ValueType>());
    builder.newRowGroup(5);
    builder.addNextValue(5, 3, storm::utility::one<ValueType>());
    builder.addNextValue(6, 5, storm::utility::one<ValueType>());
    builder.newRowGroup(7);
    builder.addNextValue(7, 3, storm::utility::one<ValueType>());
    builder.newRowGroup(8);
    builder.addNextValue(8, 0, storm::utility::one<ValueType>());
    return builder.build();
}

template<typename ValueType>
class ZeroWeightActionAnalysisTest : public ::testing::Test {};

using TestedValueTypes = ::testing::Types<double, storm::RationalNumber>;
TYPED_TEST_SUITE(ZeroWeightActionAnalysisTest, TestedValueTypes, );

TYPED_TEST(ZeroWeightActionAnalysisTest, ClassifiesNonTargetActionsAndStates) {
    using ValueType = TypeParam;
    auto const matrix = buildSelfLoopMatrix<ValueType>({1, 2, 1, 1});
    std::vector<ValueType> const actionWeights = {storm::utility::one<ValueType>(), storm::utility::zero<ValueType>(),
                                                  storm::utility::one<ValueType>() + storm::utility::one<ValueType>(), storm::utility::zero<ValueType>(),
                                                  storm::utility::zero<ValueType>()};
    storm::storage::BitVector targetStates(4, false);
    targetStates.set(3);

    auto const result = storm::transformer::ZeroWeightActionAnalysis<ValueType>::analyze(matrix, actionWeights, targetStates);

    storm::storage::BitVector expectedZeroWeightChoices(5, false);
    expectedZeroWeightChoices.set(1);
    expectedZeroWeightChoices.set(3);
    EXPECT_EQ(expectedZeroWeightChoices, result.zeroWeightChoices);

    storm::storage::BitVector expectedStatesWithZeroWeightChoices(4, false);
    expectedStatesWithZeroWeightChoices.set(1);
    expectedStatesWithZeroWeightChoices.set(2);
    EXPECT_EQ(expectedStatesWithZeroWeightChoices, result.statesWithZeroWeightChoices);

    storm::storage::BitVector expectedPureZeroWeightStates(4, false);
    expectedPureZeroWeightStates.set(2);
    EXPECT_EQ(expectedPureZeroWeightStates, result.pureZeroWeightStates);

    storm::storage::BitVector expectedMixedZeroWeightStates(4, false);
    expectedMixedZeroWeightStates.set(1);
    EXPECT_EQ(expectedMixedZeroWeightStates, result.mixedZeroWeightStates);

    storm::storage::BitVector expectedPositiveOnlyStates(4, false);
    expectedPositiveOnlyStates.set(0);
    EXPECT_EQ(expectedPositiveOnlyStates, result.positiveOnlyStates);
}

TYPED_TEST(ZeroWeightActionAnalysisTest, RejectsMismatchingInputSizes) {
    using ValueType = TypeParam;
    auto const matrix = buildSelfLoopMatrix<ValueType>({1, 1});
    std::vector<ValueType> const validActionWeights(2, storm::utility::one<ValueType>());
    storm::storage::BitVector const validTargetStates(2, false);

    EXPECT_THROW(storm::transformer::ZeroWeightActionAnalysis<ValueType>::analyze(matrix, std::vector<ValueType>(1, storm::utility::one<ValueType>()),
                                                                                  validTargetStates),
                 storm::exceptions::InvalidArgumentException);
    EXPECT_THROW(storm::transformer::ZeroWeightActionAnalysis<ValueType>::analyze(matrix, validActionWeights, storm::storage::BitVector(1, false)),
                 storm::exceptions::InvalidArgumentException);
}

TYPED_TEST(ZeroWeightActionAnalysisTest, FindsWeakComponents) {
    using ValueType = TypeParam;
    auto const matrix = buildWeakComponentMatrix<ValueType>();
    std::vector<ValueType> const actionWeights = {storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>(), storm::utility::one<ValueType>(),
                                                  storm::utility::zero<ValueType>(), storm::utility::one<ValueType>(),  storm::utility::zero<ValueType>(),
                                                  storm::utility::zero<ValueType>(), storm::utility::one<ValueType>(),  storm::utility::zero<ValueType>()};
    storm::storage::BitVector targetStates(7, false);
    targetStates.set(6);

    auto const result = storm::transformer::ZeroWeightActionAnalysis<ValueType>::analyze(matrix, actionWeights, targetStates);

    ASSERT_EQ(2ull, result.weakComponents.size());
    EXPECT_EQ((std::vector<uint64_t>{0, 1}), result.weakComponents[0]);
    EXPECT_EQ((std::vector<uint64_t>{3, 4}), result.weakComponents[1]);

    uint64_t const invalidComponent = std::numeric_limits<uint64_t>::max();
    EXPECT_EQ((std::vector<uint64_t>{0, 0, invalidComponent, 1, 1, invalidComponent, invalidComponent}), result.stateToWeakComponent);
}

TYPED_TEST(ZeroWeightActionAnalysisTest, FindsNoComponentsWithoutZeroWeights) {
    using ValueType = TypeParam;
    auto const matrix = buildSelfLoopMatrix<ValueType>({1, 1});
    std::vector<ValueType> const actionWeights(2, storm::utility::one<ValueType>());

    auto const result = storm::transformer::ZeroWeightActionAnalysis<ValueType>::analyze(matrix, actionWeights, storm::storage::BitVector(2, false));

    EXPECT_TRUE(result.weakComponents.empty());
    EXPECT_EQ((std::vector<uint64_t>(2, std::numeric_limits<uint64_t>::max())), result.stateToWeakComponent);
}

TYPED_TEST(ZeroWeightActionAnalysisTest, RejectsNegativeWeights) {
    using ValueType = TypeParam;
    auto const matrix = buildSelfLoopMatrix<ValueType>({1});
    std::vector<ValueType> const actionWeights = {-storm::utility::one<ValueType>()};

    EXPECT_THROW(storm::transformer::ZeroWeightActionAnalysis<ValueType>::analyze(matrix, actionWeights, storm::storage::BitVector(1, false)),
                 storm::exceptions::InvalidArgumentException);
}

}  // namespace
