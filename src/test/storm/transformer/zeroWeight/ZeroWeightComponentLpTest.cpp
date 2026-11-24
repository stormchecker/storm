#include "storm-config.h"
#include "test/storm_gtest.h"

#include <cstdint>
#include <type_traits>
#include <vector>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/zeroWeight/ZeroWeightAnalysis.h"
#include "storm/transformer/zeroWeight/ZeroWeightComponentLp.h"
#include "storm/utility/constants.h"

namespace {

template<typename ValueType>
struct EntryModel {
    storm::storage::SparseMatrix<ValueType> matrix;
    std::vector<ValueType> weights;
    storm::storage::BitVector targets;
    storm::storage::BitVector initials;
};

template<typename ValueType>
EntryModel<ValueType> buildEntryModel() {
    auto const one = storm::utility::one<ValueType>();
    auto const zero = storm::utility::zero<ValueType>();
    ValueType const half = one / (one + one);
    ValueType const third = one / (one + one + one);
    ValueType const sixth = half * third;
    storm::storage::SparseMatrixBuilder<ValueType> builder(6, 5, 9, true, true, 5);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, half);
    builder.addNextValue(0, 2, half);
    builder.newRowGroup(1);
    builder.addNextValue(1, 1, half);
    builder.addNextValue(1, 3, sixth);
    builder.addNextValue(1, 4, third);
    builder.addNextValue(2, 2, one);
    builder.newRowGroup(3);
    builder.addNextValue(3, 4, one);
    builder.newRowGroup(4);
    builder.addNextValue(4, 3, one);
    builder.newRowGroup(5);
    builder.addNextValue(5, 4, one);

    EntryModel<ValueType> model{builder.build(), {one, zero, zero, zero, zero, zero}, storm::storage::BitVector(5, false), storm::storage::BitVector(5, false)};
    model.targets.set(3);
    model.targets.set(4);
    model.initials.set(0);
    return model;
}

template<typename ValueType>
class ZeroWeightComponentLpTest : public ::testing::Test {};

using TestedValueTypes = ::testing::Types<double, storm::RationalNumber>;
TYPED_TEST_SUITE(ZeroWeightComponentLpTest, TestedValueTypes, );

TYPED_TEST(ZeroWeightComponentLpTest, ReusesComponentAcrossEntriesAndDirections) {
#if !defined(STORM_HAVE_GLPK) && !defined(STORM_HAVE_GUROBI) && !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    GTEST_SKIP() << "No LP solver available.";
#endif
#if !defined(STORM_HAVE_Z3) && !defined(STORM_HAVE_SOPLEX)
    if constexpr (std::is_same_v<TypeParam, storm::RationalNumber>) {
        GTEST_SKIP() << "No exact LP solver available.";
    }
#endif
    using ValueType = TypeParam;
    auto const model = buildEntryModel<ValueType>();
    auto analysis = storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(model.matrix, model.weights, model.targets, model.initials);
    storm::transformer::ZeroWeightAnalysis<ValueType>::analyzeComponentInterfaces(model.matrix, analysis);
    ASSERT_EQ(1ull, analysis.weakComponents.size());
    storm::Environment env;
    storm::transformer::ZeroWeightComponentLp<ValueType> lp(env, model.matrix, model.weights, analysis.weakComponents.front());
    EXPECT_EQ((std::vector<uint64_t>{1, 2}), lp.getStates());
    EXPECT_EQ((std::vector<uint64_t>{1, 2}), lp.getEntryStates());
    EXPECT_EQ((std::vector<uint64_t>{3, 4}), lp.getBoundaryStates());
    EXPECT_EQ((std::vector<uint64_t>{1, 2, 3}), lp.getActionRows());

    auto const one = storm::utility::one<ValueType>();
    auto const zero = storm::utility::zero<ValueType>();
    ValueType const third = one / (one + one + one);
    auto const towardThree = lp.maximize(1, {one, zero});
    if constexpr (std::is_same_v<ValueType, double>) {
        EXPECT_NEAR(third, towardThree.objectiveValue, 1e-10);
        EXPECT_NEAR(third, towardThree.boundaryProbabilities[0], 1e-10);
        EXPECT_NEAR(one - third, towardThree.boundaryProbabilities[1], 1e-10);
    } else {
        EXPECT_EQ(third, towardThree.objectiveValue);
        EXPECT_EQ((std::vector<ValueType>{third, one - third}), towardThree.boundaryProbabilities);
    }
    EXPECT_EQ((std::vector<ValueType>{one + one, zero, zero}), towardThree.actionFlows);

    auto const awayFromThree = lp.maximize(1, {-one, zero});
    EXPECT_EQ((std::vector<ValueType>{zero, one}), awayFromThree.boundaryProbabilities);
    auto const fromTwo = lp.maximize(2, {one, zero});
    EXPECT_EQ((std::vector<ValueType>{zero, one}), fromTwo.boundaryProbabilities);
    EXPECT_EQ((std::vector<ValueType>{zero, zero, one}), fromTwo.actionFlows);
    auto const repeated = lp.maximize(1, {one, zero});
    EXPECT_EQ(towardThree.boundaryProbabilities, repeated.boundaryProbabilities);
}

TYPED_TEST(ZeroWeightComponentLpTest, RejectsInvalidEntryAndDirection) {
    using ValueType = TypeParam;
    auto const model = buildEntryModel<ValueType>();
    auto analysis = storm::transformer::ZeroWeightAnalysis<ValueType>::analyze(model.matrix, model.weights, model.targets, model.initials);
    storm::transformer::ZeroWeightAnalysis<ValueType>::analyzeComponentInterfaces(model.matrix, analysis);
    storm::Environment env;
    storm::transformer::ZeroWeightComponentLp<ValueType> lp(env, model.matrix, model.weights, analysis.weakComponents.front());
    EXPECT_THROW(lp.maximize(0, {storm::utility::one<ValueType>(), storm::utility::zero<ValueType>()}), storm::exceptions::InvalidArgumentException);
    EXPECT_THROW(lp.maximize(1, {storm::utility::one<ValueType>()}), storm::exceptions::InvalidArgumentException);
}

}  // namespace
