#include "storm-config.h"
#include "test/storm_gtest.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/zeroWeight/MixedWeightStateSplitter.h"
#include "storm/transformer/zeroWeight/ZeroWeightAnalysis.h"
#include "storm/utility/constants.h"
#include "test/storm/transformer/zeroWeight/ZeroWeightTestModels.h"

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
class ZeroWeightAnalysisTest : public ::testing::Test {};

using TestedValueTypes = ::testing::Types<double, storm::RationalNumber>;
TYPED_TEST_SUITE(ZeroWeightAnalysisTest, TestedValueTypes, );

TYPED_TEST(ZeroWeightAnalysisTest, ClassifiesNonTargetActionsAndStates) {
    using ValueType = TypeParam;
    auto const matrix = buildSelfLoopMatrix<ValueType>({1, 2, 1, 1});
    std::vector<ValueType> const actionWeights = {storm::utility::one<ValueType>(), storm::utility::zero<ValueType>(),
                                                  storm::utility::one<ValueType>() + storm::utility::one<ValueType>(), storm::utility::zero<ValueType>(),
                                                  storm::utility::zero<ValueType>()};
    storm::storage::BitVector targetStates(4, false);
    targetStates.set(3);
    storm::storage::BitVector initialStates(4, false);

    auto const result = storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(matrix, actionWeights, targetStates, initialStates);

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

TYPED_TEST(ZeroWeightAnalysisTest, RejectsMismatchingInputSizes) {
    using ValueType = TypeParam;
    auto const matrix = buildSelfLoopMatrix<ValueType>({1, 1});
    std::vector<ValueType> const validActionWeights(2, storm::utility::one<ValueType>());
    storm::storage::BitVector const validTargetStates(2, false);
    storm::storage::BitVector const validInitialStates(2, false);

    EXPECT_THROW(storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(matrix, std::vector<ValueType>(1, storm::utility::one<ValueType>()),
                                                                            validTargetStates, validInitialStates),
                 storm::exceptions::InvalidArgumentException);
    EXPECT_THROW(
        storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(matrix, validActionWeights, storm::storage::BitVector(1, false), validInitialStates),
        storm::exceptions::InvalidArgumentException);
    EXPECT_THROW(storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(matrix, validActionWeights, validTargetStates, storm::storage::BitVector(1, false)),
                 storm::exceptions::InvalidArgumentException);
}

TYPED_TEST(ZeroWeightAnalysisTest, FindsWeakComponents) {
    using ValueType = TypeParam;
    auto const model = storm::test::zeroWeight::buildWeakComponentModel<ValueType>();

    auto const result = storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(model.matrix, model.weights, model.targets, model.initials);

    ASSERT_EQ(2ull, result.weakComponents.size());
    EXPECT_EQ((std::vector<uint64_t>{0, 1}), result.weakComponents[0].states);
    EXPECT_EQ((std::vector<uint64_t>{3, 4}), result.weakComponents[1].states);

    uint64_t const invalidComponent = std::numeric_limits<uint64_t>::max();
    EXPECT_EQ((std::vector<uint64_t>{0, 0, invalidComponent, 1, 1, invalidComponent, invalidComponent}), result.stateToWeakComponent);
}

TYPED_TEST(ZeroWeightAnalysisTest, FindsNoComponentsWithoutZeroWeights) {
    using ValueType = TypeParam;
    auto const matrix = buildSelfLoopMatrix<ValueType>({1, 1});
    std::vector<ValueType> const actionWeights(2, storm::utility::one<ValueType>());

    auto const result = storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(matrix, actionWeights, storm::storage::BitVector(2, false),
                                                                                   storm::storage::BitVector(2, false));

    EXPECT_TRUE(result.weakComponents.empty());
    EXPECT_EQ((std::vector<uint64_t>(2, std::numeric_limits<uint64_t>::max())), result.stateToWeakComponent);
}

TYPED_TEST(ZeroWeightAnalysisTest, RejectsNegativeWeights) {
    using ValueType = TypeParam;
    auto const matrix = buildSelfLoopMatrix<ValueType>({1});
    std::vector<ValueType> const actionWeights = {-storm::utility::one<ValueType>()};

    EXPECT_THROW(storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(matrix, actionWeights, storm::storage::BitVector(1, false),
                                                                            storm::storage::BitVector(1, false)),
                 storm::exceptions::InvalidArgumentException);
}

TYPED_TEST(ZeroWeightAnalysisTest, CollectsInterfacesAfterMixedStateSplitting) {
    using ValueType = TypeParam;
    auto const model = storm::test::zeroWeight::buildMixedModel<ValueType>();
    auto unsplitAnalysis = storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(model.matrix, model.weights, model.targets, model.initials);
    EXPECT_THROW(storm::transformer::ZeroWeightAnalysis<ValueType>::analyzeComponentInterfaces(model.matrix, unsplitAnalysis),
                 storm::exceptions::InvalidArgumentException);

    auto result = storm::transformer::MixedWeightStateSplitter<ValueType>::split(model.matrix, model.weights, model.targets, model.initials);
    storm::transformer::ZeroWeightAnalysis<ValueType>::analyzeComponentInterfaces(result.transitionMatrix, result.analysis);

    ASSERT_EQ(1ull, result.analysis.weakComponents.size());
    auto const& component = result.analysis.weakComponents[0];
    EXPECT_EQ((std::vector<uint64_t>{1, 2}), component.states);
    EXPECT_EQ((std::vector<uint64_t>{1, 2}), component.entryStates);
    EXPECT_EQ((std::vector<uint64_t>{0, 1, 8}), component.positivePredecessorRows);
    EXPECT_EQ((std::vector<uint64_t>{3, 4, 5}), component.boundaryStates);
}

TYPED_TEST(ZeroWeightAnalysisTest, SeparatesComponentsAndDeduplicatesInterfaces) {
    using ValueType = TypeParam;
    auto const model = storm::test::zeroWeight::buildComponentInterfaceModel<ValueType>();
    auto analysis = storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(model.matrix, model.weights, model.targets, model.initials);
    storm::transformer::ZeroWeightAnalysis<ValueType>::analyzeComponentInterfaces(model.matrix, analysis);

    ASSERT_EQ(2ull, analysis.weakComponents.size());
    EXPECT_EQ((std::vector<uint64_t>{1, 2}), analysis.weakComponents[0].states);
    EXPECT_EQ((std::vector<uint64_t>{1, 2}), analysis.weakComponents[0].entryStates);
    EXPECT_EQ((std::vector<uint64_t>{0, 1, 7}), analysis.weakComponents[0].positivePredecessorRows);
    EXPECT_EQ((std::vector<uint64_t>{5}), analysis.weakComponents[0].boundaryStates);
    EXPECT_EQ((std::vector<uint64_t>{3, 4}), analysis.weakComponents[1].states);
    EXPECT_EQ((std::vector<uint64_t>{3, 4}), analysis.weakComponents[1].entryStates);
    EXPECT_EQ((std::vector<uint64_t>{0, 7}), analysis.weakComponents[1].positivePredecessorRows);
    EXPECT_EQ((std::vector<uint64_t>{5, 6}), analysis.weakComponents[1].boundaryStates);
}

TYPED_TEST(ZeroWeightAnalysisTest, IgnoresUnreachableZeroWeightStatesAndChecksInitialStates) {
    using ValueType = TypeParam;
    auto const one = storm::utility::one<ValueType>();
    storm::storage::SparseMatrixBuilder<ValueType> builder(5, 5, 5, true, true, 5);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, one);
    builder.newRowGroup(1);
    builder.addNextValue(1, 4, one);
    builder.newRowGroup(2);
    builder.addNextValue(2, 2, one);
    builder.newRowGroup(3);
    builder.addNextValue(3, 1, one);
    builder.newRowGroup(4);
    builder.addNextValue(4, 4, one);
    auto const matrix = builder.build();
    std::vector<ValueType> const actionWeights = {one, storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>(),
                                                  storm::utility::zero<ValueType>()};
    storm::storage::BitVector targetStates(5, false);
    targetStates.set(4);
    storm::storage::BitVector initialStates(5, false);
    initialStates.set(0);

    auto const result = storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(matrix, actionWeights, targetStates, initialStates);

    ASSERT_EQ(1ull, result.weakComponents.size());
    EXPECT_EQ((std::vector<uint64_t>{1}), result.weakComponents[0].states);
    uint64_t const invalidComponent = std::numeric_limits<uint64_t>::max();
    EXPECT_EQ((std::vector<uint64_t>{invalidComponent, 0, invalidComponent, invalidComponent, invalidComponent}), result.stateToWeakComponent);

    initialStates.set(2);
    EXPECT_THROW(storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(matrix, actionWeights, targetStates, initialStates),
                 storm::exceptions::InvalidArgumentException);
}

TYPED_TEST(ZeroWeightAnalysisTest, ChecksLocalRatherThanGlobalProperness) {
    using ValueType = TypeParam;
    auto const one = storm::utility::one<ValueType>();
    auto const zero = storm::utility::zero<ValueType>();
    storm::storage::SparseMatrixBuilder<ValueType> builder(5, 4, 5, true, true, 4);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, one);
    builder.newRowGroup(1);
    builder.addNextValue(1, 1, one);
    builder.addNextValue(2, 2, one);
    builder.newRowGroup(3);
    builder.addNextValue(3, 2, one);
    builder.newRowGroup(4);
    builder.addNextValue(4, 3, one);
    auto const matrix = builder.build();
    storm::storage::BitVector targetStates(4, false);
    targetStates.set(3);
    storm::storage::BitVector initialStates(4, false);
    initialStates.set(0);
    auto properResult = storm::transformer::MixedWeightStateSplitter<ValueType>::split(matrix, {one, one, zero, one, zero}, targetStates, initialStates);
    storm::transformer::ZeroWeightAnalysis<ValueType>::analyzeComponentInterfaces(properResult.transitionMatrix, properResult.analysis);

    auto improperAnalysis = storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(matrix, {one, zero, zero, one, zero}, targetStates, initialStates);

    EXPECT_THROW(storm::transformer::ZeroWeightAnalysis<ValueType>::analyzeComponentInterfaces(matrix, improperAnalysis),
                 storm::exceptions::InvalidArgumentException);
}

}  // namespace
