#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/ExplicitModelParserOptions.h"
#include "storm-parsers/parser/NondeterministicSparseTransitionParser.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/storage/SparseMatrix.h"

TEST(NondeterministicSparseTransitionParserTest, NonExistingFile) {
    // No matter what happens, please do NOT create a file with the name "nonExistingFile.not"!
    STORM_SILENT_ASSERT_THROW(
        storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitions(STORM_TEST_RESOURCES_DIR "/nonExistingFile.not"),
        storm::exceptions::FileIoException);

    storm::storage::SparseMatrix<double> nullInformation;
    STORM_SILENT_ASSERT_THROW(storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitionRewards(
                                  STORM_TEST_RESOURCES_DIR "/nonExistingFile.not", nullInformation),
                              storm::exceptions::FileIoException);
}

TEST(NondeterministicSparseTransitionParserTest, BasicTransitionsParsing) {
    // Parse a nondeterministic transitions file and test the result.
    storm::storage::SparseMatrix<double> result(
        storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitions(STORM_TEST_RESOURCES_DIR "/tra/mdp_general.tra"));

    // Test the row mapping, i.e. at which row which state starts.
    ASSERT_EQ(6ul, result.getRowGroupCount());
    ASSERT_EQ(7ul, result.getRowGroupIndices().size());
    ASSERT_EQ(0ul, result.getRowGroupIndices()[0]);
    ASSERT_EQ(4ul, result.getRowGroupIndices()[1]);
    ASSERT_EQ(5ul, result.getRowGroupIndices()[2]);
    ASSERT_EQ(7ul, result.getRowGroupIndices()[3]);
    ASSERT_EQ(8ul, result.getRowGroupIndices()[4]);
    ASSERT_EQ(9ul, result.getRowGroupIndices()[5]);
    ASSERT_EQ(11ul, result.getRowGroupIndices()[6]);

    // Test the transition matrix.
    ASSERT_EQ(6ul, result.getColumnCount());
    ASSERT_EQ(11ul, result.getRowCount());
    ASSERT_EQ(22ul, result.getEntryCount());

    // Test every entry of the matrix.
    storm::storage::SparseMatrix<double>::const_iterator cIter = result.begin(0);

    ASSERT_EQ(0ul, cIter->getColumn());
    ASSERT_EQ(0.9, cIter->getValue());
    cIter++;
    ASSERT_EQ(1ul, cIter->getColumn());
    ASSERT_EQ(0.1, cIter->getValue());
    cIter++;
    ASSERT_EQ(1ul, cIter->getColumn());
    ASSERT_EQ(0.2, cIter->getValue());
    cIter++;
    ASSERT_EQ(2ul, cIter->getColumn());
    ASSERT_EQ(0.2, cIter->getValue());
    cIter++;
    ASSERT_EQ(3ul, cIter->getColumn());
    ASSERT_EQ(0.2, cIter->getValue());
    cIter++;
    ASSERT_EQ(4ul, cIter->getColumn());
    ASSERT_EQ(0.2, cIter->getValue());
    cIter++;
    ASSERT_EQ(5ul, cIter->getColumn());
    ASSERT_EQ(0.2, cIter->getValue());
    cIter++;
    ASSERT_EQ(5ul, cIter->getColumn());
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(0ul, cIter->getColumn());
    ASSERT_EQ(0.1, cIter->getValue());
    cIter++;
    ASSERT_EQ(4ul, cIter->getColumn());
    ASSERT_EQ(0.9, cIter->getValue());
    cIter++;
    ASSERT_EQ(2ul, cIter->getColumn());
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(2ul, cIter->getColumn());
    ASSERT_EQ(0.5, cIter->getValue());
    cIter++;
    ASSERT_EQ(3ul, cIter->getColumn());
    ASSERT_EQ(0.5, cIter->getValue());
    cIter++;
    ASSERT_EQ(2ul, cIter->getColumn());
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(2ul, cIter->getColumn());
    ASSERT_EQ(0.001, cIter->getValue());
    cIter++;
    ASSERT_EQ(3ul, cIter->getColumn());
    ASSERT_EQ(0.999, cIter->getValue());
    cIter++;
    ASSERT_EQ(1ul, cIter->getColumn());
    ASSERT_EQ(0.7, cIter->getValue());
    cIter++;
    ASSERT_EQ(4ul, cIter->getColumn());
    ASSERT_EQ(0.3, cIter->getValue());
    cIter++;
    ASSERT_EQ(1ul, cIter->getColumn());
    ASSERT_EQ(0.2, cIter->getValue());
    cIter++;
    ASSERT_EQ(4ul, cIter->getColumn());
    ASSERT_EQ(0.2, cIter->getValue());
    cIter++;
    ASSERT_EQ(5ul, cIter->getColumn());
    ASSERT_EQ(0.6, cIter->getValue());
    cIter++;
    ASSERT_EQ(5ul, cIter->getColumn());
    ASSERT_EQ(1, cIter->getValue());
}

TEST(NondeterministicSparseTransitionParserTest, BasicTransitionsRewardsParsing) {
    // Parse a nondeterministic transitions file and test the result.
    storm::storage::SparseMatrix<double> modelInformation(
        storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitions(STORM_TEST_RESOURCES_DIR "/tra/mdp_general.tra"));
    storm::storage::SparseMatrix<double> result(storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitionRewards(
        STORM_TEST_RESOURCES_DIR "/rew/mdp_general.trans.rew", modelInformation));

    // Test the transition matrix.
    ASSERT_EQ(6ul, result.getColumnCount());
    ASSERT_EQ(11ul, result.getRowCount());
    ASSERT_EQ(17ul, result.getEntryCount());

    // Test every entry of the matrix.
    storm::storage::SparseMatrix<double>::const_iterator cIter = result.begin(0);

    ASSERT_EQ(0ul, cIter->getColumn());
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(1ul, cIter->getColumn());
    ASSERT_EQ(30, cIter->getValue());
    cIter++;
    ASSERT_EQ(1ul, cIter->getColumn());
    ASSERT_EQ(15.2, cIter->getValue());
    cIter++;
    ASSERT_EQ(2ul, cIter->getColumn());
    ASSERT_EQ(75, cIter->getValue());
    cIter++;
    ASSERT_EQ(5ul, cIter->getColumn());
    ASSERT_EQ(2.45, cIter->getValue());
    cIter++;
    ASSERT_EQ(5ul, cIter->getColumn());
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(0ul, cIter->getColumn());
    ASSERT_EQ(0.114, cIter->getValue());
    cIter++;
    ASSERT_EQ(4ul, cIter->getColumn());
    ASSERT_EQ(90, cIter->getValue());
    cIter++;
    ASSERT_EQ(2ul, cIter->getColumn());
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(2ul, cIter->getColumn());
    ASSERT_EQ(55, cIter->getValue());
    cIter++;
    ASSERT_EQ(3ul, cIter->getColumn());
    ASSERT_EQ(87, cIter->getValue());
    cIter++;
    ASSERT_EQ(2ul, cIter->getColumn());
    ASSERT_EQ(13, cIter->getValue());
    cIter++;
    ASSERT_EQ(3ul, cIter->getColumn());
    ASSERT_EQ(999, cIter->getValue());
    cIter++;
    ASSERT_EQ(1ul, cIter->getColumn());
    ASSERT_EQ(0.7, cIter->getValue());
    cIter++;
    ASSERT_EQ(4ul, cIter->getColumn());
    ASSERT_EQ(0.3, cIter->getValue());
    cIter++;
    ASSERT_EQ(1ul, cIter->getColumn());
    ASSERT_EQ(0.1, cIter->getValue());
    cIter++;
    ASSERT_EQ(5ul, cIter->getColumn());
    ASSERT_EQ(6, cIter->getValue());
}

TEST(NondeterministicSparseTransitionParserTest, Whitespaces) {
    // Test the resilience of the parser against whitespaces.
    // Do so by comparing the hashes of the transition matices and the rowMapping vectors element by element.
    storm::storage::SparseMatrix<double> correctResult(
        storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitions(STORM_TEST_RESOURCES_DIR "/tra/mdp_general.tra"));
    storm::storage::SparseMatrix<double> whitespaceResult =
        storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitions(STORM_TEST_RESOURCES_DIR "/tra/mdp_whitespaces.tra");
    ASSERT_EQ(correctResult.hash(), whitespaceResult.hash());
    ASSERT_EQ(correctResult.getRowGroupIndices().size(), whitespaceResult.getRowGroupIndices().size());
    for (uint_fast64_t i = 0; i < correctResult.getRowGroupIndices().size(); i++) {
        ASSERT_EQ(correctResult.getRowGroupIndices()[i], whitespaceResult.getRowGroupIndices()[i]);
    }

    // Do the same (minus the unused rowMapping) for the corresponding transition rewards file (with and without whitespaces)
    uint_fast64_t correctHash = storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitionRewards(
                                    STORM_TEST_RESOURCES_DIR "/rew/mdp_general.trans.rew", correctResult)
                                    .hash();
    ASSERT_EQ(correctHash, storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitionRewards(
                               STORM_TEST_RESOURCES_DIR "/rew/mdp_whitespaces.trans.rew", whitespaceResult)
                               .hash());
}

TEST(NondeterministicSparseTransitionParserTest, MixedTransitionOrder) {
    // Since the MatrixBuilder needs sequential input of new elements reordering of transitions or states should throw an exception.
    STORM_SILENT_ASSERT_THROW(
        storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitions(STORM_TEST_RESOURCES_DIR "/tra/mdp_mixedStateOrder.tra"),
        storm::exceptions::InvalidArgumentException);

    storm::storage::SparseMatrix<double> modelInformation =
        storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitions(STORM_TEST_RESOURCES_DIR "/tra/mdp_general.tra");
    STORM_SILENT_ASSERT_THROW(storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitionRewards(
                                  STORM_TEST_RESOURCES_DIR "/rew/mdp_mixedStateOrder.trans.rew", modelInformation),
                              storm::exceptions::InvalidArgumentException);
}

TEST(NondeterministicSparseTransitionParserTest, FixDeadlocks) {
    storm::parser::ExplicitModelParserOptions options;
    options.fixDeadlocks = true;

    // Parse a transitions file with the fixDeadlocks flag set and test if it works.
    storm::storage::SparseMatrix<double> result(
        storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitions(STORM_TEST_RESOURCES_DIR "/tra/mdp_deadlock.tra", options));

    ASSERT_EQ(8ul, result.getRowGroupIndices().size());
    ASSERT_EQ(9ul, result.getRowGroupIndices()[5]);
    ASSERT_EQ(10ul, result.getRowGroupIndices()[6]);
    ASSERT_EQ(12ul, result.getRowGroupIndices()[7]);

    ASSERT_EQ(7ul, result.getColumnCount());
    ASSERT_EQ(12ul, result.getRowCount());
    ASSERT_EQ(23ul, result.getEntryCount());

    storm::storage::SparseMatrix<double>::const_iterator cIter = result.begin(8);

    ASSERT_EQ(1ul, cIter->getColumn());
    ASSERT_EQ(0.7, cIter->getValue());
    cIter++;
    ASSERT_EQ(4ul, cIter->getColumn());
    ASSERT_EQ(0.3, cIter->getValue());
    cIter++;
    ASSERT_EQ(5ul, cIter->getColumn());
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(1ul, cIter->getColumn());
    ASSERT_EQ(0.2, cIter->getValue());
    cIter++;
    ASSERT_EQ(4ul, cIter->getColumn());
    ASSERT_EQ(0.2, cIter->getValue());
    cIter++;
    ASSERT_EQ(5ul, cIter->getColumn());
    ASSERT_EQ(0.6, cIter->getValue());
    cIter++;
    ASSERT_EQ(5ul, cIter->getColumn());
    ASSERT_EQ(1, cIter->getValue());
}

TEST(NondeterministicSparseTransitionParserTest, DontFixDeadlocks) {
    // Try to parse a transitions file containing a deadlock state with the fixDeadlocks flag unset. This should throw an exception.
    storm::parser::ExplicitModelParserOptions options;
    options.fixDeadlocks = false;

    STORM_SILENT_ASSERT_THROW(
        storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitions(STORM_TEST_RESOURCES_DIR "/tra/mdp_deadlock.tra", options),
        storm::exceptions::WrongFormatException);
}

TEST(NondeterministicSparseTransitionParserTest, DoubledLines) {
    // There is a redundant line in the transition file. As the transition already exists this should throw an exception.
    STORM_SILENT_ASSERT_THROW(
        storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitions(STORM_TEST_RESOURCES_DIR "/tra/mdp_doubledLines.tra"),
        storm::exceptions::InvalidArgumentException);
}

TEST(NondeterministicSparseTransitionParserTest, RewardForNonExistentTransition) {
    // First parse a transition file. Then parse a transition reward file for the resulting transition matrix.
    storm::storage::SparseMatrix<double> transitionResult =
        storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitions(STORM_TEST_RESOURCES_DIR "/tra/mdp_general.tra");

    // There is a reward for a transition that does not exist in the transition matrix.
    STORM_SILENT_ASSERT_THROW(storm::parser::NondeterministicSparseTransitionParser<>::parseNondeterministicTransitionRewards(
                                  STORM_TEST_RESOURCES_DIR "/rew/mdp_rewardForNonExTrans.trans.rew", transitionResult),
                              storm::exceptions::WrongFormatException);
}
