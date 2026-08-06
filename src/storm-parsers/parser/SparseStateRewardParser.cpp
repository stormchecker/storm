#include "storm-parsers/parser/SparseStateRewardParser.h"

#include <iostream>

#include "storm-parsers/parser/MappedFile.h"
#include "storm-parsers/util/cstring.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {

using namespace storm::utility::cstring;

template<typename ValueType>
std::vector<ValueType> SparseStateRewardParser<ValueType>::parseSparseStateReward(uint_fast64_t stateCount, std::string const& filename) {
    // Open file.
    MappedFile file(filename.c_str());
    char const* buf = file.getData();

    // Create state reward vector with given state count.
    std::vector<ValueType> stateRewards(stateCount);

    // Now parse state reward assignments.
    uint_fast64_t state = 0;
    uint_fast64_t lastState = (uint_fast64_t)-1;
    uint_fast64_t const startIndexComparison = lastState;
    double reward;

    // Iterate over states.
    while (buf[0] != '\0') {
        // Parse state.
        state = checked_strtol(buf, &buf);

        // If the state has already been read or skipped once there might be a problem with the file (doubled lines, or blocks).
        // Note: The value -1 shows that lastState has not yet been set, i.e. this is the first run of the loop (state index (2^64)-1 is a really bad starting
        // index).
        STORM_LOG_THROW(state > lastState || lastState == startIndexComparison, storm::exceptions::WrongFormatException,
                        "Error while parsing " << filename << ": State " << state << " was found but has already been read or skipped previously.");

        if (stateCount <= state) {
            STORM_LOG_ERROR("Error while parsing " << filename << ": Found reward for a state of an invalid index \"" << state << "\". The model has only "
                                                   << stateCount << " states.");
            STORM_LOG_THROW(false, storm::exceptions::OutOfRangeException,
                            "Error while parsing " << filename << ": Found reward for a state of an invalid index \"" << state << "\".");
        }

        // Parse reward value.
        reward = checked_strtod(buf, &buf);

        if (reward < 0.0) {
            STORM_LOG_ERROR("Error while parsing " << filename << ": Expected positive reward value but got \"" << reward << "\".");
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "Error while parsing " << filename << ": State reward file specifies illegal reward value.");
        }

        stateRewards[state] = reward;

        buf = trimWhitespaces(buf);
        lastState = state;
    }
    return stateRewards;
}

template class SparseStateRewardParser<double>;
template class SparseStateRewardParser<storm::Interval>;
}  // namespace parser
}  // namespace storm
