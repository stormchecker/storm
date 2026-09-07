#pragma once
#include "storm/modelchecker/helper/SingleValueModelCheckerHelper.h"
#include "storm/storage/Decomposition.h"
#include "storm/storage/MaximalEndComponent.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/StronglyConnectedComponent.h"

namespace storm {
class Environment;

namespace models {
namespace sparse {
template<typename VT>
class StandardRewardModel;
}
}  // namespace models

namespace modelchecker {
namespace helper {

/*!
 * Helper class for model checking queries that depend on the long run behavior of the (nondeterministic) system.
 * @tparam ValueType the type a value can have
 * @tparam Nondeterministic true if there is nondeterminism in the Model (MDP or MA)
 */
template<typename ValueType, bool Nondeterministic>
class SparseInfiniteHorizonHelper : public SingleValueModelCheckerHelper<ValueType, storm::models::ModelRepresentation::Sparse> {
   public:
    /*!
     * The type of a component in which the system resides in the long run (BSCC for deterministic models, MEC for nondeterministic models)
     */
    using LongRunComponentType =
        typename std::conditional<Nondeterministic, storm::storage::MaximalEndComponent, storm::storage::StronglyConnectedComponent>::type;

    /*!
     * Function mapping from indices to values
     */
    typedef std::function<ValueType(uint64_t)> ValueGetter;

    /*!
     * Initializes the helper for a discrete time (i.e. DTMC, MDP)
     */
    SparseInfiniteHorizonHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix);

    /*!
     * Initializes the helper for continuous time (i.e. MA)
     */
    SparseInfiniteHorizonHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& markovianStates,
                                std::vector<ValueType> const& exitRates);

    /*!
     * Initializes the helper for continuous time (i.e. CTMC)
     */
    SparseInfiniteHorizonHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType> const& exitRates);

    /*!
     * Provides backward transitions that can be used during the computation.
     * Providing them is optional. If they are not provided, they will be computed internally
     * Be aware that this class does not take ownership, i.e. the caller has to make sure that the reference to the backwardstransitions remains valid.
     */
    void provideBackwardTransitions(storm::storage::SparseMatrix<ValueType> const& backwardsTransitions);

    /*!
     * Provides the decomposition into long run components (BSCCs/MECs) that can be used during the computation.
     * Providing the decomposition is optional. If it is not provided, they will be computed internally.
     * Be aware that this class does not take ownership, i.e. the caller has to make sure that the reference to the decomposition remains valid.
     */
    void provideLongRunComponentDecomposition(storm::storage::Decomposition<LongRunComponentType> const& decomposition);

    /*!
     * Computes the long run average probabilities, i.e., the fraction of the time we are in a psiState
     * @return a value for each state
     */
    std::vector<ValueType> computeLongRunAverageProbabilities(Environment const& env, storm::storage::BitVector const& psiStates);

    /*!
     * Computes the long run average rewards, i.e., the average reward collected per time unit
     * @return a value for each state
     */
    std::vector<ValueType> computeLongRunAverageRewards(Environment const& env, storm::models::sparse::StandardRewardModel<ValueType> const& rewardModel);

    /*!
     * Computes the long run average value given the provided state and action-based rewards.
     * @param stateValues a vector containing a value for every state
     * @param actionValues a vector containing a value for every choice
     * @return a value for each state
     */
    std::vector<ValueType> computeLongRunAverageValues(Environment const& env, std::vector<ValueType> const* stateValues = nullptr,
                                                       std::vector<ValueType> const* actionValues = nullptr);

    /*!
     * Computes the long run average value given the provided state and action based rewards
     * @param stateValuesGetter a function returning a value for a given state index
     * @param actionValuesGetter a function returning a value for a given (global) choice index
     * @return a value for each state
     */
    std::vector<ValueType> computeLongRunAverageValues(Environment const& env, ValueGetter const& stateValuesGetter, ValueGetter const& actionValuesGetter);

    /*!
     * @param stateValuesGetter a function returning a value for a given state index
     * @param actionValuesGetter a function returning a value for a given (global) choice index
     * @return the (unique) optimal LRA value for the given component.
     * @post if scheduler production is enabled and Nondeterministic is true, getProducedOptimalChoices() contains choices for the states of the given component
     * which yield the returned LRA value. Choices for states outside of the component are not affected.
     */
    virtual ValueType computeLraForComponent(Environment const& env, ValueGetter const& stateValuesGetter, ValueGetter const& actionValuesGetter,
                                             LongRunComponentType const& component) = 0;

   protected:
    /*!
     * @return true iff this is a computation on a continuous time model (i.e. CTMC, MA)
     */
    bool isContinuousTime() const;

    /*!
     * @post _backwardTransitions points to backward transitions.
     */
    void createBackwardTransitions();

    /*!
     * @post _longRunComponentDecomposition points to a decomposition of the long run components (MECs, BSCCs)
     */
    virtual void createDecomposition() = 0;

    /*!
     * @pre if scheduler production is enabled and Nondeterministic is true, a choice for each state within a component must be set such that the choices yield
     * optimal values w.r.t. the individual components.
     * @return Lra values for each state
     * @post if scheduler production is enabled and Nondeterministic is true, getProducedOptimalChoices() contains choices for all input model states which
     * yield the returned LRA values.
     */
    virtual std::vector<ValueType> buildAndSolveSsp(Environment const& env, std::vector<ValueType> const& mecLraValues) = 0;

    storm::storage::SparseMatrix<ValueType> const& _transitionMatrix;
    storm::storage::BitVector const* _markovianStates;
    std::vector<ValueType> const* _exitRates;

    storm::storage::SparseMatrix<ValueType> const* _backwardTransitions;
    storm::storage::Decomposition<LongRunComponentType> const* _longRunComponentDecomposition;
    std::unique_ptr<storm::storage::SparseMatrix<ValueType>> _computedBackwardTransitions;
    std::unique_ptr<storm::storage::Decomposition<LongRunComponentType>> _computedLongRunComponentDecomposition;

    boost::optional<std::vector<uint64_t>> _producedOptimalChoices;
};

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm