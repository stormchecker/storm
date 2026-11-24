#include "storm-config.h"
#include "test/storm_gtest.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/zeroWeight/LocalZeroWeightActionEliminator.h"
#include "storm/utility/constants.h"

namespace {

template<typename ValueType>
struct SingleStateEliminationModel {
    storm::storage::SparseMatrix<ValueType> matrix;
    std::vector<ValueType> weights;
    storm::storage::BitVector targets;
    storm::storage::BitVector initials;
};

template<typename ValueType>
SingleStateEliminationModel<ValueType> buildSingleStateEliminationModel() {
    auto const one = storm::utility::one<ValueType>();
    auto const zero = storm::utility::zero<ValueType>();
    ValueType const half = one / (one + one);
    ValueType const quarter = half / (one + one);
    storm::storage::SparseMatrixBuilder<ValueType> builder(5, 4, 8, true, true, 4);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, one - quarter);
    builder.addNextValue(0, 2, quarter);
    builder.newRowGroup(1);
    builder.addNextValue(1, 2, one);
    builder.addNextValue(2, 1, quarter);
    builder.addNextValue(2, 2, quarter);
    builder.addNextValue(2, 3, half);
    builder.newRowGroup(3);
    builder.addNextValue(3, 2, one);
    builder.newRowGroup(4);
    builder.addNextValue(4, 3, one);

    SingleStateEliminationModel<ValueType> model;
    model.matrix = builder.build();
    model.weights = {storm::utility::convertNumber<ValueType>(uint64_t{5}), zero, zero, zero, zero};
    model.targets = storm::storage::BitVector(4, false);
    model.targets.set(2);
    model.targets.set(3);
    model.initials = storm::storage::BitVector(4, false);
    model.initials.set(0);
    return model;
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> buildExpectedSingleStateEliminationMatrix() {
    auto const one = storm::utility::one<ValueType>();
    ValueType const half = one / (one + one);
    storm::storage::SparseMatrixBuilder<ValueType> builder(4, 3, 5, true, true, 3);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, one);
    builder.addNextValue(1, 1, half);
    builder.addNextValue(1, 2, half);
    builder.newRowGroup(2);
    builder.addNextValue(2, 1, one);
    builder.newRowGroup(3);
    builder.addNextValue(3, 2, one);
    return builder.build();
}

template<typename ValueType>
SingleStateEliminationModel<ValueType> buildSplitMultiStateEliminationModel() {
    auto const one = storm::utility::one<ValueType>();
    auto const zero = storm::utility::zero<ValueType>();
    storm::storage::SparseMatrixBuilder<ValueType> builder(5, 4, 5, true, true, 4);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, one);
    builder.newRowGroup(1);
    builder.addNextValue(1, 2, one);
    builder.newRowGroup(2);
    builder.addNextValue(2, 3, one);
    builder.addNextValue(3, 3, one);
    builder.newRowGroup(4);
    builder.addNextValue(4, 3, one);

    SingleStateEliminationModel<ValueType> model;
    model.matrix = builder.build();
    model.weights = {storm::utility::convertNumber<ValueType>(uint64_t{5}), zero, zero, storm::utility::convertNumber<ValueType>(uint64_t{7}), zero};
    model.targets = storm::storage::BitVector(4, false);
    model.targets.set(3);
    model.initials = storm::storage::BitVector(4, false);
    model.initials.set(0);
    return model;
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> buildExpectedSplitMultiStateEliminationMatrix() {
    auto const one = storm::utility::one<ValueType>();
    storm::storage::SparseMatrixBuilder<ValueType> builder(4, 3, 4, true, true, 3);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, one);
    builder.addNextValue(1, 2, one);
    builder.newRowGroup(2);
    builder.addNextValue(2, 1, one);
    builder.newRowGroup(3);
    builder.addNextValue(3, 1, one);
    return builder.build();
}

template<typename ValueType>
class LocalZeroWeightActionEliminatorTest : public ::testing::Test {};

using TestedValueTypes = ::testing::Types<double, storm::RationalNumber>;
TYPED_TEST_SUITE(LocalZeroWeightActionEliminatorTest, TestedValueTypes, );

TYPED_TEST(LocalZeroWeightActionEliminatorTest, EliminatesOneMultiChoiceState) {
    using ValueType = TypeParam;
    using Eliminator = storm::transformer::LocalZeroWeightActionEliminator<ValueType>;
    auto const model = buildSingleStateEliminationModel<ValueType>();
    Eliminator eliminator(model.matrix, model.weights, model.targets, model.initials);
    eliminator.eliminateState(1);
    auto const result = eliminator.build();
    uint64_t const invalidIndex = std::numeric_limits<uint64_t>::max();

    EXPECT_EQ(buildExpectedSingleStateEliminationMatrix<ValueType>(), result.transitionMatrix);
    EXPECT_EQ((std::vector<ValueType>{5, 5, 0, 0}), result.actionWeights);
    EXPECT_EQ((std::vector<uint64_t>{0, invalidIndex, 1, 2}), result.oldToNewStateMapping);
    EXPECT_EQ((std::vector<uint64_t>{0, 2, 3}), result.newToOldStateMapping);
    EXPECT_EQ((std::vector<std::vector<uint64_t>>{{0, 1}, {}, {}, {2}, {3}}), result.oldToNewRowMapping);
    EXPECT_EQ((std::vector<uint64_t>{0, 0, 3, 4}), result.newToOldRowMapping);
    EXPECT_EQ((std::vector<std::vector<typename Eliminator::ChoiceSelection>>{{{1, 1}}, {{1, 2}}, {}, {}}), result.choiceSelections);
    storm::storage::BitVector expectedTargets(3, false);
    expectedTargets.set(1);
    expectedTargets.set(2);
    storm::storage::BitVector expectedInitials(3, false);
    expectedInitials.set(0);
    EXPECT_EQ(expectedTargets, result.targetStates);
    EXPECT_EQ(expectedInitials, result.initialStates);
}

TYPED_TEST(LocalZeroWeightActionEliminatorTest, RejectsAnActionThatCannotLeave) {
    using ValueType = TypeParam;
    auto const one = storm::utility::one<ValueType>();
    auto const zero = storm::utility::zero<ValueType>();
    storm::storage::SparseMatrixBuilder<ValueType> builder(3, 3, 3, true, true, 3);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, one);
    builder.newRowGroup(1);
    builder.addNextValue(1, 1, one);
    builder.newRowGroup(2);
    builder.addNextValue(2, 2, one);
    auto const matrix = builder.build();
    storm::storage::BitVector targets(3, false);
    targets.set(2);
    storm::storage::BitVector initials(3, false);
    initials.set(0);
    storm::transformer::LocalZeroWeightActionEliminator<ValueType> eliminator(matrix, {one, zero, zero}, targets, initials);

    EXPECT_THROW(eliminator.eliminateState(1), storm::exceptions::InvalidArgumentException);
}

TYPED_TEST(LocalZeroWeightActionEliminatorTest, EliminatesSplitMultiStateComponent) {
    using ValueType = TypeParam;
    using Eliminator = storm::transformer::LocalZeroWeightActionEliminator<ValueType>;
    auto const model = buildSplitMultiStateEliminationModel<ValueType>();
    auto const result = Eliminator::eliminate(model.matrix, model.weights, model.targets, model.initials);
    uint64_t const invalidIndex = std::numeric_limits<uint64_t>::max();

    EXPECT_EQ(buildExpectedSplitMultiStateEliminationMatrix<ValueType>(), result.transitionMatrix);
    EXPECT_EQ((std::vector<ValueType>{5, 5, 0, 7}), result.actionWeights);
    EXPECT_EQ((std::vector<uint64_t>{0, invalidIndex, 2, 1}), result.oldToNewStateMapping);
    EXPECT_EQ((std::vector<uint64_t>{0, 3, 2}), result.newToOldStateMapping);
    EXPECT_EQ((std::vector<std::vector<uint64_t>>{{0, 1}, {}, {}, {3}, {2}}), result.oldToNewRowMapping);
    EXPECT_EQ((std::vector<uint64_t>{0, 0, 4, 3}), result.newToOldRowMapping);
    EXPECT_EQ((std::vector<std::vector<typename Eliminator::ChoiceSelection>>{{{1, 1}, {2, 2}}, {{1, 1}, {2, invalidIndex}}, {}, {}}), result.choiceSelections);
    storm::storage::BitVector expectedTargets(3, false);
    expectedTargets.set(1);
    storm::storage::BitVector expectedInitials(3, false);
    expectedInitials.set(0);
    EXPECT_EQ(expectedTargets, result.targetStates);
    EXPECT_EQ(expectedInitials, result.initialStates);
}

}  // namespace
