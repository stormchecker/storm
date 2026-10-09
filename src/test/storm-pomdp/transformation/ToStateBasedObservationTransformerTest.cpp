#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "test/storm_gtest.h"

#include "storm-pomdp/transformer/ToStateBasedObservationTransformer.h"
#include "storm/models/sparse/ChoiceLabeling.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/ModelComponents.h"

namespace {

std::shared_ptr<storm::models::sparse::Mdp<double>> buildMdp() {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 1, 1.0);
    matrixBuilder.addNextValue(1, 1, 1.0);
    matrixBuilder.newRowGroup(2);
    matrixBuilder.addNextValue(2, 2, 1.0);
    matrixBuilder.addNextValue(3, 2, 1.0);
    matrixBuilder.newRowGroup(4);
    matrixBuilder.addNextValue(4, 2, 1.0);

    storm::models::sparse::StateLabeling labeling(3);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(), std::move(labeling));
    return std::make_shared<storm::models::sparse::Mdp<double>>(std::move(components));
}

storm::storage::sparse::ModelComponents<double> buildComponentsWithUnreachableState() {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 1, 1.0);
    matrixBuilder.newRowGroup(1);
    matrixBuilder.addNextValue(1, 1, 1.0);
    matrixBuilder.newRowGroup(2);
    matrixBuilder.addNextValue(2, 1, 1.0);

    storm::models::sparse::StateLabeling labeling(3);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    labeling.addLabel("isolated");
    labeling.addLabelToState("isolated", 2);
    storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(3, 3, 3), std::move(labeling));
    components.rewardModels.emplace("r",
                                    storm::models::sparse::StandardRewardModel<double>(std::vector<double>{0.0, 0.0, 7.0}, std::vector<double>{1.0, 1.0, 3.0}));
    return components;
}

}  // namespace

TEST(ToStateBasedObservationTransformer, PassesLocalActionIndicesToCallback) {
    auto const mdp = buildMdp();
    std::vector<std::pair<uint64_t, uint64_t>> observedStateActions;

    auto const result = storm::pomdp::transformer::ToStateBasedObservationTransformer<double>::transform(
        *mdp,
        [&observedStateActions](uint64_t state, uint64_t action, uint64_t) {
            observedStateActions.emplace_back(state, action);
            return 0;
        },
        0);

    EXPECT_NE(nullptr, result);
    EXPECT_EQ((std::vector<std::pair<uint64_t, uint64_t>>{{0, 0}, {0, 1}, {1, 0}, {1, 1}, {2, 0}}), observedStateActions);
}

TEST(ToStateBasedObservationTransformer, KeepsNonInitialStateWithoutIncomingTransitions) {
    auto const mdp = std::make_shared<storm::models::sparse::Mdp<double>>(buildComponentsWithUnreachableState());

    auto const result =
        storm::pomdp::transformer::ToStateBasedObservationTransformer<double>::transform(*mdp, [](uint64_t, uint64_t, uint64_t) { return 1; }, 0, false);

    ASSERT_NE(nullptr, result);
    EXPECT_EQ(3u, result->getNumberOfStates());
    EXPECT_EQ(3u, result->getNumberOfChoices());
    EXPECT_EQ((std::vector<uint32_t>{0, 1, 0}), result->getObservations());
    EXPECT_TRUE(result->getStateLabeling().getStates("isolated").get(2));
    EXPECT_FALSE(result->getInitialStates().get(2));
    EXPECT_EQ(1u, result->getTransitionMatrix().getRowGroupSize(2));
    EXPECT_EQ(1u, result->getTransitionMatrix().getRow(2).begin()->getColumn());
    EXPECT_EQ(7.0, result->getRewardModel("r").getStateReward(2));
    EXPECT_EQ(3.0, result->getRewardModel("r").getStateActionReward(2));
}

TEST(ToStateBasedObservationTransformer, RewardAwareTransformationKeepsUnreachableState) {
    auto components = buildComponentsWithUnreachableState();
    components.observabilityClasses = std::vector<uint32_t>{0, 1, 2};
    auto const pomdp = std::make_shared<storm::models::sparse::Pomdp<double>>(std::move(components));

    auto const result = storm::pomdp::transformer::ToStateBasedObservationTransformer<double>::transformRewardAware(*pomdp, {"r"}, false);

    ASSERT_NE(nullptr, result);
    EXPECT_EQ(4u, result->getNumberOfStates());
    EXPECT_EQ((std::vector<uint32_t>{0, 3, 4, 0}), result->getObservations());
    EXPECT_TRUE(result->getStateLabeling().getStates("isolated").get(3));
    EXPECT_EQ(7.0, result->getRewardModel("r").getStateReward(3));
    EXPECT_EQ(3.0, result->getRewardModel("r").getStateActionReward(3));
}

TEST(ToStateBasedObservationTransformer, DropsUnreachableStateWithDifferentActionCount) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 1, 1.0);
    matrixBuilder.newRowGroup(1);
    matrixBuilder.addNextValue(1, 1, 1.0);
    matrixBuilder.newRowGroup(2);
    matrixBuilder.addNextValue(2, 1, 1.0);
    matrixBuilder.addNextValue(3, 1, 1.0);
    storm::models::sparse::StateLabeling labeling(3);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    storm::models::sparse::Mdp<double> mdp(matrixBuilder.build(4, 3, 3), std::move(labeling));

    auto const result =
        storm::pomdp::transformer::ToStateBasedObservationTransformer<double>::transform(mdp, [](uint64_t, uint64_t, uint64_t) { return 0; }, 0);

    ASSERT_NE(nullptr, result);
    EXPECT_EQ(2u, result->getNumberOfStates());
    EXPECT_EQ(2u, result->getNumberOfChoices());
    EXPECT_EQ((std::vector<uint32_t>{0, 0}), result->getObservations());
    for (uint64_t state = 0; state < result->getNumberOfStates(); ++state) {
        for (auto peer : result->getStatesWithObservation(result->getObservation(state))) {
            EXPECT_EQ(result->getTransitionMatrix().getRowGroupSize(state), result->getTransitionMatrix().getRowGroupSize(peer));
        }
    }
}

TEST(ToStateBasedObservationTransformer, DropsDisconnectedCycle) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    for (uint64_t state = 0; state < 4; ++state) {
        matrixBuilder.newRowGroup(state);
        matrixBuilder.addNextValue(state, state < 2 ? 1 : (state == 2 ? 3 : 2), 1.0);
    }
    storm::models::sparse::StateLabeling labeling(4);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    storm::models::sparse::Mdp<double> mdp(matrixBuilder.build(), std::move(labeling));

    auto const result = storm::pomdp::transformer::ToStateBasedObservationTransformer<double>::transform(
        mdp,
        [](uint64_t state, uint64_t, uint64_t) {
            EXPECT_LT(state, 2u);
            return 1;
        },
        0);

    ASSERT_NE(nullptr, result);
    EXPECT_EQ(2u, result->getNumberOfStates());
    EXPECT_EQ(2u, result->getNumberOfChoices());
    EXPECT_EQ((std::vector<uint32_t>{0, 1}), result->getObservations());
}

TEST(ToStateBasedObservationTransformer, RemapsMetadataAndIgnoresUnreachablePredecessorObservations) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 2, 1.0);
    matrixBuilder.newRowGroup(1);
    matrixBuilder.addNextValue(1, 2, 1.0);
    matrixBuilder.addNextValue(2, 2, 1.0);
    matrixBuilder.newRowGroup(3);
    matrixBuilder.addNextValue(3, 3, 1.0);
    matrixBuilder.addNextValue(4, 2, 1.0);
    matrixBuilder.newRowGroup(5);
    matrixBuilder.addNextValue(5, 3, 1.0);
    storm::models::sparse::StateLabeling labeling(4);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    labeling.addLabel("removed");
    labeling.addLabelToState("removed", 1);
    labeling.addLabel("target");
    labeling.addLabelToState("target", 2);
    storm::storage::sparse::ModelComponents<double> components(matrixBuilder.build(), std::move(labeling));
    components.choiceLabeling.emplace(6);
    for (auto const& [label, choice] : std::vector<std::pair<std::string, uint64_t>>{{"removed", 1}, {"go", 3}, {"wait", 4}, {"finish", 5}}) {
        components.choiceLabeling->addLabel(label);
        components.choiceLabeling->addLabelToChoice(label, choice);
    }
    components.rewardModels.emplace(
        "r", storm::models::sparse::StandardRewardModel<double>(std::vector<double>{10, 20, 30, 40}, std::vector<double>{100, 200, 201, 300, 301, 400}));
    storm::models::sparse::Mdp<double> mdp(std::move(components));
    std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> callbackArguments;

    auto const result = storm::pomdp::transformer::ToStateBasedObservationTransformer<double>::transform(
        mdp,
        [&callbackArguments](uint64_t state, uint64_t action, uint64_t target) {
            callbackArguments.emplace_back(state, action, target);
            return state == 0 ? 1 : (state == 1 ? 99 : target);
        },
        0);

    ASSERT_NE(nullptr, result);
    EXPECT_EQ((std::vector<std::tuple<uint64_t, uint64_t, uint64_t>>{{0, 0, 2}, {2, 0, 3}, {2, 1, 2}, {3, 0, 3}}), callbackArguments);
    EXPECT_EQ(4u, result->getNumberOfStates());
    EXPECT_EQ(6u, result->getNumberOfChoices());
    EXPECT_EQ((std::vector<uint32_t>{0, 1, 2, 3}), result->getObservations());
    EXPECT_TRUE(result->getStateLabeling().getStates("removed").empty());
    EXPECT_EQ(2u, result->getStateLabeling().getStates("target").getNumberOfSetBits());
    EXPECT_TRUE(result->getStateLabeling().getStates("target").get(1));
    EXPECT_TRUE(result->getStateLabeling().getStates("target").get(2));
    EXPECT_EQ(1u, result->getInitialStates().getNumberOfSetBits());
    EXPECT_TRUE(result->getInitialStates().get(0));
    EXPECT_EQ((std::vector<double>{10, 30, 30, 40}), result->getRewardModel("r").getStateRewardVector());
    EXPECT_EQ((std::vector<double>{100, 300, 301, 300, 301, 400}), result->getRewardModel("r").getStateActionRewardVector());
    EXPECT_TRUE(result->getChoiceLabeling().getChoices("removed").empty());
    EXPECT_EQ(2u, result->getChoiceLabeling().getChoices("go").getNumberOfSetBits());
    EXPECT_EQ(2u, result->getChoiceLabeling().getChoices("wait").getNumberOfSetBits());
    std::vector<uint64_t> const expectedTargets{1, 3, 2, 3, 2, 3};
    auto const& resultMatrix = result->getTransitionMatrix();
    for (uint64_t choice = 0; choice < expectedTargets.size(); ++choice) {
        auto const row = resultMatrix.getRow(choice);
        ASSERT_EQ(1u, row.getNumberOfEntries());
        EXPECT_EQ(expectedTargets[choice], row.begin()->getColumn());
        EXPECT_EQ(1.0, row.begin()->getValue());
        if (choice == 1 || choice == 3) {
            EXPECT_TRUE(result->getChoiceLabeling().getChoiceHasLabel("go", choice));
        } else if (choice == 2 || choice == 4) {
            EXPECT_TRUE(result->getChoiceLabeling().getChoiceHasLabel("wait", choice));
        }
    }
    EXPECT_TRUE(result->getChoiceLabeling().getChoiceHasLabel("finish", 5));
}

TEST(ToStateBasedObservationTransformer, RetainsAllInitialStates) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    for (uint64_t state = 0; state < 3; ++state) {
        matrixBuilder.newRowGroup(state);
        matrixBuilder.addNextValue(state, 2, 1.0);
    }
    storm::models::sparse::StateLabeling labeling(3);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    labeling.addLabelToState("init", 2);
    storm::models::sparse::Mdp<double> mdp(matrixBuilder.build(), std::move(labeling));

    auto const result = storm::pomdp::transformer::ToStateBasedObservationTransformer<double>::transform(
        mdp,
        [](uint64_t state, uint64_t, uint64_t) {
            EXPECT_NE(1u, state);
            return 1;
        },
        0);

    ASSERT_NE(nullptr, result);
    EXPECT_EQ(3u, result->getNumberOfStates());
    EXPECT_EQ((std::vector<uint32_t>{0, 0, 1}), result->getObservations());
    EXPECT_EQ(2u, result->getInitialStates().getNumberOfSetBits());
    EXPECT_TRUE(result->getInitialStates().get(0));
    EXPECT_TRUE(result->getInitialStates().get(1));
    EXPECT_FALSE(result->getInitialStates().get(2));
}

TEST(ToStateBasedObservationTransformer, IgnoresZeroProbabilityTransitionsWhenDroppingUnreachableStates) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(0, 0, 0, false, true);
    matrixBuilder.newRowGroup(0);
    matrixBuilder.addNextValue(0, 0, 1.0);
    matrixBuilder.addNextValue(0, 1, 0.0);
    matrixBuilder.newRowGroup(1);
    matrixBuilder.addNextValue(1, 1, 1.0);
    storm::models::sparse::StateLabeling labeling(2);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    storm::models::sparse::Mdp<double> mdp(matrixBuilder.build(), std::move(labeling));
    ASSERT_EQ(3u, mdp.getTransitionMatrix().getEntryCount());
    std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> callbackArguments;
    auto observationFunction = [&callbackArguments](uint64_t state, uint64_t action, uint64_t target) {
        callbackArguments.emplace_back(state, action, target);
        return target;
    };

    auto const result = storm::pomdp::transformer::ToStateBasedObservationTransformer<double>::transform(mdp, observationFunction, 0);

    ASSERT_NE(nullptr, result);
    EXPECT_EQ(1u, result->getNumberOfStates());
    EXPECT_EQ(1u, result->getNumberOfChoices());
    EXPECT_EQ(1u, result->getTransitionMatrix().getEntryCount());
    EXPECT_EQ((std::vector<std::tuple<uint64_t, uint64_t, uint64_t>>{{0, 0, 0}}), callbackArguments);

    callbackArguments.clear();
    auto const unpruned = storm::pomdp::transformer::ToStateBasedObservationTransformer<double>::transform(mdp, observationFunction, 0, false);
    EXPECT_EQ(2u, unpruned->getNumberOfStates());
    EXPECT_EQ(3u, unpruned->getTransitionMatrix().getEntryCount());
    EXPECT_EQ((std::vector<std::tuple<uint64_t, uint64_t, uint64_t>>{{0, 0, 0}, {0, 0, 1}, {1, 0, 1}}), callbackArguments);
}

TEST(ToStateBasedObservationTransformer, RewardAwareTransformationDropsUnreachableState) {
    auto components = buildComponentsWithUnreachableState();
    components.observabilityClasses = std::vector<uint32_t>{0, 1, 2};
    auto const pomdp = std::make_shared<storm::models::sparse::Pomdp<double>>(std::move(components), true);

    auto const result = storm::pomdp::transformer::ToStateBasedObservationTransformer<double>::transformRewardAware(*pomdp, {"r"});

    ASSERT_NE(nullptr, result);
    EXPECT_EQ(2u, result->getNumberOfStates());
    EXPECT_EQ(2u, result->getNumberOfChoices());
    EXPECT_EQ((std::vector<uint32_t>{0, 3}), result->getObservations());
    EXPECT_TRUE(result->getStateLabeling().getStates("isolated").empty());
    EXPECT_EQ((std::vector<double>{0.0, 0.0}), result->getRewardModel("r").getStateRewardVector());
    EXPECT_EQ((std::vector<double>{1.0, 1.0}), result->getRewardModel("r").getStateActionRewardVector());
    EXPECT_TRUE(result->isCanonic());
}
