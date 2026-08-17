#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/DeterministicModelParser.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/StandardRewardModel.h"

TEST(DeterministicModelParserTest, NonExistingFile) {
    // No matter what happens, please do NOT create a file with the name "nonExistingFile.not"!
    STORM_SILENT_ASSERT_THROW(
        storm::parser::DeterministicModelParser<>::parseDtmc(STORM_TEST_RESOURCES_DIR "/nonExistingFile.not", STORM_TEST_RESOURCES_DIR "/nonExistingFile.not"),
        storm::exceptions::FileIoException);

    STORM_SILENT_ASSERT_THROW(
        storm::parser::DeterministicModelParser<>::parseCtmc(STORM_TEST_RESOURCES_DIR "/nonExistingFile.not", STORM_TEST_RESOURCES_DIR "/nonExistingFile.not"),
        storm::exceptions::FileIoException);
}

TEST(DeterministicModelParserTest, BasicDtmcParsing) {
    // Parse a Dtmc and check the result.
    storm::models::sparse::Dtmc<double> dtmc(storm::parser::DeterministicModelParser<>::parseDtmc(
        STORM_TEST_RESOURCES_DIR "/tra/dtmc_general.tra", STORM_TEST_RESOURCES_DIR "/lab/dtmc_general.lab",
        STORM_TEST_RESOURCES_DIR "/rew/dtmc_general.state.rew", STORM_TEST_RESOURCES_DIR "/rew/dtmc_general.trans.rew"));

    ASSERT_EQ(8ul, dtmc.getNumberOfStates());
    ASSERT_EQ(16ul, dtmc.getNumberOfTransitions());

    ASSERT_EQ(2ul, dtmc.getInitialStates().getNumberOfSetBits());
    ASSERT_TRUE(dtmc.getInitialStates().get(0));
    ASSERT_TRUE(dtmc.getInitialStates().get(7));
    ASSERT_EQ(5ul, dtmc.getStateLabeling().getNumberOfLabels());
    ASSERT_EQ(2ul, dtmc.getLabelsOfState(6).size());

    ASSERT_TRUE(dtmc.hasRewardModel());
    ASSERT_EQ(42, dtmc.getRewardModel("").getStateRewardVector()[7]);
    double rewardSum = 0;
    for (uint_fast64_t i = 0; i < dtmc.getRewardModel("").getStateRewardVector().size(); i++) {
        rewardSum += dtmc.getRewardModel("").getStateRewardVector()[i];
    }
    ASSERT_EQ(263.32, rewardSum);

    ASSERT_TRUE(dtmc.getRewardModel("").hasTransitionRewards());
    ASSERT_EQ(17ul, dtmc.getRewardModel("").getTransitionRewardMatrix().getEntryCount());
    rewardSum = 0;
    for (uint_fast64_t i = 0; i < dtmc.getRewardModel("").getTransitionRewardMatrix().getRowCount(); i++) {
        rewardSum += dtmc.getRewardModel("").getTransitionRewardMatrix().getRowSum(i);
    }
    ASSERT_EQ(125.4, rewardSum);
}

TEST(DeterministicModelParserTest, BasicCtmcParsing) {
    // Parse a Ctmc and check the result.
    storm::models::sparse::Ctmc<double> ctmc(storm::parser::DeterministicModelParser<>::parseCtmc(
        STORM_TEST_RESOURCES_DIR "/tra/dtmc_general.tra", STORM_TEST_RESOURCES_DIR "/lab/dtmc_general.lab",
        STORM_TEST_RESOURCES_DIR "/rew/dtmc_general.state.rew", STORM_TEST_RESOURCES_DIR "/rew/dtmc_general.trans.rew"));

    ASSERT_EQ(8ul, ctmc.getNumberOfStates());
    ASSERT_EQ(16ul, ctmc.getNumberOfTransitions());

    ASSERT_EQ(2ul, ctmc.getInitialStates().getNumberOfSetBits());
    ASSERT_TRUE(ctmc.getInitialStates().get(0));
    ASSERT_TRUE(ctmc.getInitialStates().get(7));
    ASSERT_EQ(5ul, ctmc.getStateLabeling().getNumberOfLabels());
    ASSERT_EQ(2ul, ctmc.getLabelsOfState(6).size());

    ASSERT_TRUE(ctmc.hasRewardModel());
    ASSERT_EQ(42, ctmc.getRewardModel("").getStateRewardVector()[7]);
    double rewardSum = 0;
    for (uint_fast64_t i = 0; i < ctmc.getRewardModel("").getStateRewardVector().size(); i++) {
        rewardSum += ctmc.getRewardModel("").getStateRewardVector()[i];
    }
    ASSERT_EQ(263.32, rewardSum);

    ASSERT_TRUE(ctmc.getRewardModel("").hasTransitionRewards());
    ASSERT_EQ(17ul, ctmc.getRewardModel("").getTransitionRewardMatrix().getEntryCount());
    rewardSum = 0;
    for (uint_fast64_t i = 0; i < ctmc.getRewardModel("").getTransitionRewardMatrix().getRowCount(); i++) {
        rewardSum += ctmc.getRewardModel("").getTransitionRewardMatrix().getRowSum(i);
    }
    ASSERT_EQ(125.4, rewardSum);
}

TEST(DeterministicModelParserTest, MismatchedFiles) {
    // Test file combinations that do not match, i.e. differing number of states, transitions, etc.

    // The labeling file contains a label for a non existent state.
    STORM_SILENT_ASSERT_THROW(storm::parser::DeterministicModelParser<>::parseDtmc(STORM_TEST_RESOURCES_DIR "/tra/dtmc_mismatched.tra",
                                                                                   STORM_TEST_RESOURCES_DIR "/lab/dtmc_general.lab"),
                              storm::exceptions::OutOfRangeException);

    // The state reward file contains a reward for a non existent state.
    STORM_SILENT_ASSERT_THROW(storm::parser::DeterministicModelParser<>::parseDtmc(STORM_TEST_RESOURCES_DIR "/tra/dtmc_mismatched.tra",
                                                                                   STORM_TEST_RESOURCES_DIR "/lab/dtmc_mismatched.lab",
                                                                                   STORM_TEST_RESOURCES_DIR "/rew/dtmc_general.state.rew"),
                              storm::exceptions::OutOfRangeException);

    // The transition reward file contains rewards for a non existent state.
    STORM_SILENT_ASSERT_THROW(storm::parser::DeterministicModelParser<>::parseDtmc(STORM_TEST_RESOURCES_DIR "/tra/dtmc_mismatched.tra",
                                                                                   STORM_TEST_RESOURCES_DIR "/lab/dtmc_mismatched.lab", "",
                                                                                   STORM_TEST_RESOURCES_DIR "/rew/dtmc_general.trans.rew"),
                              storm::exceptions::OutOfRangeException);

    // The transition reward file contains rewards for a non existent transition
    STORM_SILENT_ASSERT_THROW(storm::parser::DeterministicModelParser<>::parseDtmc(STORM_TEST_RESOURCES_DIR "/tra/dtmc_mismatched.tra",
                                                                                   STORM_TEST_RESOURCES_DIR "/lab/dtmc_mismatched.lab", "",
                                                                                   STORM_TEST_RESOURCES_DIR "/rew/dtmc_mismatched.trans.rew"),
                              storm::exceptions::OutOfRangeException);
}
