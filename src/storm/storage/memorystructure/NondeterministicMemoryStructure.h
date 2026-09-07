#pragma once

#include <vector>

#include "storm/storage/BitVector.h"

namespace storm {
namespace storage {

class NondeterministicMemoryStructure {
   public:
    NondeterministicMemoryStructure(std::vector<storm::storage::BitVector> const& transitions, uint64_t initialState);
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

}  // namespace storage
}  // namespace storm