#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "test/storm_gtest.h"

#include "storm-pomdp/transformer/ToStateBasedObservationTransformer.h"
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
        storm::pomdp::transformer::ToStateBasedObservationTransformer<double>::transform(*mdp, [](uint64_t, uint64_t, uint64_t) { return 1; }, 0);

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

    auto const result = storm::pomdp::transformer::ToStateBasedObservationTransformer<double>::transformRewardAware(*pomdp, {"r"});

    ASSERT_NE(nullptr, result);
    EXPECT_EQ(4u, result->getNumberOfStates());
    EXPECT_EQ((std::vector<uint32_t>{0, 3, 4, 0}), result->getObservations());
    EXPECT_TRUE(result->getStateLabeling().getStates("isolated").get(3));
    EXPECT_EQ(7.0, result->getRewardModel("r").getStateReward(3));
    EXPECT_EQ(3.0, result->getRewardModel("r").getStateActionReward(3));
}
