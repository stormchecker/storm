#pragma once
#include <optional>
#include "storm/modelchecker/helper/infinitehorizon/SparseInfiniteHorizonHelper.h"

namespace storm {

namespace storage {
template<typename VT>
class Scheduler;
}

namespace modelchecker {
namespace helper {

/*!
 * Helper class for model checking queries that depend on the long run behavior of the (nondeterministic) system.
 * @tparam ValueType the type a value can have
 */
template<typename ValueType>
class SparseNondeterministicInfiniteHorizonHelper : public SparseInfiniteHorizonHelper<ValueType, true> {
   public:
    /*!
     * Function mapping from indices to values
     */
    typedef typename SparseInfiniteHorizonHelper<ValueType, true>::ValueGetter ValueGetter;

    /*!
     * Initializes the helper for a discrete time model (i.e. MDP)
     */
    SparseNondeterministicInfiniteHorizonHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix);

    /*!
     * Initializes the helper for a continuous time model (i.e. MA)
     */
    SparseNondeterministicInfiniteHorizonHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                storm::storage::BitVector const& markovianStates, std::vector<ValueType> const& exitRates);

    /*!
     * @pre before calling this, a computation call should have been performed during which scheduler production was enabled.
     * @return the produced scheduler of the most recent call.
     */
    std::vector<uint64_t> const& getProducedOptimalChoices() const;

    /*!
     * @pre before calling this, a computation call should have been performed during which scheduler production was enabled.
     * @return the produced scheduler of the most recent call.
     */
    std::vector<uint64_t>& getProducedOptimalChoices();

    /*!
     * @pre before calling this, a computation call should have been performed during which scheduler production was enabled.
     * @return a new scheduler containing optimal choices for each state that yield the long run average values of the most recent call.
     */
    storm::storage::Scheduler<ValueType> extractScheduler() const;

    /*!
     * @param stateValuesGetter a function returning a value for a given state index
     * @param actionValuesGetter a function returning a value for a given (global) choice index
     * @return the (unique) optimal LRA value for the given component.
     * @post if scheduler production is enabled and Nondeterministic is true, getProducedOptimalChoices() contains choices for the states of the given component
     * which yield the returned LRA value. Choices for states outside of the component are not affected.
     */
    virtual ValueType computeLraForComponent(Environment const& env, ValueGetter const& stateValuesGetter, ValueGetter const& actionValuesGetter,
                                             storm::storage::MaximalEndComponent const& component) override;

   protected:
    virtual void createDecomposition() override;

    /*!
     * Checks if the component consists of a single state and if so, returns the value of that state.
     */
    std::optional<ValueType> computeLraForTrivialMec(Environment const& env, ValueGetter const& stateValuesGetter, ValueGetter const& actionValuesGetter,
                                                     storm::storage::MaximalEndComponent const& mec);
    /*!
     * Checks if we can determine via graph analysis, that the LRA value of the component is exactly zero.
     */
    std::optional<ValueType> computeLraForZeroMec(Environment const& env, ValueGetter const& stateValuesGetter, ValueGetter const& actionValuesGetter,
                                                  storm::storage::MaximalEndComponent const& mec);

    /*!
     * As computeLraForMec but uses value iteration as a solution method (independent of what is set in env)
     */
    ValueType computeLraForMecVi(Environment const& env, ValueGetter const& stateValuesGetter, ValueGetter const& actionValuesGetter,
                                 storm::storage::MaximalEndComponent const& mec);

    /*!
     * As computeLraForMec but uses linear programming as a solution method (independent of what is set in env)
     * @see Guck et al.: Modelling and Analysis of Markov Reward Automata (ATVA'14), https://doi.org/10.1007/978-3-319-11936-6_13
     */
    ValueType computeLraForMecLp(Environment const& env, ValueGetter const& stateValuesGetter, ValueGetter const& actionValuesGetter,
                                 storm::storage::MaximalEndComponent const& mec);

    std::pair<storm::storage::SparseMatrix<ValueType>, std::vector<ValueType>> buildSspMatrixVector(
        std::vector<ValueType> const& mecLraValues, std::vector<uint64_t> const& inputToSspStateMap, storm::storage::BitVector const& statesNotInComponent,
        uint64_t numberOfNonComponentStates, std::vector<std::pair<uint64_t, uint64_t>>* sspComponentExitChoicesToOriginalMap);

    /*!
     * @pre a choice for each state within a component must be set such that the choices yield optimal values w.r.t. the individual components.
     * Translates optimal choices for MECS and SSP to the original model.
     * @post getProducedOptimalChoices() contains choices for all input model states which yield the returned LRA values.
     */
    void constructOptimalChoices(std::vector<uint64_t> const& sspChoices, storm::storage::SparseMatrix<ValueType> const& sspMatrix,
                                 std::vector<uint64_t> const& inputToSspStateMap, storm::storage::BitVector const& statesNotInComponent,
                                 uint64_t numberOfNonComponentStates, std::vector<std::pair<uint64_t, uint64_t>> const& sspComponentExitChoicesToOriginalMap);

    /*!
     * @pre if scheduler production is enabled a choice for each state within a component must be set such that the choices yield optimal values w.r.t. the
     * individual components.
     * @return Lra values for each state
     * @post if scheduler production is enabled getProducedOptimalChoices() contains choices for all input model states which yield the returned LRA values.
     */
    virtual std::vector<ValueType> buildAndSolveSsp(Environment const& env, std::vector<ValueType> const& mecLraValues) override;
};

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm