#include "storm-config.h"
#include "test/storm_gtest.h"

#include <vector>

#include "storm-parsers/parser/ExplicitModelParserOptions.h"
#include "storm-parsers/parser/MarkovAutomatonParser.h"
#include "storm-parsers/parser/MarkovAutomatonSparseTransitionParser.h"
#include "storm-parsers/util/cstring.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/WrongFormatException.h"

TEST(MarkovAutomatonSparseTransitionParserTest, NonExistingFile) {
    // No matter what happens, please do NOT create a file with the name "nonExistingFile.not"!
    STORM_SILENT_ASSERT_THROW(
        storm::parser::MarkovAutomatonSparseTransitionParser<>::parseMarkovAutomatonTransitions(STORM_TEST_RESOURCES_DIR "/nonExistingFile.not"),
        storm::exceptions::FileIoException);
}

TEST(MarkovAutomatonSparseTransitionParserTest, BasicParsing) {
    // The file that will be used for the test.
    std::string filename = STORM_TEST_RESOURCES_DIR "/tra/ma_general.tra";

    // Execute the parser.
    typename storm::parser::MarkovAutomatonSparseTransitionParser<>::Result result =
        storm::parser::MarkovAutomatonSparseTransitionParser<>::parseMarkovAutomatonTransitions(filename);

    // Build the actual transition matrix.
    storm::storage::SparseMatrix<double> transitionMatrix(result.transitionMatrixBuilder.build(0, 0));

    // Test all sizes and counts.
    ASSERT_EQ(6ul, transitionMatrix.getColumnCount());
    ASSERT_EQ(7ul, transitionMatrix.getRowCount());
    ASSERT_EQ(12ul, transitionMatrix.getEntryCount());
    ASSERT_EQ(6ul, transitionMatrix.getRowGroupCount());
    ASSERT_EQ(7ul, transitionMatrix.getRowGroupIndices().size());
    ASSERT_EQ(7ul, result.markovianChoices.size());
    ASSERT_EQ(6ul, result.markovianStates.size());
    ASSERT_EQ(2ul, result.markovianStates.getNumberOfSetBits());
    ASSERT_EQ(6ul, result.exitRates.size());

    // Test the general structure of the transition system (that will be an Markov automaton).

    // Test the mapping between states and transition matrix rows.
    ASSERT_EQ(0ul, transitionMatrix.getRowGroupIndices()[0]);
    ASSERT_EQ(1ul, transitionMatrix.getRowGroupIndices()[1]);
    ASSERT_EQ(2ul, transitionMatrix.getRowGroupIndices()[2]);
    ASSERT_EQ(3ul, transitionMatrix.getRowGroupIndices()[3]);
    ASSERT_EQ(4ul, transitionMatrix.getRowGroupIndices()[4]);
    ASSERT_EQ(6ul, transitionMatrix.getRowGroupIndices()[5]);
    ASSERT_EQ(7ul, transitionMatrix.getRowGroupIndices()[6]);

    // Test the Markovian states.
    ASSERT_TRUE(result.markovianStates.get(0));
    ASSERT_FALSE(result.markovianStates.get(1));
    ASSERT_TRUE(result.markovianStates.get(2));
    ASSERT_FALSE(result.markovianStates.get(3));
    ASSERT_FALSE(result.markovianStates.get(4));
    ASSERT_FALSE(result.markovianStates.get(5));

    // Test the exit rates. These have to be 0 for all non-Markovian states.
    ASSERT_EQ(2, result.exitRates[0]);
    ASSERT_EQ(0, result.exitRates[1]);
    ASSERT_EQ(15, result.exitRates[2]);
    ASSERT_EQ(0, result.exitRates[3]);
    ASSERT_EQ(0, result.exitRates[4]);
    ASSERT_EQ(0, result.exitRates[5]);

    // Finally, test the transition matrix itself.
    storm::storage::SparseMatrix<double>::const_iterator cIter = transitionMatrix.begin(0);

    ASSERT_EQ(2, cIter->getValue());
    cIter++;
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(2, cIter->getValue());
    cIter++;
    ASSERT_EQ(4, cIter->getValue());
    cIter++;
    ASSERT_EQ(8, cIter->getValue());
    cIter++;
    ASSERT_EQ(0.5, cIter->getValue());
    cIter++;
    ASSERT_EQ(0.5, cIter->getValue());
    cIter++;
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(0.5, cIter->getValue());
    cIter++;
    ASSERT_EQ(0.5, cIter->getValue());
    cIter++;
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(transitionMatrix.end(), cIter);
}

TEST(MarkovAutomatonSparseTransitionParserTest, Whitespaces) {
    // The file that will be used for the test.
    std::string filename = STORM_TEST_RESOURCES_DIR "/tra/ma_whitespaces.tra";

    // Execute the parser.
    typename storm::parser::MarkovAutomatonSparseTransitionParser<>::Result result =
        storm::parser::MarkovAutomatonSparseTransitionParser<>::parseMarkovAutomatonTransitions(filename);

    // Build the actual transition matrix.
    storm::storage::SparseMatrix<double> transitionMatrix(result.transitionMatrixBuilder.build());

    // Test all sizes and counts.
    ASSERT_EQ(6ul, transitionMatrix.getColumnCount());
    ASSERT_EQ(7ul, transitionMatrix.getRowCount());
    ASSERT_EQ(12ul, transitionMatrix.getEntryCount());
    ASSERT_EQ(6ul, transitionMatrix.getRowGroupCount());
    ASSERT_EQ(7ul, transitionMatrix.getRowGroupIndices().size());
    ASSERT_EQ(7ul, result.markovianChoices.size());
    ASSERT_EQ(6ul, result.markovianStates.size());
    ASSERT_EQ(2ul, result.markovianStates.getNumberOfSetBits());
    ASSERT_EQ(6ul, result.exitRates.size());

    // Test the general structure of the transition system (that will be an Markov automaton).

    // Test the mapping between states and transition matrix rows.
    ASSERT_EQ(0ul, transitionMatrix.getRowGroupIndices()[0]);
    ASSERT_EQ(1ul, transitionMatrix.getRowGroupIndices()[1]);
    ASSERT_EQ(2ul, transitionMatrix.getRowGroupIndices()[2]);
    ASSERT_EQ(3ul, transitionMatrix.getRowGroupIndices()[3]);
    ASSERT_EQ(4ul, transitionMatrix.getRowGroupIndices()[4]);
    ASSERT_EQ(6ul, transitionMatrix.getRowGroupIndices()[5]);
    ASSERT_EQ(7ul, transitionMatrix.getRowGroupIndices()[6]);

    // Test the Markovian states.
    ASSERT_TRUE(result.markovianStates.get(0));
    ASSERT_FALSE(result.markovianStates.get(1));
    ASSERT_TRUE(result.markovianStates.get(2));
    ASSERT_FALSE(result.markovianStates.get(3));
    ASSERT_FALSE(result.markovianStates.get(4));
    ASSERT_FALSE(result.markovianStates.get(5));

    // Test the exit rates. These have to be 0 for all non-Markovian states.
    ASSERT_EQ(2, result.exitRates[0]);
    ASSERT_EQ(0, result.exitRates[1]);
    ASSERT_EQ(15, result.exitRates[2]);
    ASSERT_EQ(0, result.exitRates[3]);
    ASSERT_EQ(0, result.exitRates[4]);
    ASSERT_EQ(0, result.exitRates[5]);

    // Finally, test the transition matrix itself.
    storm::storage::SparseMatrix<double>::const_iterator cIter = transitionMatrix.begin(0);

    ASSERT_EQ(2, cIter->getValue());
    cIter++;
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(2, cIter->getValue());
    cIter++;
    ASSERT_EQ(4, cIter->getValue());
    cIter++;
    ASSERT_EQ(8, cIter->getValue());
    cIter++;
    ASSERT_EQ(0.5, cIter->getValue());
    cIter++;
    ASSERT_EQ(0.5, cIter->getValue());
    cIter++;
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(0.5, cIter->getValue());
    cIter++;
    ASSERT_EQ(0.5, cIter->getValue());
    cIter++;
    ASSERT_EQ(1, cIter->getValue());
    cIter++;
    ASSERT_EQ(transitionMatrix.end(), cIter);
}

TEST(MarkovAutomatonSparseTransitionParserTest, FixDeadlocks) {
    storm::parser::ExplicitModelParserOptions options;
    options.fixDeadlocks = true;

    // Parse a Markov Automaton transition file with the fixDeadlocks flag set and test if it works.
    typename storm::parser::MarkovAutomatonSparseTransitionParser<>::Result result =
        storm::parser::MarkovAutomatonSparseTransitionParser<>::parseMarkovAutomatonTransitions(STORM_TEST_RESOURCES_DIR "/tra/ma_deadlock.tra", options);

    // Test if the result is consistent with the parsed Markov Automaton.
    storm::storage::SparseMatrix<double> resultMatrix(result.transitionMatrixBuilder.build());
    ASSERT_EQ(7ul, resultMatrix.getColumnCount());
    ASSERT_EQ(13ul, resultMatrix.getEntryCount());
    ASSERT_EQ(7ul, resultMatrix.getRowGroupCount());
    ASSERT_EQ(8ul, resultMatrix.getRowGroupIndices().size());
    ASSERT_EQ(8ul, result.markovianChoices.size());
    ASSERT_EQ(7ul, result.markovianStates.size());
    ASSERT_EQ(2ul, result.markovianStates.getNumberOfSetBits());
    ASSERT_EQ(7ul, result.exitRates.size());
}

TEST(MarkovAutomatonSparseTransitionParserTest, DontFixDeadlocks) {
    // Try to parse a Markov Automaton transition file containing a deadlock state with the fixDeadlocks flag unset. This should throw an exception.
    storm::parser::ExplicitModelParserOptions options;
    options.fixDeadlocks = false;

    STORM_SILENT_ASSERT_THROW(
        storm::parser::MarkovAutomatonSparseTransitionParser<>::parseMarkovAutomatonTransitions(STORM_TEST_RESOURCES_DIR "/tra/ma_deadlock.tra", options),
        storm::exceptions::WrongFormatException);
}
