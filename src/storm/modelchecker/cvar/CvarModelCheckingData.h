#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "storm/modelchecker/cvar/CvarFormulaInformation.h"
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

template<typename SparseMdpModelType>
struct CvarModelCheckingData {
    using ValueType = typename SparseMdpModelType::ValueType;

    double alpha;
    storm::solver::OptimizationDirection optimizationDirection;
    uint64_t initialState;
    std::string rewardModelName;
    storm::storage::BitVector targetStates;
    std::vector<ValueType> terminalRewards;
    std::vector<ValueType> candidateThresholds;
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix;
    SparseMdpModelType const& model;
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

template<typename SparseMdpModelType>
CvarModelCheckingData<SparseMdpModelType> createCvarModelCheckingData(
    SparseMdpModelType const& model, CvarFormulaInformation const& formulaInformation,
    WeightedReachabilityModelInformation<typename SparseMdpModelType::ValueType> const& weightedReachabilityModelInformation) {
    auto candidateThresholds =
        collectCandidateThresholds(weightedReachabilityModelInformation.targetStates, weightedReachabilityModelInformation.terminalRewards);
    return {formulaInformation.alpha,
            formulaInformation.optimizationDirection,
            *model.getInitialStates().begin(),
            weightedReachabilityModelInformation.rewardModelName,
            weightedReachabilityModelInformation.targetStates,
            weightedReachabilityModelInformation.terminalRewards,
            std::move(candidateThresholds),
            model.getTransitionMatrix(),
            model};
}

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
