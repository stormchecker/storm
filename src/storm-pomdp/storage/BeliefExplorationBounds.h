#pragma once

#include <vector>

#include "storm/storage/Scheduler.h"

namespace storm {
namespace pomdp {
namespace storage {

/**
 * Struct for storing precomputed values bounding the actual values on the POMDP
 */
template<typename ValueType>
struct PreprocessingPomdpValueBounds {
    // Vectors containing upper and lower bound values for the POMDP states
    std::vector<std::vector<ValueType>> lower;
    std::vector<std::vector<ValueType>> upper;
    std::vector<storm::storage::Scheduler<ValueType>> lowerSchedulers;
    std::vector<storm::storage::Scheduler<ValueType>> upperSchedulers;

    /**
     * Picks the precomputed lower bound for a given scheduler index and state of the POMDP
     * @param scheduler_id the scheduler ID
     * @param state the state ID
     * @return the lower bound value
     */
    ValueType getLowerBound(uint64_t scheduler_id, uint64_t const& state);
    /**
     * Picks the precomputed upper bound for a given scheduler index and state of the POMDP
     * @param scheduler_id the scheduler ID
     * @param state the state ID
     * @return the smallest upper bound value
     */
    ValueType getUpperBound(uint64_t scheduler_id, uint64_t const& state);

    /**
     * Picks the largest precomputed lower bound for a given state of the POMDP
     * @param state the state ID
     * @return the largest lower bound value
     */
    ValueType getHighestLowerBound(uint64_t const& state);
    /**
     * Picks the smallest precomputed upper bound for a given state of the POMDP
     * @param state the state ID
     * @return the smallest upper bound value
     */
    ValueType getSmallestUpperBound(uint64_t const& state);
};

/**
 * Struct to store the extreme bound values needed for the reward correction values when clipping is used
 */
template<typename ValueType>
struct ExtremePOMDPValueBound {
    bool min;
    std::vector<ValueType> values;
    storm::storage::BitVector isInfinite;
    /**
     * Get the extreme bound value for a given state
     * @param state the state ID
     * @return the bound value
     */
    ValueType getValueForState(uint64_t const& state);
};
}  // namespace storage
}  // namespace pomdp
}  // namespace storm