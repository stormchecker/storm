#pragma once

#include "storm/models/sparse/DeterministicModel.h"
#include "storm/storage/BoostTypes.h"
#include "storm/storage/Decomposition.h"
#include "storm/storage/StronglyConnectedComponent.h"
#include "storm/storage/sparse/StateType.h"
#include "storm/utility/OptionalRef.h"

namespace storm::storage {

/*!
 * This class represents the decomposition of a nondeterministic model into its maximal end components.
 */
template<typename ValueType>
class RobustMaximalEndComponentDecomposition : public Decomposition<StronglyConnectedComponent> {
   public:
    /*
     * Creates an empty MEC decomposition.
     */
    RobustMaximalEndComponentDecomposition();

    /*
     * Creates an MEC decomposition of the given model.
     *
     * @param model The model to decompose into MECs.
     */
    template<typename RewardModelType = storm::models::sparse::StandardRewardModel<ValueType>>
    RobustMaximalEndComponentDecomposition(storm::models::sparse::DeterministicModel<ValueType, RewardModelType> const& model);

    /*
     * Creates an MEC decomposition of the given model (represented by a row-grouped matrix).
     *
     * @param transitionMatrix The transition relation of model to decompose into MECs.
     * @param backwardTransition The reversed transition relation.
     * @param vector The vector of the system of equations (e.g. rewards or probabilities to go to target).
     */
    RobustMaximalEndComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                           storm::storage::SparseMatrix<ValueType> const& backwardTransitions, std::vector<ValueType> const& vector);

    /*
     * Creates an MEC decomposition of the given subsystem of given model (represented by a row-grouped matrix).
     * If a state-action pair (aka choice) has a transition that leaves the subsystem, the entire state-action pair is ignored.
     *
     * @param transitionMatrix The transition relation of model to decompose into MECs.
     * @param backwardTransition The reversed transition relation.
     * @param states The states of the subsystem to decompose.
     * @param vector The vector of the system of equations (e.g. rewards or probabilities to go to target).
     */
    RobustMaximalEndComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                           storm::storage::SparseMatrix<ValueType> const& backwardTransitions, std::vector<ValueType> const& vector,
                                           storm::storage::BitVector const& states);

    /*!
     * Creates an MEC decomposition by copying the contents of the given MEC decomposition.
     *
     * @param other The MEC decomposition to copy.
     */
    RobustMaximalEndComponentDecomposition(RobustMaximalEndComponentDecomposition const& other);

    /*!
     * Assigns the contents of the given MEC decomposition to the current one by copying its contents.
     *
     * @param other The MEC decomposition from which to copy-assign.
     */
    RobustMaximalEndComponentDecomposition& operator=(RobustMaximalEndComponentDecomposition const& other);

    /*!
     * Creates an MEC decomposition by moving the contents of the given MEC decomposition.
     *
     * @param other The MEC decomposition to move.
     */
    RobustMaximalEndComponentDecomposition(RobustMaximalEndComponentDecomposition&& other);

    /*!
     * Assigns the contents of the given MEC decomposition to the current one by moving its contents.
     *
     * @param other The MEC decomposition from which to move-assign.
     */
    RobustMaximalEndComponentDecomposition& operator=(RobustMaximalEndComponentDecomposition&& other);

    /*!
     * Computes a vector that for each state has the index of the scc of that state in it.
     * If a state has no SCC in this decomposition (e.g. because we considered a subsystem), they will get SCC index std::numeric_limits<uint64_t>::max()
     *
     * @param numberOfStates the total number of states
     */
    std::vector<uint64_t> computeStateToSccIndexMap(uint64_t numberOfStates) const;

   private:
    /*!
     * Performs the actual decomposition of the given subsystem in the given model into MECs. Stores the MECs found in the current decomposition.
     *
     * @param transitionMatrix The transition matrix representing the system whose subsystem to decompose into MECs.
     * @param backwardTransitions The reversed transition relation.
     * @param states The states of the subsystem to decompose. If not given, all states are considered.
     * @param vector The vector of the system of equations (e.g. rewards or probabilities to go to target).
     *
     */
    void performRobustMaximalEndComponentDecomposition(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                       storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                       storm::OptionalRef<std::vector<ValueType> const> vector = storm::NullRef,
                                                       storm::OptionalRef<storm::storage::BitVector const> states = storm::NullRef);
};
}  // namespace storm::storage
