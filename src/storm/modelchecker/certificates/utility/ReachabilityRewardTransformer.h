#pragma once

#include <optional>
#include <vector>

#include "storm/modelchecker/certificates/utility/TotalRewardSpecification.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/BackwardTransitionCache.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"

namespace storm::modelchecker {

template<typename ValueType>
class ReachabilityRewardTransformer {
   public:
    // TODO: allow symbolic representations, too
    using StateSet = storm::storage::BitVector;

    ReachabilityRewardTransformer(storm::storage::SparseMatrix<ValueType> const& transitionMatrix);
    ReachabilityRewardTransformer(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                  storm::storage::BackwardTransitionCache<ValueType> backwardTransitionCache);

    TotalRewardSpecification<ValueType> transform(StateSet const& targetStates, std::vector<ValueType> const& actionRewards);

    TotalRewardSpecification<ValueType> transform(std::optional<storm::OptimizationDirection> dir, StateSet const& targetStates,
                                                  std::vector<ValueType> const& actionRewards);

    // The matrix needs to remain valid throughout the lifetime of this object.
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix;
    storm::storage::BackwardTransitionCache<ValueType> backwardTransitionCache;
};

}  // namespace storm::modelchecker
