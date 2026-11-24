#pragma once

#include <string>

#include "storm/models/sparse/NondeterministicModel.h"
#include "storm/storage/decomposition/Decomposition.h"
#include "storm/storage/decomposition/MaximalEndComponent.h"
#include "storm/utility/OptionalRef.h"

namespace storm::storage {

/*!
 * This class represents the decomposition of a nondeterministic model into its maximal end components.
 */
template<typename ValueType>
class MaximalEndComponentDecomposition : public Decomposition<MaximalEndComponent> {
   public:
    /*
     * Creates an empty MEC decomposition.
     */
    MaximalEndComponentDecomposition();

    /*
     * Creates an MEC decomposition of the given model.
     *
     * @param model The model to decompose into MECs.
     */
    template<typename RewardModelType = storm::models::sparse::StandardRewardModel<ValueType>>
    MaximalEndComponentDecomposition(storm::models::sparse::NondeterministicModel<ValueType, RewardModelType> const& model);

    /*
     * Creates an MEC decomposition of the given model (represented by a row-grouped matrix).
     *
     * @param transitionMatrix The transition relation of model to decompose into MECs.
     * @param backwardTransition The reversed transition relation.
     */
    MaximalEndComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                     storm::storage::SparseMatrix<ValueType> const& backwardTransitions);

    /*
     * Creates an MEC decomposition of the given subsystem of given model (represented by a row-grouped matrix).
     * If a state-action pair (aka choice) has a transition that leaves the subsystem, the entire state-action pair is ignored.
     *
     * @param transitionMatrix The transition relation of model to decompose into MECs.
     * @param backwardTransition The reversed transition relation.
     * @param states The states of the subsystem to decompose.
     */
    MaximalEndComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                     storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& states);

    /*
     * Creates an MEC decomposition of the given subsystem of given model (represented by a row-grouped matrix).
     * If a state-action pair (aka choice) has a transition that leaves the subsystem, the entire state-action pair is ignored, independent of how the given
     * 'choices' vector is set for that choice.
     *
     * @param transitionMatrix The transition relation of model to decompose into MECs.
     * @param backwardTransition The reversed transition relation.
     * @param states The states of the subsystem to decompose.
     * @param choices The choices of the subsystem to decompose.
     */
    MaximalEndComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                     storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& states,
                                     storm::storage::BitVector const& choices);

    /*!
     * Creates an MEC decomposition of the given subsystem in the given model.
     *
     * @param model The model whose subsystem to decompose into MECs.
     * @param states The states of the subsystem to decompose.
     */
    MaximalEndComponentDecomposition(storm::models::sparse::NondeterministicModel<ValueType> const& model, storm::storage::BitVector const& states);

    /*!
     * Creates an MEC decomposition by copying the contents of the given MEC decomposition.
     *
     * @param other The MEC decomposition to copy.
     */
    MaximalEndComponentDecomposition(MaximalEndComponentDecomposition const& other);

    /*!
     * Assigns the contents of the given MEC decomposition to the current one by copying its contents.
     *
     * @param other The MEC decomposition from which to copy-assign.
     */
    MaximalEndComponentDecomposition& operator=(MaximalEndComponentDecomposition const& other);

    /*!
     * Creates an MEC decomposition by moving the contents of the given MEC decomposition.
     *
     * @param other The MEC decomposition to move.
     */
    MaximalEndComponentDecomposition(MaximalEndComponentDecomposition&& other);

    /*!
     * Assigns the contents of the given MEC decomposition to the current one by moving its contents.
     *
     * @param other The MEC decomposition from which to move-assign.
     */
    MaximalEndComponentDecomposition& operator=(MaximalEndComponentDecomposition&& other);

    /*!
     * Returns a string containing statistics about the MEC decomposition, e.g., the number of (trivial/non-trivial) MECs, the percentage of states on a MEC,
     * etc.
     * @param totalNumberOfStates the total number of states in the model
     */
    std::string statistics(uint64_t totalNumberOfStates) const;

   private:
    /*!
     * Performs the actual decomposition of the given subsystem in the given model into MECs. Stores the MECs found in the current decomposition.
     *
     * @param transitionMatrix The transition matrix representing the system whose subsystem to decompose into MECs.
     * @param backwardTransitions The reversed transition relation.
     * @param states The states of the subsystem to decompose. If not given, all states are considered.
     * @param choices The choices of the subsystem to decompose. If not given, all choices are considered.
     *
     */
    void performMaximalEndComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                 storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                 storm::OptionalRef<storm::storage::BitVector const> states = storm::NullRef,
                                                 storm::OptionalRef<storm::storage::BitVector const> choices = storm::NullRef);
};
}  // namespace storm::storage
