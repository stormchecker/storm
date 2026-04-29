#pragma once

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {

template<typename ValueType>
void validateTargetStatesAreAbsorbing(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& targetStates) {
    for (auto targetState : targetStates) {
        for (uint64_t row = transitionMatrix.getRowGroupIndices()[targetState], endRow = transitionMatrix.getRowGroupIndices()[targetState + 1]; row < endRow;
             ++row) {
            for (auto const& entry : transitionMatrix.getRow(row)) {
                STORM_LOG_THROW(entry.getColumn() == targetState, storm::exceptions::InvalidPropertyException,
                                "CVaR query currently requires all original target states to be absorbing.");
            }
        }
    }
}

template<typename ValueType>
storm::storage::MaximalEndComponentDecomposition<ValueType> computeReachableMecs(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                 storm::storage::BitVector const& initialStates) {
    storm::storage::BitVector allStates(transitionMatrix.getRowGroupCount(), true);
    storm::storage::BitVector noStates(transitionMatrix.getRowGroupCount(), false);
    auto reachableStates = storm::utility::graph::getReachableStates(transitionMatrix, initialStates, allStates, noStates);
    auto backwardTransitions = transitionMatrix.transpose(true);
    return storm::storage::MaximalEndComponentDecomposition<ValueType>(transitionMatrix, backwardTransitions, reachableStates);
}

template<typename ValueType>
storm::storage::BitVector computeStatesThatCanReachTarget(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                          storm::storage::BitVector const& targetStates) {
    storm::storage::BitVector allStates(transitionMatrix.getRowGroupCount(), true);
    auto backwardTransitions = transitionMatrix.transpose(true);
    return storm::utility::graph::performProbGreater0(backwardTransitions, allStates, targetStates);
}

template<typename ValueType>
storm::storage::BitVector computeBadMecStates(storm::storage::MaximalEndComponentDecomposition<ValueType> const& mecs,
                                              storm::storage::BitVector const& statesThatCanReachTarget, uint64_t numberOfStates) {
    storm::storage::BitVector badMecStates(numberOfStates, false);
    for (auto const& mec : mecs) {
        if (!mec.containsAnyState(statesThatCanReachTarget)) {
            for (auto const& stateChoices : mec) {
                badMecStates.set(stateChoices.first, true);
            }
        }
    }
    return badMecStates;
}

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
