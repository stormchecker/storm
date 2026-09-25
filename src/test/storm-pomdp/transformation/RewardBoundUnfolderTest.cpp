#include "storm-config.h"
#include "test/storm_gtest.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <boost/optional.hpp>

#include "storm-pomdp/transformer/RewardBoundUnfolder.h"
#include "storm-pomdp/transformer/ToStateBasedObservationTransformer.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/api/storm.h"
#include "storm/logic/AtomicLabelFormula.h"
#include "storm/logic/BooleanLiteralFormula.h"
#include "storm/logic/BoundedUntilFormula.h"
#include "storm/logic/ProbabilityOperatorFormula.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/ExtendedNumber.h"

namespace {

using ValueType = storm::RationalNumber;
using ResultValueType = storm::utility::ExtendedValueType<ValueType>;
using DtmcType = storm::models::sparse::Dtmc<ValueType>;
using PomdpType = storm::models::sparse::Pomdp<ValueType>;

std::shared_ptr<storm::logic::Formula const> makeRewardBoundedFormula(std::optional<int64_t> lowerBound, std::optional<int64_t> upperBound) {
    static auto const expressionManager = std::make_shared<storm::expressions::ExpressionManager>();
    std::optional<storm::logic::TimeBound> lowerTimeBound;
    std::optional<storm::logic::TimeBound> upperTimeBound;
    if (lowerBound) {
        lowerTimeBound.emplace(false, expressionManager->integer(*lowerBound));
    }
    if (upperBound) {
        upperTimeBound.emplace(false, expressionManager->integer(*upperBound));
    }

    auto pathFormula = std::make_shared<storm::logic::BoundedUntilFormula>(std::make_shared<storm::logic::BooleanLiteralFormula>(true),
                                                                           std::make_shared<storm::logic::AtomicLabelFormula>("goal"), lowerTimeBound,
                                                                           upperTimeBound, storm::logic::TimeBoundReference(boost::optional<std::string>{"r"}));
    return std::make_shared<storm::logic::ProbabilityOperatorFormula>(
        std::move(pathFormula),
        storm::logic::OperatorInformation(boost::optional<storm::solver::OptimizationDirection>{storm::solver::OptimizationDirection::Maximize}));
}

storm::models::sparse::StateLabeling makeLabeling() {
    storm::models::sparse::StateLabeling labeling(3);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    labeling.addLabel("goal");
    labeling.addLabelToState("goal", 1);
    return labeling;
}

storm::models::sparse::StandardRewardModel<ValueType> makeRewardModel() {
    std::optional<std::vector<ValueType>> stateRewards{
        std::vector<ValueType>{storm::utility::one<ValueType>(), storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>()}};
    return storm::models::sparse::StandardRewardModel<ValueType>(std::move(stateRewards));
}

std::shared_ptr<DtmcType> buildDtmc() {
    storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder(3, 3, 4, false, false);
    auto const half = storm::utility::convertNumber<ValueType>(std::string("1/2"));
    matrixBuilder.addNextValue(0, 1, half);
    matrixBuilder.addNextValue(0, 2, half);
    matrixBuilder.addNextValue(1, 1, storm::utility::one<ValueType>());
    matrixBuilder.addNextValue(2, 2, storm::utility::one<ValueType>());

    storm::storage::sparse::ModelComponents<ValueType> components(matrixBuilder.build(), makeLabeling());
    components.rewardModels.emplace("r", makeRewardModel());
    return std::make_shared<DtmcType>(std::move(components));
}

std::shared_ptr<PomdpType> buildPomdp() {
    storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder(0, 0, 0, false, true);
    auto const half = storm::utility::convertNumber<ValueType>(std::string("1/2"));
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 1, half);
    matrixBuilder.addNextValue(0, 2, half);
    matrixBuilder.newRowGroup(1);
    matrixBuilder.addNextValue(1, 1, storm::utility::one<ValueType>());
    matrixBuilder.newRowGroup(2);
    matrixBuilder.addNextValue(2, 2, storm::utility::one<ValueType>());

    storm::storage::sparse::ModelComponents<ValueType> components(matrixBuilder.build(), makeLabeling());
    components.rewardModels.emplace("r", makeRewardModel());
    components.observabilityClasses = std::vector<uint32_t>{0, 1, 2};
    return std::make_shared<PomdpType>(std::move(components), true);
}

ResultValueType getInitialValue(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model,
                                std::shared_ptr<storm::logic::Formula const> const& formula) {
    auto result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<ValueType>(formula, true));
    EXPECT_TRUE(result->isExplicitQuantitativeCheckResult());
    return result->asExplicitQuantitativeCheckResult<ValueType>()[*model->getInitialStates().begin()];
}

}  // namespace

TEST(RewardBoundUnfolder, FullyUnfoldedDtmcPreservesRewardBoundedProbability) {
    auto const dtmc = buildDtmc();
    auto const formula = makeRewardBoundedFormula(std::nullopt, 1);
    auto const expectedValue = getInitialValue(dtmc, formula);

    auto const result = storm::pomdp::transformer::RewardBoundUnfolder<ValueType>::transform(*dtmc, *formula);

    EXPECT_EQ(storm::models::ModelType::Dtmc, result.model->getType());
    EXPECT_TRUE(result.formula->asProbabilityOperatorFormula().getSubformula().isUntilFormula());
    EXPECT_EQ(expectedValue, getInitialValue(result.model, result.formula));
}

TEST(RewardBoundUnfolder, LevelAbstractionPreservesPomdpSpecificComponents) {
    auto const pomdp = buildPomdp();
    auto const formula = makeRewardBoundedFormula(std::nullopt, 1);

    storm::pomdp::transformer::RewardBoundUnfolder<ValueType>::UnfoldingOptions options;
    options.levelWidths = {2};
    options.preservedRewardModels = {"r"};
    auto const result = storm::pomdp::transformer::RewardBoundUnfolder<ValueType>::transform(*pomdp, *formula, options);
    auto const unfoldedPomdp = result.model->as<PomdpType>();

    ASSERT_NE(nullptr, unfoldedPomdp);
    EXPECT_TRUE(unfoldedPomdp->isCanonic());
    EXPECT_EQ(pomdp->getNrObservations(), unfoldedPomdp->getNrObservations());
    EXPECT_TRUE(unfoldedPomdp->hasRewardModel("r"));
    EXPECT_TRUE(unfoldedPomdp->hasRewardModel("dim0_levelReward"));
    EXPECT_TRUE(result.formula->asProbabilityOperatorFormula().getSubformula().isBoundedUntilFormula());
}

TEST(RewardBoundUnfolder, TrivialLowerBoundCanBeFollowedByRewardAwareObservationTransformation) {
    auto const pomdp = buildPomdp();
    auto const formula = makeRewardBoundedFormula(0, std::nullopt);
    auto const& inputBoundedUntilFormula = formula->asProbabilityOperatorFormula().getSubformula().asBoundedUntilFormula();
    ASSERT_TRUE(inputBoundedUntilFormula.hasLowerBound());
    EXPECT_FALSE(inputBoundedUntilFormula.isLowerBoundStrict());

    storm::pomdp::transformer::RewardBoundUnfolder<ValueType>::UnfoldingOptions options;
    options.levelWidths = {1};
    auto const unfoldingResult = storm::pomdp::transformer::RewardBoundUnfolder<ValueType>::transform(*pomdp, *formula, options);
    auto const unfoldedPomdp = unfoldingResult.model->as<PomdpType>();
    std::set<std::string> levelRewardModels;
    unfoldingResult.formula->gatherReferencedRewardModels(levelRewardModels);

    ASSERT_NE(nullptr, unfoldedPomdp);
    EXPECT_TRUE(unfoldingResult.formula->asProbabilityOperatorFormula().getSubformula().isUntilFormula());
    EXPECT_TRUE(levelRewardModels.empty());

    auto const rewardAwarePomdp =
        storm::pomdp::transformer::ToStateBasedObservationTransformer<ValueType>::transformRewardAware(*unfoldedPomdp, levelRewardModels);

    EXPECT_TRUE(rewardAwarePomdp->isCanonic());
    EXPECT_EQ(5u, rewardAwarePomdp->getNrObservations());
    EXPECT_EQ(unfoldedPomdp->getNumberOfStates(), rewardAwarePomdp->getNumberOfStates());
}

TEST(RewardBoundUnfolder, LevelAbstractionTurnsNonStrictLowerBoundIntoStrictBound) {
    auto const dtmc = buildDtmc();
    auto const formula = makeRewardBoundedFormula(1, std::nullopt);
    auto const& inputBoundedUntilFormula = formula->asProbabilityOperatorFormula().getSubformula().asBoundedUntilFormula();
    ASSERT_TRUE(inputBoundedUntilFormula.hasLowerBound());
    EXPECT_FALSE(inputBoundedUntilFormula.isLowerBoundStrict());
    auto const expectedValue = getInitialValue(dtmc, formula);

    storm::pomdp::transformer::RewardBoundUnfolder<ValueType>::UnfoldingOptions options;
    options.levelWidths = {2};
    auto const result = storm::pomdp::transformer::RewardBoundUnfolder<ValueType>::transform(*dtmc, *formula, options);
    auto const& unfoldedBoundedUntilFormula = result.formula->asProbabilityOperatorFormula().getSubformula().asBoundedUntilFormula();

    ASSERT_TRUE(unfoldedBoundedUntilFormula.hasLowerBound());
    EXPECT_TRUE(unfoldedBoundedUntilFormula.isLowerBoundStrict());
    EXPECT_EQ(0, unfoldedBoundedUntilFormula.getLowerBound().evaluateAsInt());
    EXPECT_FALSE(unfoldedBoundedUntilFormula.hasUpperBound());
    EXPECT_EQ("dim0_levelReward", unfoldedBoundedUntilFormula.getTimeBoundReference().getRewardModelName());
    EXPECT_EQ(expectedValue, getInitialValue(result.model, result.formula));
}

TEST(RewardBoundUnfolder, DropsTriviallySatisfiedLowerBoundForDtmc) {
    auto const dtmc = buildDtmc();
    auto const formula = makeRewardBoundedFormula(0, std::nullopt);
    auto const expectedValue = getInitialValue(dtmc, formula);

    auto const result = storm::pomdp::transformer::RewardBoundUnfolder<ValueType>::transform(*dtmc, *formula);

    EXPECT_TRUE(result.formula->asProbabilityOperatorFormula().getSubformula().isUntilFormula());
    EXPECT_EQ(expectedValue, getInitialValue(result.model, result.formula));
}
