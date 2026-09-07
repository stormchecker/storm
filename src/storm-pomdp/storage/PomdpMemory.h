#pragma once

#include <vector>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace storage {

class PomdpMemory {
   public:
    PomdpMemory(std::vector<storm::storage::BitVector> const& transitions, uint64_t initialState);
    uint64_t getNumberOfStates() const;
    uint64_t getInitialState() const;
    storm::storage::BitVector const& getTransitions(uint64_t state) const;
    uint64_t getNumberOfOutgoingTransitions(uint64_t state) const;
    std::vector<storm::storage::BitVector> const& getTransitions() const;
    std::string toString() const;

   private:
    std::vector<storm::storage::BitVector> transitions;
    uint64_t initialState;
};

enum class PomdpMemoryPattern { Trivial, FixedCounter, SelectiveCounter, FixedRing, SelectiveRing, SettableBits, Full };

std::string toString(PomdpMemoryPattern const& pattern);

class PomdpMemoryBuilder {
   public:
    // Builds a memory structure with the given pattern and the given number of states.
    PomdpMemory build(PomdpMemoryPattern pattern, uint64_t numStates) const;

    // Builds a memory structure that consists of just a single memory state
    PomdpMemory buildTrivialMemory() const;

    // Builds a memory structure that consists of a chain of the given number of states.
    // Every state has exactly one transition to the next state. The last state has just a selfloop.
    PomdpMemory buildFixedCountingMemory(uint64_t numStates) const;

    // Builds a memory structure that consists of a chain of the given number of states.
    // Every state has a selfloop and a transition to the next state. The last state just has a selfloop.
    PomdpMemory buildSelectiveCountingMemory(uint64_t numStates) const;

    // Builds a memory structure that consists of a ring of the given number of states.
    // Every state has a transition to the successor state
    PomdpMemory buildFixedRingMemory(uint64_t numStates) const;

    // Builds a memory structure that consists of a ring of the given number of states.
    // Every state has a transition to the successor state and a selfloop
    PomdpMemory buildSelectiveRingMemory(uint64_t numStates) const;

    // Builds a memory structure that represents floor(log(numStates)) bits that can only be set from zero to one or from zero to zero.
    PomdpMemory buildSettableBitsMemory(uint64_t numStates) const;

    // Builds a memory structure that consists of the given number of states which are fully connected.
    PomdpMemory buildFullyConnectedMemory(uint64_t numStates) const;
};

}  // namespace storage
}  // namespace storm