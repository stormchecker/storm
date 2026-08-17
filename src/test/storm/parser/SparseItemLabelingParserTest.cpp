#include "storm-config.h"
#include "test/storm_gtest.h"

#include <memory>

#include "storm-parsers/parser/SparseItemLabelingParser.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/models/sparse/StateLabeling.h"

TEST(SparseItemLabelingParserTest, NonExistingFile) {
    // No matter what happens, please do NOT create a file with the name "nonExistingFile.not"!
    STORM_SILENT_ASSERT_THROW(storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(0, STORM_TEST_RESOURCES_DIR "/nonExistingFile.not"),
                              storm::exceptions::FileIoException);
}

TEST(SparseItemLabelingParserTest, BasicDeterministicParsing) {
    // This test is based on a test case from the original MRMC.

    // Parsing the file
    storm::models::sparse::StateLabeling labeling =
        storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(12, STORM_TEST_RESOURCES_DIR "/lab/pctl_general.lab");

    // Checking whether all propositions are in the labelling

    char phi[] = "phi", psi[] = "psi", smth[] = "smth";

    ASSERT_TRUE(labeling.containsLabel(phi));
    ASSERT_TRUE(labeling.containsLabel(psi));
    ASSERT_TRUE(labeling.containsLabel(smth));

    // Testing whether all and only the correct nodes are labeled with "phi"
    ASSERT_TRUE(labeling.getStateHasLabel(phi, 0));
    ASSERT_TRUE(labeling.getStateHasLabel(phi, 1));
    ASSERT_TRUE(labeling.getStateHasLabel(phi, 2));

    ASSERT_FALSE(labeling.getStateHasLabel(phi, 3));
    ASSERT_FALSE(labeling.getStateHasLabel(phi, 4));
    ASSERT_FALSE(labeling.getStateHasLabel(phi, 5));
    ASSERT_FALSE(labeling.getStateHasLabel(phi, 6));
    ASSERT_FALSE(labeling.getStateHasLabel(phi, 7));
    ASSERT_FALSE(labeling.getStateHasLabel(phi, 8));
    ASSERT_FALSE(labeling.getStateHasLabel(phi, 9));
    ASSERT_FALSE(labeling.getStateHasLabel(phi, 10));
    ASSERT_FALSE(labeling.getStateHasLabel(phi, 11));

    // Testing whether all and only the correct nodes are labeled with "psi"
    ASSERT_FALSE(labeling.getStateHasLabel(psi, 0));
    ASSERT_FALSE(labeling.getStateHasLabel(psi, 1));
    ASSERT_FALSE(labeling.getStateHasLabel(psi, 2));
    ASSERT_FALSE(labeling.getStateHasLabel(psi, 3));
    ASSERT_FALSE(labeling.getStateHasLabel(psi, 4));
    ASSERT_FALSE(labeling.getStateHasLabel(psi, 5));
    ASSERT_FALSE(labeling.getStateHasLabel(psi, 6));
    ASSERT_FALSE(labeling.getStateHasLabel(psi, 7));
    ASSERT_FALSE(labeling.getStateHasLabel(psi, 8));
    ASSERT_FALSE(labeling.getStateHasLabel(psi, 9));
    ASSERT_FALSE(labeling.getStateHasLabel(psi, 10));
    ASSERT_FALSE(labeling.getStateHasLabel(psi, 11));

    // Testing whether all and only the correct nodes are labeled with "smth"
    ASSERT_TRUE(labeling.getStateHasLabel(smth, 2));

    ASSERT_FALSE(labeling.getStateHasLabel(smth, 0));
    ASSERT_FALSE(labeling.getStateHasLabel(smth, 1));
    ASSERT_FALSE(labeling.getStateHasLabel(smth, 3));
    ASSERT_FALSE(labeling.getStateHasLabel(smth, 4));
    ASSERT_FALSE(labeling.getStateHasLabel(smth, 5));
    ASSERT_FALSE(labeling.getStateHasLabel(smth, 6));
    ASSERT_FALSE(labeling.getStateHasLabel(smth, 7));
    ASSERT_FALSE(labeling.getStateHasLabel(smth, 8));
    ASSERT_FALSE(labeling.getStateHasLabel(smth, 9));
    ASSERT_FALSE(labeling.getStateHasLabel(smth, 10));
    ASSERT_FALSE(labeling.getStateHasLabel(smth, 11));
}

TEST(SparseItemLabelingParserTest, BasicNondeterministicParsing) {
    std::vector<uint_fast64_t> choiceIndices{0, 3, 5, 6, 9, 11};

    // Parsing the file
    storm::models::sparse::ChoiceLabeling labeling =
        storm::parser::SparseItemLabelingParser::parseChoiceLabeling(11, STORM_TEST_RESOURCES_DIR "/lab/nondet.choicelab", choiceIndices);

    // Checking whether the parsed labeling is correct
    ASSERT_TRUE(labeling.containsLabel("alpha"));
    EXPECT_EQ(2ull, labeling.getChoices("alpha").getNumberOfSetBits());
    EXPECT_TRUE(labeling.getChoiceHasLabel("alpha", 0));
    EXPECT_TRUE(labeling.getChoiceHasLabel("alpha", 1));

    ASSERT_TRUE(labeling.containsLabel("beta"));
    EXPECT_EQ(3ull, labeling.getChoices("beta").getNumberOfSetBits());
    EXPECT_TRUE(labeling.getChoiceHasLabel("beta", 1));
    EXPECT_TRUE(labeling.getChoiceHasLabel("beta", 3));
    EXPECT_TRUE(labeling.getChoiceHasLabel("beta", 8));

    ASSERT_TRUE(labeling.containsLabel("gamma"));
    EXPECT_EQ(1ull, labeling.getChoices("gamma").getNumberOfSetBits());
    EXPECT_TRUE(labeling.getChoiceHasLabel("gamma", 2));

    ASSERT_TRUE(labeling.containsLabel("delta"));
    EXPECT_TRUE(labeling.getChoices("delta").empty());
}

TEST(SparseItemLabelingParserTest, NoDeclarationTagHeader) {
    // No #DECLARATION tag in file
    STORM_SILENT_ASSERT_THROW(storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(3, STORM_TEST_RESOURCES_DIR "/lab/noDeclarationTag.lab"),
                              storm::exceptions::WrongFormatException);
}

TEST(SparseItemLabelingParserTest, NoEndTagHeader) {
    // No #END tag in file.
    STORM_SILENT_ASSERT_THROW(storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(3, STORM_TEST_RESOURCES_DIR "/lab/noEndTag.lab"),
                              storm::exceptions::WrongFormatException);
}

TEST(SparseItemLabelingParserTest, MisspelledDeclarationTagHeader) {
    // The #DECLARATION tag is misspelled.
    STORM_SILENT_ASSERT_THROW(
        storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(3, STORM_TEST_RESOURCES_DIR "/lab/declarationMisspell.lab"),
        storm::exceptions::WrongFormatException);
}

TEST(SparseItemLabelingParserTest, MisspelledEndTagHeader) {
    // The #END tag is misspelled.
    STORM_SILENT_ASSERT_THROW(storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(3, STORM_TEST_RESOURCES_DIR "/lab/endMisspell.lab"),
                              storm::exceptions::WrongFormatException);
}

TEST(SparseItemLabelingParserTest, NoLabelDeclaredNoneGiven) {
    // No label between #DECLARATION and #END and no labels given.
    storm::models::sparse::StateLabeling labeling =
        storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(13, STORM_TEST_RESOURCES_DIR "/lab/noLabelsDecNoneGiven.lab");
    ASSERT_EQ(0ul, labeling.getNumberOfLabels());
    for (uint_fast64_t i = 0; i < 13; i++) {
        ASSERT_TRUE(labeling.getLabelsOfState(i).empty());
    }
}

TEST(SparseItemLabelingParserTest, UndeclaredLabelsGiven) {
    // Undeclared labels given.
    STORM_SILENT_ASSERT_THROW(
        storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(3, STORM_TEST_RESOURCES_DIR "/lab/undeclaredLabelsGiven.lab"),
        storm::exceptions::WrongFormatException);
}

TEST(SparseItemLabelingParserTest, LabelForNonExistentState) {
    // The index of one of the state that are to be labeled is higher than the number of states in the model.
    STORM_SILENT_ASSERT_THROW(
        storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(3, STORM_TEST_RESOURCES_DIR "/lab/labelForNonexistentState.lab"),
        storm::exceptions::OutOfRangeException);
}

// Note: As implemented at the moment, each label given for a state in any line is set to true for that state (logical or over all lines for that state).
// This behavior might not be ideal as multiple lines for one state are not necessary and might indicate a corrupt or wrong file.
TEST(SparseItemLabelingParserTest, DoubledLines) {
    // There are multiple lines attributing labels to the same state.
    STORM_SILENT_ASSERT_THROW(storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(6, STORM_TEST_RESOURCES_DIR "/lab/doubledLines.lab"),
                              storm::exceptions::WrongFormatException);

    // There is a line for a state that has been skipped.
    STORM_SILENT_ASSERT_THROW(
        storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(6, STORM_TEST_RESOURCES_DIR "/lab/doubledLinesSkipped.lab"),
        storm::exceptions::WrongFormatException);
}

TEST(SparseItemLabelingParserTest, WrongProposition) {
    // Swapped the state index and the label at one entry.
    STORM_SILENT_ASSERT_THROW(
        storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(3, STORM_TEST_RESOURCES_DIR "/lab/swappedStateAndProposition.lab"),
        storm::exceptions::WrongFormatException);
}

TEST(SparseItemLabelingParserTest, Whitespaces) {
    // Different configurations of allowed whitespaces are tested.

    // First parse the labeling file without added whitespaces and obtain the hash of its parsed representation.
    storm::models::sparse::StateLabeling labeling =
        storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(13, STORM_TEST_RESOURCES_DIR "/lab/withoutWhitespaces.lab");

    // Now parse the labeling file with the added whitespaces and compare the hashes.
    storm::models::sparse::StateLabeling labeling2 =
        storm::parser::SparseItemLabelingParser::parseAtomicPropositionLabeling(13, STORM_TEST_RESOURCES_DIR "/lab/withWhitespaces.lab");
    ASSERT_TRUE(labeling == labeling2);
}
