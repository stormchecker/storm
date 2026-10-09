#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/storage/Scheduler.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace pomdp {
namespace storage {
/**
 * Struct for storing precomputed values bounding the actual values on the POMDP
 */
template<typename POMDPValueType>
struct PreprocessingPomdpValueBounds {
    // Vectors containing upper and lower bound values for the POMDP states
    std::vector<std::vector<POMDPValueType>> lower;
    std::vector<std::vector<POMDPValueType>> upper;
    std::vector<storm::storage::Scheduler<POMDPValueType>> lowerSchedulers;
    std::vector<storm::storage::Scheduler<POMDPValueType>> upperSchedulers;

    /**
     * Picks the precomputed lower bound for a given scheduler index and state of the POMDP
     * @param scheduler_id the scheduler ID
     * @param state the state ID
     * @return the lower bound value
     */
    template<typename OutputValueType>
    OutputValueType getLowerBound(uint64_t scheduler_id, uint64_t const& state) {
        STORM_LOG_ASSERT(!lower.empty(), "requested a lower bound but none were available");
        return storm::utility::convertNumber<OutputValueType>(lower[scheduler_id][state]);
    }
    /**
     * Picks the precomputed upper bound for a given scheduler index and state of the POMDP
     * @param scheduler_id the scheduler ID
     * @param state the state ID
     * @return the smallest upper bound value
     */
    template<typename OutputValueType>
    OutputValueType getUpperBound(uint64_t scheduler_id, uint64_t const& state) {
        STORM_LOG_ASSERT(!upper.empty(), "requested an upper bound but none were available");
        return storm::utility::convertNumber<OutputValueType>(upper[scheduler_id][state]);
    }

    /**
     * Picks the largest precomputed lower bound for a given state of the POMDP
     * @param state the state ID
     * @return the largest lower bound value
     */
    template<typename OutputValueType>
    OutputValueType getHighestLowerBound(uint64_t const& state) {
        STORM_LOG_ASSERT(!lower.empty(), "requested a lower bound but none were available");
        auto it = lower.begin();
        POMDPValueType result = (*it)[state];
        for (++it; it != lower.end(); ++it) {
            result = std::max(result, (*it)[state]);
        }
        return storm::utility::convertNumber<OutputValueType>(result);
    }
    /**
     * Picks the smallest precomputed upper bound for a given state of the POMDP
     * @param state the state ID
     * @return the smallest upper bound value
     */
    template<typename OutputValueType>
    OutputValueType getSmallestUpperBound(uint64_t const& state) {
        STORM_LOG_ASSERT(!upper.empty(), "requested an upper bound but none were available");
        auto it = upper.begin();
        POMDPValueType result = (*it)[state];
        for (++it; it != upper.end(); ++it) {
            result = std::min(result, (*it)[state]);
        }
        return storm::utility::convertNumber<OutputValueType>(result);
    }

    template<typename OutputValueType>
    PreprocessingPomdpValueBounds<OutputValueType> toValueType() {
        PreprocessingPomdpValueBounds<OutputValueType> convertedBounds;
        for (auto const& vec : lower) {
            convertedBounds.lower.push_back(storm::utility::vector::convertNumericVector<OutputValueType>(vec));
        }
        for (auto const& vec : upper) {
            convertedBounds.upper.push_back(storm::utility::vector::convertNumericVector<OutputValueType>(vec));
        }
        for (auto const& sched : lowerSchedulers) {
            convertedBounds.lowerSchedulers.push_back(sched.template toValueType<OutputValueType>());
        }
        for (auto const& sched : upperSchedulers) {
            convertedBounds.upperSchedulers.push_back(sched.template toValueType<OutputValueType>());
        }
        return convertedBounds;
    }
};

/**
 * Struct to store the extreme bound values needed for the reward correction values when clipping is used
 */
template<typename POMDPValueType>
struct ExtremePOMDPValueBound {
    bool min;
    std::vector<POMDPValueType> values;
    storm::storage::BitVector isInfinite;
    /**
     * Get the extreme bound value for a given state
     * @param state the state ID
     * @return the bound value
     */
    template<typename OutputValueType>
    OutputValueType getValueForState(uint64_t const& state) {
        STORM_LOG_ASSERT(!values.empty(), "requested an extreme bound but none were available");
        return storm::utility::convertNumber<OutputValueType>(values[state]);
    }
    template<typename OutputValueType>
    std::vector<OutputValueType> copyValues() const {
        return storm::utility::vector::convertNumericVector<OutputValueType>(values);
    }

    template<typename OutputValueType>
    ExtremePOMDPValueBound<OutputValueType> toValueType() {
        ExtremePOMDPValueBound<OutputValueType> convertedBounds;
        convertedBounds.values = copyValues<OutputValueType>();
        convertedBounds.min = min;
        convertedBounds.isInfinite = isInfinite;
        return convertedBounds;
    }
};

/**
 * Struct for storing precomputed values bounding the actual values on the POMDP
 */
template<typename POMDPValueType>
struct BeliefExplorationBounds {
    std::optional<PreprocessingPomdpValueBounds<POMDPValueType>> preprocessingBounds = std::nullopt;
    std::optional<ExtremePOMDPValueBound<POMDPValueType>> extremeBounds = std::nullopt;
};

}  // namespace storage
}  // namespace pomdp
}  // namespace storm
