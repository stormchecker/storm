#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/simulator/DiscreteTimeSparseModelSimulator.h"
#include "storm/storage/SparseMatrix.h"

TEST(DiscreteTimeSparseModelSimulatorTest, TransitionRewards) {
    // 0 --0.5--> 1, 0 --0.5--> 2, 1 -> 1, 2 -> 2
    storm::storage::SparseMatrixBuilder<double> builder(3, 3, 4);
    builder.addNextValue(0, 1, 0.5);
    builder.addNextValue(0, 2, 0.5);
    builder.addNextValue(1, 1, 1.0);
    builder.addNextValue(2, 2, 1.0);
    auto matrix = builder.build();

    // Transition rewards: 0->1 yields 3, 0->2 yields 7 and the self loops yield nothing (entries omitted).
    storm::storage::SparseMatrixBuilder<double> rewBuilder(3, 3, 2);
    rewBuilder.addNextValue(0, 1, 3.0);
    rewBuilder.addNextValue(0, 2, 7.0);
    auto rewMatrix = rewBuilder.build();

    storm::models::sparse::StateLabeling labeling(3);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<double>> rewardModels;
    rewardModels.emplace("r", storm::models::sparse::StandardRewardModel<double>(std::nullopt, std::nullopt, std::move(rewMatrix)));
    storm::models::sparse::Dtmc<double> dtmc(std::move(matrix), std::move(labeling), std::move(rewardModels));

    storm::simulator::DiscreteTimeSparseModelSimulator<double> sim(dtmc);
    sim.setSeed(1);
    bool seen1 = false, seen2 = false;
    for (int i = 0; i < 50; ++i) {
        sim.resetToInitial();
        EXPECT_EQ(0.0, sim.getLastRewards()[0]);
        ASSERT_TRUE(sim.randomStep());
        if (sim.getCurrentState() == 1) {
            EXPECT_EQ(3.0, sim.getLastRewards()[0]);
            seen1 = true;
        } else {
            ASSERT_EQ(2ul, sim.getCurrentState());
            EXPECT_EQ(7.0, sim.getLastRewards()[0]);
            seen2 = true;
        }
        ASSERT_TRUE(sim.randomStep());
        EXPECT_EQ(0.0, sim.getLastRewards()[0]);
    }
    EXPECT_TRUE(seen1);
    EXPECT_TRUE(seen2);
}
