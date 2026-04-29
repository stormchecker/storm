#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/modelchecker/cvar/WeightedReachabilityModelInformation.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"

namespace storm {
namespace modelchecker {
namespace cvar {

template<typename ValueType>
struct CvarThresholdData {
    ValueType threshold;
    storm::storage::BitVector targetStatesBelowThreshold;
    storm::storage::BitVector targetStatesAtThreshold;
    storm::storage::BitVector targetStatesBelowOrAtThreshold;
};

template<typename ValueType>
struct WeightedReachabilityCvarLpData {
    double alpha;
    storm::solver::OptimizationDirection optimizationDirection;
    uint64_t initialState;
    std::string rewardModelName;
    storm::storage::BitVector targetStates;
    std::vector<ValueType> terminalRewards;
    std::vector<ValueType> candidateThresholds;
    storm::storage::SparseMatrix<ValueType> transitionMatrix;
};

template<typename ValueType>
std::vector<ValueType> collectCandidateThresholds(storm::storage::BitVector const& targetStates, std::vector<ValueType> const& terminalRewards) {
    std::vector<ValueType> candidateThresholds;
    candidateThresholds.reserve(targetStates.getNumberOfSetBits());
    for (uint64_t state = 0; state < terminalRewards.size(); ++state) {
        if (targetStates[state]) {
            candidateThresholds.push_back(terminalRewards[state]);
        }
    }
    std::sort(candidateThresholds.begin(), candidateThresholds.end());
    candidateThresholds.erase(std::unique(candidateThresholds.begin(), candidateThresholds.end()), candidateThresholds.end());
    return candidateThresholds;
}

template<typename ValueType>
CvarThresholdData<ValueType> createCvarThresholdData(storm::storage::BitVector const& targetStates, std::vector<ValueType> const& terminalRewards,
                                                     ValueType const& threshold) {
    storm::storage::BitVector targetStatesBelowThreshold(targetStates.size(), false);
    storm::storage::BitVector targetStatesAtThreshold(targetStates.size(), false);
    storm::storage::BitVector targetStatesBelowOrAtThreshold(targetStates.size(), false);

    for (uint64_t state = 0; state < terminalRewards.size(); ++state) {
        if (!targetStates[state]) {
            continue;
        }
        if (terminalRewards[state] < threshold) {
            targetStatesBelowThreshold.set(state, true);
            targetStatesBelowOrAtThreshold.set(state, true);
        } else if (terminalRewards[state] == threshold) {
            targetStatesAtThreshold.set(state, true);
            targetStatesBelowOrAtThreshold.set(state, true);
        }
    }

    return {threshold, targetStatesBelowThreshold, targetStatesAtThreshold, targetStatesBelowOrAtThreshold};
}

template<typename ValueType>
WeightedReachabilityCvarLpData<ValueType> createWeightedReachabilityCvarLpData(
    CvarQueryInformation const& queryInformation, WeightedReachabilityModelInformation<ValueType> const& weightedReachabilityModelInformation) {
    auto candidateThresholds =
        collectCandidateThresholds(weightedReachabilityModelInformation.effectiveTargetStates, weightedReachabilityModelInformation.terminalRewards);
    return {queryInformation.alpha,
            queryInformation.optimizationDirection,
            weightedReachabilityModelInformation.initialState,
            weightedReachabilityModelInformation.rewardModelName,
            weightedReachabilityModelInformation.effectiveTargetStates,
            weightedReachabilityModelInformation.terminalRewards,
            std::move(candidateThresholds),
            weightedReachabilityModelInformation.transitionMatrix};
}

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
