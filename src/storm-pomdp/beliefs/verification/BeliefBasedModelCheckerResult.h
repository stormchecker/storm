#pragma once

#include "storm-pomdp/beliefs/policy/ObservationBasedFiniteStateController.h"
#include "storm/utility/ExtendedNumber.h"

#include <cstdint>
#include <optional>

namespace storm::pomdp::beliefs {

/** Statistics recorded during a belief-based model-checking run. */
struct BeliefBasedModelCheckerStatistics {
    bool completedExploration = false;
    uint64_t discoveredBeliefs = 0;
    uint64_t exploredBeliefs = 0;
    uint64_t beliefMdpStates = 0;
    uint64_t beliefMdpChoices = 0;
    uint64_t beliefMdpTransitions = 0;
    std::optional<uint64_t> processedMdpStates;
    std::optional<uint64_t> processedMdpChoices;
    std::optional<uint64_t> processedMdpTransitions;
    uint64_t explorationTimeMilliseconds = 0;
    uint64_t beliefMdpBuildTimeMilliseconds = 0;
    uint64_t beliefMdpAnalysisTimeMilliseconds = 0;
};

/** Result of a belief-based model-checking run. */
template<typename BeliefMdpValueType, typename PolicyValueType = BeliefMdpValueType>
struct BeliefBasedModelCheckerResult {
    storm::utility::ExtendedValueType<BeliefMdpValueType> value;
    bool completedExploration = false;
    BeliefBasedModelCheckerStatistics statistics;
    std::optional<storm::pomdp::policy::ObservationBasedFiniteStateController<PolicyValueType>> policy;
};

}  // namespace storm::pomdp::beliefs
