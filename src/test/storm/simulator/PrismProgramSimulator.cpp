#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/PrismParser.h"
#include "storm/environment/Environment.h"
#include "storm/simulator/PrismProgramSimulator.h"

TEST(PrismProgramSimulatorTest, KnuthYaoDieTest) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::Environment env;
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/die_c1.nm");
    storm::builder::BuilderOptions options;
    options.setBuildAllRewardModels();

    storm::simulator::DiscreteTimePrismProgramSimulator<double> sim(program, options);
    sim.setSeed(42);
    EXPECT_EQ("coin_flips", sim.getRewardNames()[0]);
    auto rew = sim.getLastRewards();
    rew = sim.getLastRewards();
    EXPECT_EQ(1ul, rew.size());
    EXPECT_EQ(0.0, rew[0]);
    auto labels = sim.getCurrentStateLabelling();
    EXPECT_EQ(0ul, labels.size());
    EXPECT_EQ(2ul, sim.getChoices().size());
    sim.step(0);
    rew = sim.getLastRewards();
    EXPECT_EQ(1ul, rew.size());
    EXPECT_EQ(0.0, rew[0]);
    labels = sim.getCurrentStateLabelling();
    EXPECT_EQ(0ul, labels.size());
    EXPECT_EQ(1ul, sim.getChoices().size());
    sim.step(0);
    rew = sim.getLastRewards();
    EXPECT_EQ(1ul, rew.size());
    EXPECT_EQ(1.0, rew[0]);
    labels = sim.getCurrentStateLabelling();
    EXPECT_EQ(0ul, labels.size());
    sim.step(0);
    rew = sim.getLastRewards();
    EXPECT_EQ(1ul, rew.size());
    EXPECT_EQ(1.0, rew[0]);
    labels = sim.getCurrentStateLabelling();
    EXPECT_EQ(2ul, labels.size());
    EXPECT_TRUE(std::count(labels.begin(), labels.end(), "done") == 1);
    EXPECT_TRUE(std::count(labels.begin(), labels.end(), "five") == 1);
    sim.step(0);
    rew = sim.getLastRewards();
    EXPECT_EQ(1ul, rew.size());
    EXPECT_EQ(0.0, rew[0]);
    labels = sim.getCurrentStateLabelling();
    EXPECT_TRUE(std::count(labels.begin(), labels.end(), "done") == 1);
    EXPECT_TRUE(std::count(labels.begin(), labels.end(), "five") == 1);
}
