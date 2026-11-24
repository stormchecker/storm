#include "storm-config.h"
#include "test/storm_gtest.h"

#include <optional>

#include "storm-parsers/api/storm-parsers.h"
#include "storm-parsers/parser/AutoParser.h"
#include "storm-parsers/parser/DeterministicModelParser.h"
#include "storm-parsers/parser/MarkovAutomatonParser.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/api/storm.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/transformer/TransitionToActionRewardTransformer.h"
#include "storm/utility/constants.h"

namespace {
template<typename TransitionValueType, typename RewardValueType>
std::shared_ptr<storm::models::sparse::Dtmc<TransitionValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>> buildDtmcWithTransitionRewards(
    RewardValueType const& firstReward, RewardValueType const& secondReward) {
    storm::storage::SparseMatrixBuilder<TransitionValueType> transitionBuilder(3, 3, 3);
    transitionBuilder.addNextValue(0, 2, storm::utility::one<TransitionValueType>());
    transitionBuilder.addNextValue(1, 2, storm::utility::one<TransitionValueType>());
    transitionBuilder.addNextValue(2, 2, storm::utility::one<TransitionValueType>());

    storm::storage::SparseMatrixBuilder<RewardValueType> rewardBuilder(3, 3, 2);
    rewardBuilder.addNextValue(0, 2, firstReward);
    rewardBuilder.addNextValue(1, 2, secondReward);

    storm::models::sparse::StateLabeling labeling(3);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    storm::storage::sparse::ModelComponents<TransitionValueType, storm::models::sparse::StandardRewardModel<RewardValueType>> components(
        transitionBuilder.build(), std::move(labeling));
    std::optional<storm::storage::SparseMatrix<RewardValueType>> transitionRewards(rewardBuilder.build());
    components.rewardModels.emplace("rew",
                                    storm::models::sparse::StandardRewardModel<RewardValueType>(std::nullopt, std::nullopt, std::move(transitionRewards)));
    return std::make_shared<storm::models::sparse::Dtmc<TransitionValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>>(
        std::move(components));
}

template<typename TransitionValueType, typename RewardValueType>
void checkTransformedDtmc(storm::transformer::TransitionToActionRewardTransformerReturnType<
                              TransitionValueType, storm::models::sparse::StandardRewardModel<RewardValueType>> const& transformed,
                          RewardValueType const& firstReward, RewardValueType const& secondReward) {
    ASSERT_EQ(5ull, transformed.model->getNumberOfStates());
    EXPECT_EQ((std::vector<uint64_t>{0, 1, 4}), transformed.originalToNewStateIndices);

    auto const& transitionMatrix = transformed.model->getTransitionMatrix();
    auto checkTransition = [&transitionMatrix](uint64_t row, uint64_t column) {
        auto const rowEntries = transitionMatrix.getRow(row);
        ASSERT_EQ(1ull, rowEntries.getNumberOfEntries());
        auto const& entry = *rowEntries.begin();
        EXPECT_EQ(column, entry.getColumn());
        EXPECT_EQ(storm::utility::one<TransitionValueType>(), entry.getValue());
    };
    checkTransition(0, 2);
    checkTransition(1, 3);
    checkTransition(2, 4);
    checkTransition(3, 4);
    checkTransition(4, 4);

    auto const& rewardModel = transformed.model->getRewardModel("rew");
    ASSERT_TRUE(rewardModel.hasStateActionRewards());
    ASSERT_FALSE(rewardModel.hasTransitionRewards());
    auto const& actionRewards = rewardModel.getStateActionRewardVector();
    ASSERT_EQ(5ull, actionRewards.size());
    EXPECT_EQ(firstReward, actionRewards[2]);
    EXPECT_EQ(secondReward, actionRewards[3]);
}

double computeInitialReward(std::shared_ptr<storm::models::sparse::Model<double>> const& model, std::string const& formulaString) {
    auto const formula = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulaString)).front();
    auto const result = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(formula, true));
    return result->asExplicitQuantitativeCheckResult<double>()[*model->getInitialStates().begin()];
}

TEST(TransitionToActionRewardTransformerTest, DtmcDie) {
    auto const model = storm::parser::AutoParser<>::parseModel(STORM_TEST_RESOURCES_DIR "/dtmc/die.tra", STORM_TEST_RESOURCES_DIR "/dtmc/die.lab", "",
                                                               STORM_TEST_RESOURCES_DIR "/rew/die.coin_flips.trans.rew");

    auto const transformed = storm::transformer::transformTransitionToActionRewards<double>(model, {""});

    EXPECT_EQ(25ull, transformed.model->getNumberOfStates());
    EXPECT_EQ(32ull, transformed.model->getNumberOfTransitions());
    EXPECT_EQ((std::vector<uint64_t>{0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24}), transformed.originalToNewStateIndices);
    EXPECT_TRUE(transformed.model->getRewardModel("").hasStateActionRewards());
    EXPECT_FALSE(transformed.model->getRewardModel("").hasTransitionRewards());

    EXPECT_NEAR(11.0 / 3.0, computeInitialReward(transformed.model, "R=? [F \"done\"]"), 1e-6);
}

TEST(TransitionToActionRewardTransformerTest, CtmcDie) {
    auto const model = std::make_shared<storm::models::sparse::Ctmc<double>>(storm::parser::DeterministicModelParser<>::parseCtmc(
        STORM_TEST_RESOURCES_DIR "/tra/die.tra", STORM_TEST_RESOURCES_DIR "/lab/die.lab", "", STORM_TEST_RESOURCES_DIR "/rew/die.coin_flips.trans.rew"));

    auto const transformed = storm::transformer::transformTransitionToActionRewards<double>(model, {""});

    EXPECT_EQ(storm::models::ModelType::Ctmc, transformed.model->getType());
    EXPECT_EQ(25ull, transformed.model->getNumberOfStates());
    EXPECT_EQ(32ull, transformed.model->getNumberOfTransitions());
    EXPECT_TRUE(transformed.model->getRewardModel("").hasStateActionRewards());
    EXPECT_FALSE(transformed.model->getRewardModel("").hasTransitionRewards());

    EXPECT_NEAR(11.0 / 3.0, computeInitialReward(transformed.model, "R=? [F \"done\"]"), 1e-6);
}

TEST(TransitionToActionRewardTransformerTest, MdpTwoDice) {
    auto const model = storm::parser::AutoParser<>::parseModel(STORM_TEST_RESOURCES_DIR "/tra/two_dice.tra", STORM_TEST_RESOURCES_DIR "/lab/two_dice.lab", "",
                                                               STORM_TEST_RESOURCES_DIR "/rew/two_dice.flip.trans.rew");

    auto const transformed = storm::transformer::transformTransitionToActionRewards<double>(model, {""});

    EXPECT_EQ(storm::models::ModelType::Mdp, transformed.model->getType());
    EXPECT_EQ(337ull, transformed.model->getNumberOfStates());
    EXPECT_EQ(604ull, transformed.model->getNumberOfTransitions());
    ASSERT_EQ(169ull, transformed.originalToNewStateIndices.size());
    for (uint64_t state = 0; state < transformed.originalToNewStateIndices.size(); ++state) {
        EXPECT_EQ(2 * state, transformed.originalToNewStateIndices[state]);
    }
    EXPECT_TRUE(transformed.model->getRewardModel("").hasStateActionRewards());
    EXPECT_FALSE(transformed.model->getRewardModel("").hasTransitionRewards());

    EXPECT_NEAR(22.0 / 3.0, computeInitialReward(transformed.model, "Rmin=? [F \"done\"]"), 1e-6);
}

TEST(TransitionToActionRewardTransformerTest, MarkovAutomatonGeneral) {
    auto const model = std::make_shared<storm::models::sparse::MarkovAutomaton<double>>(storm::parser::MarkovAutomatonParser<>::parseMarkovAutomaton(
        STORM_TEST_RESOURCES_DIR "/tra/ma_general.tra", STORM_TEST_RESOURCES_DIR "/lab/ma_general.lab", STORM_TEST_RESOURCES_DIR "/rew/ma_general.state.rew"));
    auto transitionRewards = model->getTransitionMatrix();
    for (auto& entry : transitionRewards) {
        entry.setValue(1.0);
    }
    auto const stateRewards = model->getRewardModel("").getOptionalStateRewardVector();
    model->getRewardModel("") = storm::models::sparse::StandardRewardModel<double>(std::move(stateRewards), std::nullopt, std::move(transitionRewards));

    auto const transformed = storm::transformer::transformTransitionToActionRewards<double>(model, {""});

    EXPECT_EQ(storm::models::ModelType::MarkovAutomaton, transformed.model->getType());
    EXPECT_EQ(12ull, transformed.model->getNumberOfStates());
    EXPECT_EQ(18ull, transformed.model->getNumberOfTransitions());
    EXPECT_EQ((std::vector<uint64_t>{1, 3, 5, 7, 9, 11}), transformed.originalToNewStateIndices);
    EXPECT_TRUE(transformed.model->getRewardModel("").hasStateActionRewards());
    EXPECT_FALSE(transformed.model->getRewardModel("").hasTransitionRewards());

    auto transformedMa = transformed.model->as<storm::models::sparse::MarkovAutomaton<double>>();
    EXPECT_EQ(2ull, transformedMa->getMarkovianStates().getNumberOfSetBits());
    EXPECT_TRUE(transformedMa->isMarkovianState(1));
    EXPECT_EQ(2.0, transformedMa->getExitRate(1));
    EXPECT_TRUE(transformedMa->isMarkovianState(5));
    EXPECT_EQ(15.0, transformedMa->getExitRate(5));
    for (uint64_t state = 0; state < transformedMa->getNumberOfStates(); state += 2) {
        EXPECT_TRUE(transformedMa->isProbabilisticState(state));
    }
}

TEST(TransitionToActionRewardTransformerTest, OverlappingIntervalRewardsAreNotMerged) {
    storm::Interval const firstReward(1.0, 3.0);
    storm::Interval const secondReward(2.0, 4.0);
    auto const model = buildDtmcWithTransitionRewards<storm::Interval>(firstReward, secondReward);

    auto const transformed = storm::transformer::transformTransitionToActionRewards<storm::Interval>(model, {"rew"});

    checkTransformedDtmc(transformed, firstReward, secondReward);
}

TEST(TransitionToActionRewardTransformerTest, OverlappingRationalIntervalRewardsAreNotMerged) {
    storm::RationalInterval const firstReward(storm::RationalNumber(1), storm::RationalNumber(3));
    storm::RationalInterval const secondReward(storm::RationalNumber(2), storm::RationalNumber(4));
    auto const model = buildDtmcWithTransitionRewards<storm::RationalInterval>(firstReward, secondReward);

    auto const transformed = storm::transformer::transformTransitionToActionRewards<storm::RationalInterval>(model, {"rew"});

    checkTransformedDtmc(transformed, firstReward, secondReward);
}

TEST(TransitionToActionRewardTransformerTest, DoubleOverlappingIntervalRewardsAreNotMerged) {
    storm::Interval const firstReward(1.0, 3.0);
    storm::Interval const secondReward(2.0, 4.0);
    auto const model = buildDtmcWithTransitionRewards<double>(firstReward, secondReward);

    auto const transformed =
        storm::transformer::transformTransitionToActionRewards<double, storm::models::sparse::StandardRewardModel<storm::Interval>>(model, {"rew"});

    checkTransformedDtmc(transformed, firstReward, secondReward);
}

TEST(TransitionToActionRewardTransformerTest, RationalOverlappingRationalIntervalRewardsAreNotMerged) {
    storm::RationalInterval const firstReward(storm::RationalNumber(1), storm::RationalNumber(3));
    storm::RationalInterval const secondReward(storm::RationalNumber(2), storm::RationalNumber(4));
    auto const model = buildDtmcWithTransitionRewards<storm::RationalNumber>(firstReward, secondReward);

    auto const transformed =
        storm::transformer::transformTransitionToActionRewards<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>(
            model, {"rew"});

    checkTransformedDtmc(transformed, firstReward, secondReward);
}

}  // namespace
