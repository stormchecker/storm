#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <storm-pomdp/beliefs/exploration/ExplorationQueueOrder.h>

namespace storm::pomdp::beliefs {
/** Criterion that ends exploration and leaves the queued beliefs as the explicit frontier. */
enum explorationTerminationCriterion { MAX_EXPLORATION_SIZE, MAX_EXPLORATION_TIME, MAX_EXPLORATION_SIZE_AND_TIME, NONE };

template<typename ValueType>
struct BeliefBasedModelCheckerOptions {
    bool buildChoiceLabeling = true;
    bool useClipping = false;

    ExplorationQueueOrder explorationQueueOrder = ExplorationQueueOrder::Unordered;

    // Clipping abstraction parameters
    std::optional<std::vector<uint64_t>> clippingResolutions;

    // Termination criteria
    std::optional<uint64_t> maxExplorationSize = std::nullopt;
    std::optional<uint64_t> maxExplorationTime = std::nullopt;
    std::optional<ValueType> maxGapToCut = std::nullopt;

    /**
     * Get the termination criterion for the exploration
     * @return the termination criterion
     */
    [[nodiscard]] explorationTerminationCriterion getTerminationCriterion() const {
        if (maxExplorationSize.has_value() && maxExplorationTime.has_value()) {
            return explorationTerminationCriterion::MAX_EXPLORATION_SIZE_AND_TIME;
        }
        if (maxExplorationSize.has_value()) {
            return explorationTerminationCriterion::MAX_EXPLORATION_SIZE;
        }
        if (maxExplorationTime.has_value()) {
            return explorationTerminationCriterion::MAX_EXPLORATION_TIME;
        }
        return explorationTerminationCriterion::NONE;
    }
};
}  // namespace storm::pomdp::beliefs
