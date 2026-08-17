#include "storm-parsers/parser/SparseChoiceLabelingParser.h"

#include "storm-parsers/parser/MappedFile.h"
#include "storm-parsers/util/cstring.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {

using namespace storm::utility::cstring;

std::vector<storm::storage::FlatSet<uint_fast64_t>> SparseChoiceLabelingParser::parseChoiceLabeling(
    std::vector<uint_fast64_t> const& nondeterministicChoiceIndices, std::string const& filename) {
    // Open file.
    MappedFile file(filename.c_str());
    char const* buf = file.getData();

    uint_fast64_t totalNumberOfChoices = nondeterministicChoiceIndices.back();

    // Create choice labeling vector with given choice count.
    std::vector<storm::storage::FlatSet<uint_fast64_t>> result(totalNumberOfChoices);

    // Now parse state reward assignments.
    uint_fast64_t state = 0;
    uint_fast64_t lastState = (uint_fast64_t)-1;
    uint_fast64_t lastChoice = (uint_fast64_t)-1;
    uint_fast64_t const startIndexComparison = lastState;
    uint_fast64_t const startChoiceIndexComparison = lastChoice;
    uint_fast64_t choice = 0;
    uint_fast64_t label = 0;

    // Iterate over states.
    while (buf[0] != '\0') {
        // Parse state number and choice.
        state = checked_strtol(buf, &buf);
        choice = checked_strtol(buf, &buf);

        // If the state has already been read or skipped once there might be a problem with the file (doubled lines, or blocks).
        // Note: The value -1 shows that lastState has not yet been set, i.e. this is the first run of the loop (state index (2^64)-1 is a really bad starting
        // index).
        STORM_LOG_THROW(state >= lastState || lastState == startIndexComparison, storm::exceptions::WrongFormatException,
                        "Error while parsing " << filename << ": State " << state << " was found but has already been read or skipped previously.");
        STORM_LOG_THROW(state != lastState || choice >= lastChoice || lastChoice == startChoiceIndexComparison, storm::exceptions::WrongFormatException,
                        "Error while parsing " << filename << ": Choice " << choice << " was found but has already been read or skipped previously.");
        STORM_LOG_THROW(state < nondeterministicChoiceIndices.size(), storm::exceptions::WrongFormatException,
                        "Error while parsing " << filename << ": Illegal state " << state << ".");
        uint_fast64_t numberOfChoicesForState = nondeterministicChoiceIndices[state + 1] - nondeterministicChoiceIndices[state];
        if (choice >= numberOfChoicesForState) {
            STORM_LOG_THROW(
                false, storm::exceptions::WrongFormatException,
                "Error while parsing " << filename << ": Choice " << choice << " is illegal for state " << state << ", because this state has fewer choices.");
        }

        label = checked_strtol(buf, &buf);

        result[nondeterministicChoiceIndices[state] + choice].insert(label);

        buf = trimWhitespaces(buf);
        lastState = state;
        lastChoice = choice;
    }

    return result;
}
}  // namespace parser
}  // namespace storm
