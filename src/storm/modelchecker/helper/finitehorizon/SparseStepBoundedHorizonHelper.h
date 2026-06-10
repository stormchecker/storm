#pragma once

#include "storm/modelchecker/hints/ModelCheckerHint.h"
#include "storm/solver/SolveGoal.h"
#include "storm/storage/SparseMatrix.h"

namespace storm::modelchecker::helper {

template<typename ValueType, typename SolutionType = ValueType>
class SparseStepBoundedHorizonHelper {
   public:
    SparseStepBoundedHorizonHelper();

    /*!
     * Computes the probability of staying in phiStates until reaching psiStates within [lowerBound, upperBound] steps.
     */
    std::vector<SolutionType> computeStepBoundedUntilProbabilities(Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal,
                                                                   storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                   storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                   storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                                   uint64_t const lowerBound, uint64_t const upperBound,
                                                                   ModelCheckerHint const& hint = ModelCheckerHint());
};

}  // namespace storm::modelchecker::helper
