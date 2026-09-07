#include "NondeterministicMemoryStructure.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/memorystructure/NondeterministicMemoryStructureBuilder.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {

NondeterministicMemoryStructure::NondeterministicMemoryStructure(std::vector<storm::storage::BitVector> const& transitions, uint64_t initialState)
    : transitions(transitions), initialState(initialState) {
    STORM_LOG_THROW(this->initialState < this->transitions.size(), storm::exceptions::InvalidArgumentException,
                    "Initial state " << this->initialState << " of nondeterministic memory structure is invalid.");
    for (auto const& t : this->transitions) {
        STORM_LOG_THROW(t.size() == this->transitions.size(), storm::exceptions::InvalidArgumentException,
                        "Invalid dimension of transition matrix of nondeterministic memory structure.");
        STORM_LOG_THROW(!t.empty(), storm::exceptions::InvalidArgumentException,
                        "Invalid transition matrix of nondeterministic memory structure: No deadlock states allowed.");
    }
}

uint64_t NondeterministicMemoryStructure::getNumberOfStates() const {
    return transitions.size();
}

uint64_t NondeterministicMemoryStructure::getInitialState() const {
    return initialState;
}

storm::storage::BitVector const& NondeterministicMemoryStructure::getTransitions(uint64_t state) const {
    return transitions.at(state);
}

uint64_t NondeterministicMemoryStructure::getNumberOfOutgoingTransitions(uint64_t state) const {
    return getTransitions(state).getNumberOfSetBits();
}

std::vector<storm::storage::BitVector> const& NondeterministicMemoryStructure::getTransitions() const {
    return transitions;
}

std::string NondeterministicMemoryStructure::toString() const {
    std::string result = "NondeterministicMemoryStructure with " + std::to_string(getNumberOfStates()) + " states.\n";
    result += "Initial state is " + std::to_string(getInitialState()) + ". Transitions are \n";

    // header
    result += "  |";
    for (uint64_t state = 0; state < getNumberOfStates(); ++state) {
        if (state < 10) {
            result += " ";
        }
        result += std::to_string(state);
    }
    result += "\n";
    result += "--|";
    for (uint64_t state = 0; state < getNumberOfStates(); ++state) {
        result += "--";
    }
    result += "\n";

    // transition matrix entries
    for (uint64_t state = 0; state < getNumberOfStates(); ++state) {
        if (state < 10) {
            result += " ";
        }
        result += std::to_string(state) + "|";
        for (uint64_t statePrime = 0; statePrime < getNumberOfStates(); ++statePrime) {
            result += " ";
            if (getTransitions(state).get(statePrime)) {
                result += "1";
            } else {
                result += "0";
            }
        }
        result += "\n";
    }
    return result;
}
}  // namespace storage
}  // namespace storm