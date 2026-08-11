#include "storm-parsers/parser/SparseItemLabelingParser.h"

#include <cstring>
#include <iostream>
#include <string>

#include "storm-parsers/parser/MappedFile.h"
#include "storm-parsers/util/cstring.h"

#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {

using namespace storm::utility::cstring;

storm::models::sparse::StateLabeling SparseItemLabelingParser::parseAtomicPropositionLabeling(uint_fast64_t stateCount, std::string const& filename) {
    MappedFile file(filename.c_str());
    checkSyntax(filename, file);

    // Create labeling object with given node and label count.
    storm::models::sparse::StateLabeling labeling(stateCount);

    // initialize the buffer
    char const* buf = file.getData();

    // add the labels to the labeling
    parseLabelNames(filename, labeling, buf);

    // Now parse the assignments of labels to states.
    parseDeterministicLabelAssignments(filename, labeling, buf);

    return labeling;
}

storm::models::sparse::ChoiceLabeling SparseItemLabelingParser::parseChoiceLabeling(
    uint_fast64_t choiceCount, std::string const& filename, boost::optional<std::vector<uint_fast64_t>> const& nondeterministicChoiceIndices) {
    MappedFile file(filename.c_str());
    checkSyntax(filename, file);

    // Create labeling object with given node and label count.
    storm::models::sparse::ChoiceLabeling labeling(choiceCount);

    // initialize the buffer
    char const* buf = file.getData();

    // add the labels to the labeling
    parseLabelNames(filename, labeling, buf);

    // Now parse the assignments of labels to states.
    if (nondeterministicChoiceIndices) {
        parseNonDeterministicLabelAssignments(filename, labeling, nondeterministicChoiceIndices.get(), buf);
    } else {
        parseDeterministicLabelAssignments(filename, labeling, buf);
    }

    return labeling;
}

void SparseItemLabelingParser::checkSyntax(std::string const& filename, storm::parser::MappedFile const& file) {
    char const* buf = file.getData();

    // First pass: Count the number of propositions.
    bool foundDecl = false, foundEnd = false;
    size_t cnt = 0;

    // Iterate over tokens until we hit #END or the end of the file.
    while (buf[0] != '\0') {
        // Move the buffer to the beginning of the next word.
        buf += cnt;
        buf = trimWhitespaces(buf);

        // Get the number of characters until the next separator.
        cnt = skipWord(buf) - buf;
        if (cnt > 0) {
            // If the next token is #DECLARATION: Just skip it.
            // If the next token is #END: Stop the search.
            // Otherwise increase proposition_count.
            if (strncmp(buf, "#DECLARATION", cnt) == 0) {
                foundDecl = true;
                continue;
            } else if (strncmp(buf, "#END", cnt) == 0) {
                foundEnd = true;
                break;
            }
        }
    }

    // If #DECLARATION or #END have not been found, the file format is wrong.
    STORM_LOG_THROW(foundDecl, storm::exceptions::WrongFormatException,
                    "Error while parsing " << filename << ": File header is corrupted (Token #DECLARATION missing - case sensitive).");
    STORM_LOG_THROW(foundEnd, storm::exceptions::WrongFormatException,
                    "Error while parsing " << filename << ": File header is corrupted (Token #END missing - case sensitive).");
}

void SparseItemLabelingParser::parseLabelNames(std::string const& filename, storm::models::sparse::ItemLabeling& labeling, char const*& buf) {
    // Prepare a buffer for proposition names.
    char proposition[128];
    size_t cnt = 0;

    // Parse proposition names.
    // As we already checked the file header, we know that #DECLARATION and #END are tokens in the character stream.
    while (buf[0] != '\0') {
        // Move the buffer to the beginning of the next word.
        buf += cnt;
        buf = trimWhitespaces(buf);

        // Get the number of characters until the next separator.
        cnt = skipWord(buf) - buf;

        if (cnt >= sizeof(proposition)) {
            // if token is longer than our buffer, the following strncpy code might get risky...
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "Error while parsing " << filename << ": Atomic proposition with length > " << (sizeof(proposition) - 1) << " was found.");

        } else if (cnt > 0) {
            // If the next token is #DECLARATION: Just skip it.
            if (strncmp(buf, "#DECLARATION", cnt) == 0)
                continue;

            // If the next token is #END: Stop the search.
            if (strncmp(buf, "#END", cnt) == 0)
                break;

            // Otherwise copy the token to the buffer, append a trailing null byte and hand it to labeling.
            strncpy(proposition, buf, cnt);
            proposition[cnt] = '\0';
            labeling.addLabel(proposition);
        }
    }

    // At this point, the pointer buf is still pointing to our last token, i.e. to #END.
    // We want to skip it.
    buf += 4;

    // Now eliminate remaining whitespaces such as empty lines and start parsing.
    buf = trimWhitespaces(buf);
}

void SparseItemLabelingParser::parseDeterministicLabelAssignments(std::string const& filename, storm::models::sparse::ItemLabeling& labeling,
                                                                  char const*& buf) {
    uint_fast64_t state = 0;
    uint_fast64_t lastState = (uint_fast64_t)-1;
    uint_fast64_t const startIndexComparison = lastState;
    size_t cnt = 0;
    char proposition[128];

    while (buf[0] != '\0') {
        // Parse the state number and iterate over its labels (atomic propositions).
        // Stop at the end of the line.
        state = checked_strtol(buf, &buf);

        // If the state has already been read or skipped once there might be a problem with the file (doubled lines, or blocks).
        STORM_LOG_THROW(state > lastState || lastState == startIndexComparison, storm::exceptions::WrongFormatException,
                        "Error while parsing " << filename << ": State " << state << " was found but has already been read or skipped previously.");

        while ((buf[0] != '\r') && (buf[0] != '\n') && (buf[0] != '\0')) {
            cnt = skipWord(buf) - buf;
            if (cnt == 0) {
                // The next character is a separator.
                // If it is a line separator, we continue with next node.
                // Otherwise, we skip it and try again.
                if (buf[0] == '\n' || buf[0] == '\r')
                    break;
                buf++;
            } else {
                // Copy the label to the buffer, null terminate it and add it to labeling.
                strncpy(proposition, buf, cnt);
                proposition[cnt] = '\0';

                // Has the label been declared in the header?
                STORM_LOG_THROW(labeling.containsLabel(proposition), storm::exceptions::WrongFormatException,
                                "Error while parsing " << filename << ": Atomic proposition" << proposition << " was found but not declared.");
                if (labeling.isStateLabeling()) {
                    labeling.asStateLabeling().addLabelToState(proposition, state);
                } else {
                    STORM_LOG_ASSERT(labeling.isChoiceLabeling(), "Unexpected labeling type.");
                    labeling.asChoiceLabeling().addLabelToChoice(proposition, state);
                }
                buf += cnt;
            }
        }
        buf = trimWhitespaces(buf);
        lastState = state;
    }
}

void SparseItemLabelingParser::parseNonDeterministicLabelAssignments(std::string const& filename, storm::models::sparse::ChoiceLabeling& labeling,
                                                                     std::vector<uint_fast64_t> const& nondeterministicChoiceIndices, char const*& buf) {
    uint_fast64_t const startIndexComparison = (uint_fast64_t)-1;
    uint_fast64_t state = 0;
    uint_fast64_t lastState = startIndexComparison;
    uint_fast64_t localChoice = 0;
    uint_fast64_t lastLocalChoice = startIndexComparison;
    size_t cnt = 0;
    char proposition[128];

    while (buf[0] != '\0') {
        // Parse the state and choice number and iterate over its labels (atomic propositions).
        // Stop at the end of the line.
        state = checked_strtol(buf, &buf);
        localChoice = checked_strtol(buf, &buf);

        // If we see this state for the first time, reset the last choice
        if (state != lastState) {
            lastLocalChoice = startIndexComparison;
        }

        // If the state-choice pair has already been read or skipped once there might be a problem with the file (doubled lines, or blocks).
        STORM_LOG_THROW(state >= lastState || lastState == startIndexComparison, storm::exceptions::WrongFormatException,
                        "Error while parsing " << filename << ": State " << state << " and Choice " << localChoice
                                               << " were found but the state has already been read or skipped previously.");
        STORM_LOG_THROW(state != lastState || localChoice > lastLocalChoice || lastLocalChoice == startIndexComparison, storm::exceptions::WrongFormatException,
                        "Error while parsing " << filename << ": State " << state << " and Choice " << localChoice
                                               << " were found but the choice has already been read or skipped previously.");

        uint_fast64_t const globalChoice = nondeterministicChoiceIndices[state] + localChoice;
        STORM_LOG_THROW(globalChoice < nondeterministicChoiceIndices[state + 1], storm::exceptions::WrongFormatException,
                        "Error while parsing " << filename << ": Invalid choice index " << localChoice << " at state " << state << ".");

        while ((buf[0] != '\r') && (buf[0] != '\n') && (buf[0] != '\0')) {
            cnt = skipWord(buf) - buf;
            if (cnt == 0) {
                // The next character is a separator.
                // If it is a line separator, we continue with next node.
                // Otherwise, we skip it and try again.
                if (buf[0] == '\n' || buf[0] == '\r')
                    break;
                buf++;
            } else {
                // Copy the label to the buffer, null terminate it and add it to labeling.
                strncpy(proposition, buf, cnt);
                proposition[cnt] = '\0';

                // Has the label been declared in the header?
                STORM_LOG_THROW(labeling.containsLabel(proposition), storm::exceptions::WrongFormatException,
                                "Error while parsing " << filename << ": Atomic proposition" << proposition << " was found but not declared.");

                labeling.addLabelToChoice(proposition, globalChoice);
                buf += cnt;
            }
        }
        buf = trimWhitespaces(buf);
        lastState = state;
        lastLocalChoice = localChoice;
    }
}

}  // namespace parser
}  // namespace storm
