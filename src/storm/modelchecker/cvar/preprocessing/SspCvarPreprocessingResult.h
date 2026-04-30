#pragma once

#include <string>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"

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
    std::vector<ValueType> choiceCosts;
    std::vector<ValueType> expectedCostsToGoal;
    storm::storage::SparseMatrix<ValueType> transitionMatrix;
};

}  // namespace preprocessing
}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
