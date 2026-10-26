#pragma once

#include <optional>

#include "storm/modelchecker/certificates/utility/TotalRewardSpecification.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/BackwardTransitionCache.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/OptionalRef.h"

namespace storm::modelchecker {

template<typename ValueType>
class ReachabilityProbabilityToRewardTransformer {
   public:
    using StateSet = storm::storage::BitVector;

    ReachabilityProbabilityToRewardTransformer(storm::storage::SparseMatrix<ValueType> const& transitionMatrix);
    ReachabilityProbabilityToRewardTransformer(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                               storm::storage::BackwardTransitionCache<ValueType> backwardTransitionCache);

    TotalRewardSpecification<ValueType> transform(StateSet const& targetStates, storm::OptionalRef<StateSet const> constraintStates = storm::NullRef);

    TotalRewardSpecification<ValueType> transform(std::optional<storm::OptimizationDirection> dir, StateSet const& targetStates,
                                                  storm::OptionalRef<StateSet const> constraintStates = storm::NullRef);

    // The matrix needs to remain valid throughout the lifetime of this object.
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix;
    storm::storage::BackwardTransitionCache<ValueType> backwardTransitionCache;
};

}  // namespace storm::modelchecker
