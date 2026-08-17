#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/NondeterministicModelParser.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"

TEST(NondeterministicModelParserTest, NonExistingFile) {
    // No matter what happens, please do NOT create a file with the name "nonExistingFile.not"!
    STORM_SILENT_ASSERT_THROW(storm::parser::NondeterministicModelParser<>::parseMdp(STORM_TEST_RESOURCES_DIR "/nonExistingFile.not",
                                                                                     STORM_TEST_RESOURCES_DIR "/nonExistingFile.not"),
                              storm::exceptions::FileIoException);
}

TEST(NondeterministicModelParserTest, BasicMdpParsing) {
    // Parse a Mdp and check the result.
    storm::models::sparse::Mdp<double> mdp(storm::parser::NondeterministicModelParser<>::parseMdp(
        STORM_TEST_RESOURCES_DIR "/tra/mdp_general.tra", STORM_TEST_RESOURCES_DIR "/lab/mdp_general.lab", STORM_TEST_RESOURCES_DIR "/rew/mdp_general.state.rew",
        STORM_TEST_RESOURCES_DIR "/rew/mdp_general.trans.rew"));

    ASSERT_EQ(6ul, mdp.getNumberOfStates());
    ASSERT_EQ(22ul, mdp.getNumberOfTransitions());
    ASSERT_EQ(11ul, mdp.getNumberOfChoices());

    ASSERT_EQ(2ul, mdp.getInitialStates().getNumberOfSetBits());
    ASSERT_TRUE(mdp.getInitialStates().get(0));
    ASSERT_TRUE(mdp.getInitialStates().get(4));
    ASSERT_EQ(4ul, mdp.getStateLabeling().getNumberOfLabels());
    ASSERT_EQ(3ul, mdp.getLabelsOfState(0).size());

    ASSERT_TRUE(mdp.hasRewardModel());
    ASSERT_EQ(0, mdp.getRewardModel("").getStateRewardVector()[0]);
    ASSERT_EQ(42, mdp.getRewardModel("").getStateRewardVector()[4]);
    double rewardSum = 0;
    for (uint_fast64_t i = 0; i < mdp.getRewardModel("").getStateRewardVector().size(); i++) {
        rewardSum += mdp.getRewardModel("").getStateRewardVector()[i];
    }
    ASSERT_EQ(158.32, rewardSum);

    ASSERT_TRUE(mdp.getRewardModel("").hasTransitionRewards());
    ASSERT_EQ(17ul, mdp.getRewardModel("").getTransitionRewardMatrix().getEntryCount());
    rewardSum = 0;
    for (uint_fast64_t i = 0; i < mdp.getRewardModel("").getTransitionRewardMatrix().getRowCount(); i++) {
        rewardSum += mdp.getRewardModel("").getTransitionRewardMatrix().getRowSum(i);
    }
    ASSERT_EQ(1376.864, rewardSum);
}

TEST(NondeterministicModelParserTest, MismatchedFiles) {
    // Test file combinations that do not match, i.e. differing number of states, transitions, etc.

    // The labeling file contains a label for a non existent state.
    STORM_SILENT_ASSERT_THROW(storm::parser::NondeterministicModelParser<>::parseMdp(STORM_TEST_RESOURCES_DIR "/tra/mdp_mismatched.tra",
                                                                                     STORM_TEST_RESOURCES_DIR "/lab/mdp_general.lab"),
                              storm::exceptions::OutOfRangeException);

    // The state reward file contains a reward for a non existent state.
    STORM_SILENT_ASSERT_THROW(storm::parser::NondeterministicModelParser<>::parseMdp(STORM_TEST_RESOURCES_DIR "/tra/mdp_mismatched.tra",
                                                                                     STORM_TEST_RESOURCES_DIR "/lab/mdp_mismatched.lab",
                                                                                     STORM_TEST_RESOURCES_DIR "/rew/mdp_general.state.rew"),
                              storm::exceptions::OutOfRangeException);

    // The transition reward file contains rewards for a non existent state.
    STORM_SILENT_ASSERT_THROW(storm::parser::NondeterministicModelParser<>::parseMdp(STORM_TEST_RESOURCES_DIR "/tra/mdp_mismatched.tra",
                                                                                     STORM_TEST_RESOURCES_DIR "/lab/mdp_mismatched.lab", "",
                                                                                     STORM_TEST_RESOURCES_DIR "/rew/mdp_general.trans.rew"),
                              storm::exceptions::OutOfRangeException);

    // The transition reward file contains rewards for a non existent transition
    STORM_SILENT_ASSERT_THROW(storm::parser::NondeterministicModelParser<>::parseMdp(STORM_TEST_RESOURCES_DIR "/tra/mdp_mismatched.tra",
                                                                                     STORM_TEST_RESOURCES_DIR "/lab/mdp_mismatched.lab", "",
                                                                                     STORM_TEST_RESOURCES_DIR "/rew/mdp_mismatched.trans.rew"),
                              storm::exceptions::OutOfRangeException);
}
