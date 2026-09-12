#include "storm-config.h"
#include "test/storm_gtest.h"

#include <cstdint>
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

TYPED_TEST(ZeroWeightActionAnalysisTest, RejectsNegativeWeights) {
    using ValueType = TypeParam;
    auto const matrix = buildSelfLoopMatrix<ValueType>({1});
    std::vector<ValueType> const actionWeights = {-storm::utility::one<ValueType>()};

    EXPECT_THROW(storm::transformer::ZeroWeightActionAnalysis<ValueType>::analyze(matrix, actionWeights, storm::storage::BitVector(1, false)),
                 storm::exceptions::InvalidArgumentException);
}

}  // namespace
