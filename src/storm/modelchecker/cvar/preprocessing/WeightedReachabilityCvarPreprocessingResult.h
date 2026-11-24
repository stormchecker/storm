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
struct WeightedReachabilityCvarPreprocessingResult {
    std::string rewardModelName;
    uint64_t initialState;
    storm::storage::BitVector originalTargetStates;
    storm::storage::BitVector effectiveTargetStates;
    storm::storage::BitVector badMecStates;
    uint64_t collapsedTargetReachingMecCount;
    std::vector<ValueType> terminalRewards;
    storm::storage::SparseMatrix<ValueType> transitionMatrix;
};

}  // namespace preprocessing
}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
