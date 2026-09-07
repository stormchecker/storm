#include "NondeterministicMemoryStructureBuilder.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/memorystructure/NondeterministicMemoryStructure.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {

std::string toString(NondeterministicMemoryStructurePattern const& pattern) {
    switch (pattern) {
        case NondeterministicMemoryStructurePattern::Trivial:
            return "trivial";
        case NondeterministicMemoryStructurePattern::FixedCounter:
            return "fixedcounter";
        case NondeterministicMemoryStructurePattern::SelectiveCounter:
            return "selectivecounter";
        case NondeterministicMemoryStructurePattern::FixedRing:
            return "fixedring";
        case NondeterministicMemoryStructurePattern::SelectiveRing:
            return "ring";
        case NondeterministicMemoryStructurePattern::SettableBits:
            return "settablebits";
        case NondeterministicMemoryStructurePattern::Full:
            return "full";
    }
    return "unknown";
}

NondeterministicMemoryStructure NondeterministicMemoryStructureBuilder::build(NondeterministicMemoryStructurePattern pattern, uint64_t numStates) const {
    switch (pattern) {
        case NondeterministicMemoryStructurePattern::Trivial:
            STORM_LOG_ERROR_COND(numStates == 1, "Invoked building trivial nondeterministic memory structure with "
                                                     << numStates << " states. However, trivial  nondeterministic memory structure always has one state.");
            return buildTrivialMemory();
        case NondeterministicMemoryStructurePattern::FixedCounter:
            return buildFixedCountingMemory(numStates);
        case NondeterministicMemoryStructurePattern::SelectiveCounter:
            return buildSelectiveCountingMemory(numStates);
        case NondeterministicMemoryStructurePattern::FixedRing:
            return buildFixedRingMemory(numStates);
        case NondeterministicMemoryStructurePattern::SelectiveRing:
            return buildSelectiveRingMemory(numStates);
        case NondeterministicMemoryStructurePattern::SettableBits:
            return buildSettableBitsMemory(numStates);
        case NondeterministicMemoryStructurePattern::Full:
            return buildFullyConnectedMemory(numStates);
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Unhandled pattern.");
}

NondeterministicMemoryStructure NondeterministicMemoryStructureBuilder::buildTrivialMemory() const {
    return buildFullyConnectedMemory(1);
}

NondeterministicMemoryStructure NondeterministicMemoryStructureBuilder::buildFixedCountingMemory(uint64_t numStates) const {
    std::vector<storm::storage::BitVector> transitions(numStates, storm::storage::BitVector(numStates, false));
    for (uint64_t state = 0; state < numStates; ++state) {
        transitions[state].set(std::min(state + 1, numStates - 1));
    }
    return NondeterministicMemoryStructure(transitions, 0);
}

NondeterministicMemoryStructure NondeterministicMemoryStructureBuilder::buildSelectiveCountingMemory(uint64_t numStates) const {
    std::vector<storm::storage::BitVector> transitions(numStates, storm::storage::BitVector(numStates, false));
    for (uint64_t state = 0; state < numStates; ++state) {
        transitions[state].set(state);
        transitions[state].set(std::min(state + 1, numStates - 1));
    }
    return NondeterministicMemoryStructure(transitions, 0);
}

NondeterministicMemoryStructure NondeterministicMemoryStructureBuilder::buildFixedRingMemory(uint64_t numStates) const {
    std::vector<storm::storage::BitVector> transitions(numStates, storm::storage::BitVector(numStates, false));
    for (uint64_t state = 0; state < numStates; ++state) {
        transitions[state].set((state + 1) % numStates);
    }
    return NondeterministicMemoryStructure(transitions, 0);
}

NondeterministicMemoryStructure NondeterministicMemoryStructureBuilder::buildSelectiveRingMemory(uint64_t numStates) const {
    std::vector<storm::storage::BitVector> transitions(numStates, storm::storage::BitVector(numStates, false));
    for (uint64_t state = 0; state < numStates; ++state) {
        transitions[state].set(state);
        transitions[state].set((state + 1) % numStates);
    }
    return NondeterministicMemoryStructure(transitions, 0);
}

NondeterministicMemoryStructure NondeterministicMemoryStructureBuilder::buildSettableBitsMemory(uint64_t numStates) const {
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
    return NondeterministicMemoryStructure(transitions, 0);
}

NondeterministicMemoryStructure NondeterministicMemoryStructureBuilder::buildFullyConnectedMemory(uint64_t numStates) const {
    std::vector<storm::storage::BitVector> transitions(numStates, storm::storage::BitVector(numStates, true));
    return NondeterministicMemoryStructure(transitions, 0);
}
}  // namespace storage
}  // namespace storm