/**
 * @file:   DeterministicTransition.h
 * @author: Sebastian Junges
 *
 * @since April 24, 2014
 */

#pragma once

#include <cstdint>
#include <utility>

namespace storm {
namespace storage {
typedef uint_fast64_t StateId;

template<typename ProbabilityType>
class DeterministicTransition {
    std::pair<StateId, ProbabilityType> mTransition;

   public:
    DeterministicTransition(std::pair<StateId, ProbabilityType> const& transition) : mTransition(transition) {}

    DeterministicTransition(std::pair<StateId, ProbabilityType>&& transition) : mTransition(transition) {}

    DeterministicTransition(StateId targetState) : DeterministicTransition({targetState, ProbabilityType(0)}) {}

    StateId& targetState() {
        return mTransition.first;
    }
    StateId const& targetState() const {
        return mTransition.first;
    }

    ProbabilityType& probability() {
        return mTransition.second;
    }

    ProbabilityType const& probability() const {
        return mTransition.second;
    }
};
}  // namespace storage
}  // namespace storm
