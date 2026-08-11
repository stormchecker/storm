#include "storm-pomdp/storage/PomdpMemory.h"

#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {

PomdpMemory::PomdpMemory(std::vector<storm::storage::BitVector> const& transitions, uint64_t initialState)
    : transitions(transitions), initialState(initialState) {
    STORM_LOG_THROW(this->initialState < this->transitions.size(), storm::exceptions::InvalidArgumentException,
                    "Initial state " << this->initialState << " of pomdp memory is invalid.");
    for (auto const& t : this->transitions) {
        STORM_LOG_THROW(t.size() == this->transitions.size(), storm::exceptions::InvalidArgumentException,
                        "Invalid dimension of transition matrix of pomdp memory.");
        STORM_LOG_THROW(!t.empty(), storm::exceptions::InvalidArgumentException, "Invalid transition matrix of pomdp memory: No deadlock states allowed.");
    }
}

uint64_t PomdpMemory::getNumberOfStates() const {
    return transitions.size();
}

uint64_t PomdpMemory::getInitialState() const {
    return initialState;
}

storm::storage::BitVector const& PomdpMemory::getTransitions(uint64_t state) const {
    return transitions.at(state);
}

uint64_t PomdpMemory::getNumberOfOutgoingTransitions(uint64_t state) const {
    return getTransitions(state).getNumberOfSetBits();
}

std::vector<storm::storage::BitVector> const& PomdpMemory::getTransitions() const {
    return transitions;
}

std::string PomdpMemory::toString() const {
    std::string result = "PomdpMemory with " + std::to_string(getNumberOfStates()) + " states.\n";
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

std::string toString(PomdpMemoryPattern const& pattern) {
    switch (pattern) {
        case PomdpMemoryPattern::Trivial:
            return "trivial";
        case PomdpMemoryPattern::FixedCounter:
            return "fixedcounter";
        case PomdpMemoryPattern::SelectiveCounter:
            return "selectivecounter";
        case PomdpMemoryPattern::FixedRing:
            return "fixedring";
        case PomdpMemoryPattern::SelectiveRing:
            return "ring";
        case PomdpMemoryPattern::SettableBits:
            return "settablebits";
        case PomdpMemoryPattern::Full:
            return "full";
    }
    return "unknown";
}

PomdpMemory PomdpMemoryBuilder::build(PomdpMemoryPattern pattern, uint64_t numStates) const {
    switch (pattern) {
        case PomdpMemoryPattern::Trivial:
            STORM_LOG_ERROR_COND(numStates == 1,
                                 "Invoked building trivial POMDP memory with " << numStates << " states. However, trivial POMDP memory always has one state.");
            return buildTrivialMemory();
        case PomdpMemoryPattern::FixedCounter:
            return buildFixedCountingMemory(numStates);
        case PomdpMemoryPattern::SelectiveCounter:
            return buildSelectiveCountingMemory(numStates);
        case PomdpMemoryPattern::FixedRing:
            return buildFixedRingMemory(numStates);
        case PomdpMemoryPattern::SelectiveRing:
            return buildSelectiveRingMemory(numStates);
        case PomdpMemoryPattern::SettableBits:
            return buildSettableBitsMemory(numStates);
        case PomdpMemoryPattern::Full:
            return buildFullyConnectedMemory(numStates);
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Unknown PomdpMemoryPattern.");
}

PomdpMemory PomdpMemoryBuilder::buildTrivialMemory() const {
    return buildFullyConnectedMemory(1);
}

PomdpMemory PomdpMemoryBuilder::buildFixedCountingMemory(uint64_t numStates) const {
    std::vector<storm::storage::BitVector> transitions(numStates, storm::storage::BitVector(numStates, false));
    for (uint64_t state = 0; state < numStates; ++state) {
        transitions[state].set(std::min(state + 1, numStates - 1));
    }
    return PomdpMemory(transitions, 0);
}

PomdpMemory PomdpMemoryBuilder::buildSelectiveCountingMemory(uint64_t numStates) const {
    std::vector<storm::storage::BitVector> transitions(numStates, storm::storage::BitVector(numStates, false));
    for (uint64_t state = 0; state < numStates; ++state) {
        transitions[state].set(state);
        transitions[state].set(std::min(state + 1, numStates - 1));
    }
    return PomdpMemory(transitions, 0);
}

PomdpMemory PomdpMemoryBuilder::buildFixedRingMemory(uint64_t numStates) const {
    std::vector<storm::storage::BitVector> transitions(numStates, storm::storage::BitVector(numStates, false));
    for (uint64_t state = 0; state < numStates; ++state) {
        transitions[state].set((state + 1) % numStates);
    }
    return PomdpMemory(transitions, 0);
}

PomdpMemory PomdpMemoryBuilder::buildSelectiveRingMemory(uint64_t numStates) const {
    std::vector<storm::storage::BitVector> transitions(numStates, storm::storage::BitVector(numStates, false));
    for (uint64_t state = 0; state < numStates; ++state) {
        transitions[state].set(state);
        transitions[state].set((state + 1) % numStates);
    }
    return PomdpMemory(transitions, 0);
}

PomdpMemory PomdpMemoryBuilder::buildSettableBitsMemory(uint64_t numStates) const {
    // compute the number of bits, i.e., floor(log(numStates))
    uint64_t numBits = 0;
    uint64_t actualNumStates = 1;
    while (actualNumStates * 2 <= numStates) {
        actualNumStates *= 2;
        ++numBits;
    }

    STORM_LOG_WARN_COND(actualNumStates == numStates,
                        "The number of memory states for the settable bits pattern has to be a power of 2. Shrinking the number of memory states to "
                            << actualNumStates << ".");

    std::vector<storm::storage::BitVector> transitions(actualNumStates, storm::storage::BitVector(actualNumStates, false));
    for (uint64_t state = 0; state < actualNumStates; ++state) {
        transitions[state].set(state);
        for (uint64_t bit = 0; bit < numBits; ++bit) {
            uint64_t bitMask = 1u << bit;
            transitions[state].set(state | bitMask);
        }
    }
    return PomdpMemory(transitions, 0);
}

PomdpMemory PomdpMemoryBuilder::buildFullyConnectedMemory(uint64_t numStates) const {
    std::vector<storm::storage::BitVector> transitions(numStates, storm::storage::BitVector(numStates, true));
    return PomdpMemory(transitions, 0);
}
}  // namespace storage
}  // namespace storm
