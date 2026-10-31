#pragma once

#include <optional>
#include <string>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/zeroWeight/ZeroWeightAnalysis.h"

namespace storm {
namespace modelchecker {
namespace cvar {
namespace preprocessing {

template<typename ValueType>
struct SspCvarPreprocessingResult {
    std::string rewardModelName;
    uint64_t initialState;
    storm::storage::BitVector targetStates;
    storm::storage::BitVector reachableStates;
    bool liftedStateRewardsToChoiceCosts;
    bool normalizedTargetStatesToAbsorbing;
    uint64_t maximalChoiceCost;
    std::vector<ValueType> choiceCosts;
    std::vector<ValueType> expectedCostsToGoal;
    storm::storage::SparseMatrix<ValueType> transitionMatrix;
    std::optional<typename storm::transformer::ZeroWeightAnalysis<ValueType>::Result> zeroWeightAnalysis;
};

}  // namespace preprocessing
}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
