#pragma once
#include <vector>

#include "storm/modelchecker/helper/SingleValueModelCheckerHelper.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/OptionalRef.h"

namespace storm {
class Environment;

namespace modelchecker {
namespace helper {

/*!
 * Helper class for computing for each state the expected number of times to visit that state assuming a given initial distribution.
 * @see https://arxiv.org/abs/2401.10638 for theoretical background of this.
 * @tparam ValueType the type a value can have
 */
template<typename ValueType>
class SparseDeterministicVisitingTimesHelper : public SingleValueModelCheckerHelper<ValueType, storm::models::ModelRepresentation::Sparse> {
   public:
    /*!
     * Function mapping from indices to values
     */
    typedef std::function<ValueType(uint64_t)> ValueGetter;

    using ExtendedValueType = storm::utility::ExtendedValueType<ValueType>;

    /*!
     * Initializes the helper for a DTMC
     */
    SparseDeterministicVisitingTimesHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix);

    /*!
     * Initializes the helper for a CTMC
     */
    SparseDeterministicVisitingTimesHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType> const& exitRates);

    /*!
     * Provides the backward transitions that can be used during the computation.
     * Providing the backward transitions is optional. If they are not provided, they will be computed internally.
     * Be aware that this class does not take ownership, i.e. the caller has to make sure that the reference to the given matrix remains valid.
     */
    void provideBackwardTransitions(storm::storage::SparseMatrix<ValueType> const& backwardTransitions);

    /*!
     * Provides the decomposition into SCCs that can be used during the computation.
     * Providing the decomposition is optional. If it is not provided, they will be computed internally.
     * Be aware that this class does not take ownership, i.e. the caller has to make sure that the reference to the decomposition remains valid.
     * The given decomposition shall
     * * include naive SCCs,
     * * be topologically sorted, and
     * * (if sound model checking is requested) shall contain sccDepths
     */
    void provideSCCDecomposition(storm::storage::StronglyConnectedComponentDecomposition<ValueType> const& decomposition);

    /*!
     * Computes for each state the expected number of times we are visiting that state assuming the given initial state(s).
     *
     * @note In contrast to most other queries, this method assumes a uniform distribution over the provided initial state.
     *
     * @return a value for each state
     */
    std::vector<ExtendedValueType> computeExpectedVisitingTimes(Environment const& env, storm::storage::BitVector const& initialStates);

    /*!
     * Computes for each state the expected number of times we are visiting that state assuming the given initial state.
     *
     * @return a value for each state
     */
    std::vector<ExtendedValueType> computeExpectedVisitingTimes(Environment const& env, uint64_t initialState);

    /*!
     * Computes for each state the expected number of times we are visiting that state assuming the given initial state probabilities
     * @param initialStateValueGetter function that returns for each state the initial value (probability) for that state.
     * The values can actually sum up to something different than 1 but should be non-negative.
     */
    std::vector<ExtendedValueType> computeExpectedVisitingTimes(Environment const& env, ValueGetter const& initialStateValueGetter);

    /*!
     * Computes for each state the expected number of times we are visiting that state assuming the given initial state(s).
     * @pre parameter stateValues vector contains for each state the initial value (probability) for that state.
     * The values can actually sum up to something different than 1 but should be non-negative.
     * @post parameter stateValues contains the desired values
     */
    void computeExpectedVisitingTimes(Environment const& env, std::vector<ExtendedValueType>& stateValues);

    /*!
     * Computes for each selected state the expected number of times we are visiting that state assuming the given initial state probabilities
     * The interpretation of the subsystem is that once a path has exited the subsystem, all subsequent visits will be ignored.
     * All states within the subsystem must reach a state outside of the subsystem almost surely. In other words, the subsystem shall not contain a BSCC.
     *
     * @param subsystem the set of states for which visiting times are computed.
     * @param initialValues contains for each subsystem state the initial value (probability) for that state.
     *        The values can actually sum up to something different than 1 but should be non-negative.
     * @return the expected visiting times for the states in the subsystem.
     *
     * @pre subsystem.getNumberOfSetBits() == initialValues.size()
     * @post result.size() == initialValues.size()
     */
    std::vector<ValueType> computeExpectedVisitingTimes(Environment const& env, storm::storage::BitVector const& subsystem,
                                                        std::vector<ValueType> const& initialValues) const;

   private:
    /*!
     * @return true iff this is a computation on a continuous time model (i.e. CTMC, MA)
     */
    bool isContinuousTime() const;

    /*!
     * @post _backwardTransitions points to the backward transitions
     */
    void createBackwardTransitions();

    /*!
     * @post _sccDecomposition points to an SCC decomposition
     */
    void createDecomposition(Environment const& env);

    /*!
     * @post _nonBsccStates points to the vector of non-BSCC states
     */
    void createNonBsccStateVector();

    /*!
     * Computes for each state an upper bound on the expected number of times we are visiting that state.
     * @note The upper bounds are computed using techniques from by Baier et al. [CAV'17] (https://doi.org/10.1007/978-3-319-63387-9_8)
     * @param stateSetAsBitVector the states for which the upper bounds are computed.
     * @return for each state the upper bound on the expected number of times that state is visited.
     */
    std::vector<ValueType> computeUpperBounds(storm::storage::BitVector const& stateSetAsBitVector) const;

    /*!
     * Adapts the precision of the solving method if necessary (i.e., if the model is a CTMC or when a topological solving method is used).
     * @param env The environment, containing information on the precision that should be achieved.
     * @param topological If set to true, the environment when solving non-trivial SCCs for topological solving method is returned.
     * @return the environment used when solving the equation system(s) for EVTs with adapted precision for a topological solving method or CTMCs.
     */
    storm::Environment getEnvironmentForSolver(storm::Environment const& env, bool topological = false) const;

    /*!
     * @return the environment used when solving non-trivial SCCs for topological solving method.
     */
    storm::Environment getEnvironmentForTopologicalSolver(storm::Environment const& env) const;

    /*!
     * Processes (bottom or non-bottom SCCs consisting of a single state). The resulting value is directly inserted into stateValues
     */
    void processSingletonScc(uint64_t sccState, std::vector<ExtendedValueType>& stateValues) const;

    /*!
     * Solves the equation system for non-singleton (subs)sets of the chain's non-bottom states.
     * @pre none of the given state values is infinite
     * @return for each state of the given set the expected number of times that state is visited.
     */
    std::vector<ValueType> computeValueForStateSet(storm::Environment const& env, storm::storage::BitVector const& stateSetAsBitVector,
                                                   std::vector<ExtendedValueType> const& stateValues) const;

    storm::storage::SparseMatrix<ValueType> const& transitionMatrix;
    storm::OptionalRef<std::vector<ValueType> const> exitRates;

    storm::OptionalRef<storm::storage::SparseMatrix<ValueType> const> backwardTransitions;
    std::unique_ptr<storm::storage::SparseMatrix<ValueType>> computedBackwardTransitions;

    storm::OptionalRef<storm::storage::StronglyConnectedComponentDecomposition<ValueType> const> sccDecomposition;
    std::unique_ptr<storm::storage::StronglyConnectedComponentDecomposition<ValueType>> computedSccDecomposition;

    storm::storage::BitVector nonBsccStates;
};

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm