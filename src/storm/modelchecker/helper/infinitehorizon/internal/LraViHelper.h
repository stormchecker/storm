#pragma once

#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/solver/multiplier/Multiplier.h"
#include "storm/storage/SparseMatrix.h"

namespace storm {
class Environment;

namespace modelchecker {
namespace helper {
namespace internal {

/*!
 * Specifies differnt kinds of transition types with which this helper can be used
 * Ts means timed states (cf. Markovian states in a Markov Automaton) and Is means instant states (cf. probabilistic states in a Markov automaton).
 * The way to think about this is that time can only pass in a timed state, whereas transitions emerging from an instant state fire immediately
 * In an MDP, all states are seen as timed.
 * In this enum, we also specify whether there can be a nondeterministic choice at the corresponding states or not.
 */
enum class LraViTransitionsType {
    DetTsNoIs,      /// deterministic choice at timed states, no instant states (as in DTMCs and CTMCs)
    DetTsNondetIs,  /// deterministic choice at timed states, nondeterministic choice at instant states (as in Markov Automata)
    DetTsDetIs,     /// deterministic choice at timed states, deterministic choice at instant states (as in Markov Automata without any nondeterminisim)
    NondetTsNoIs    /// nondeterministic choice at timed states, no instant states (as in MDPs)
};

/*!
 * Helper class that performs iterations of the value iteration method.
 * The purpose of the template parameters ComponentType and TransitionsType are used to make this work for various model types.
 *
 * @see Ashok et al.: Value Iteration for Long-Run Average Reward in Markov Decision Processes (CAV'17), https://doi.org/10.1007/978-3-319-63387-9_10
 * @see Butkova, Wimmer, Hermanns: Long-Run Rewards for Markov Automata (TACAS'17), https://doi.org/10.1007/978-3-662-54580-5_11
 *
 * @tparam ValueType The type of a value
 * @tparam ComponentType The type of a 'bottom component' of the model (e.g. a BSCC (for purely deterministic models) or a MEC (for models with potential
 * nondeterminism).
 * @tparam TransitionsType The kind of transitions that occur.
 */
template<typename ValueType, typename ComponentType, LraViTransitionsType TransitionsType>
class LraViHelper {
   public:
    /// Function mapping from indices to values
    typedef std::function<ValueType(uint64_t)> ValueGetter;

    /*!
     * Initializes a new VI helper for the provided MEC or BSCC
     * @param component the MEC or BSCC
     * @param transitionMatrix The transition matrix of the input model
     * @param aperiodicFactor a non-zero factor that is used for making the MEC aperiodic (by adding selfloops to each state)
     * @param timedStates States in which time can pass (Markovian states in a Markov automaton). If nullptr, it is assumed that all states are timed states
     * @param exitRates The exit rates of the timed states (relevant for continuous time models). If nullptr, all rates are assumed to be 1 (which corresponds
     * to a discrete time model)
     * @note All indices and vectors must be w.r.t. the states as described by the provided transition matrix
     */
    LraViHelper(ComponentType const& component, storm::storage::SparseMatrix<ValueType> const& transitionMatrix, ValueType const& aperiodicFactor,
                storm::storage::BitVector const* timedStates = nullptr, std::vector<ValueType> const* exitRates = nullptr);

    /*!
     * Performs value iteration with the given state- and action values.
     * @param env The environment, containing information on the precision of this computation.
     * @param stateValueGetter function that returns for each state index (w.r.t. the input transition matrix) the reward for staying in state. Will only be
     * called for timed states.
     * @param actionValueGetter function that returns for each global choice index (w.r.t. the input transition matrix) the reward for taking that choice
     * @param exitRates (as in the constructor)
     * @param dir Optimization direction. Must be not nullptr in case of nondeterminism
     * @param choices if not nullptr, the optimal choices will be inserted in this vector. The vector's size must then be equal to the number of row groups of
     * the input transition matrix.
     * @return The (optimal) long run average value of the specified component.
     * @note it is possible to call this method multiple times with different values. However, other changes to the environment or the optimization direction
     * might not have the expected effect due to caching.
     */
    ValueType performValueIteration(Environment const& env, ValueGetter const& stateValueGetter, ValueGetter const& actionValueGetter,
                                    std::vector<ValueType> const* exitRates = nullptr, storm::solver::OptimizationDirection const* dir = nullptr,
                                    std::vector<uint64_t>* choices = nullptr);

   private:
    /*!
     * Initializes the value iterations with the provided values.
     * Resets all information from potential previous calls.
     * Must be called before the first call to performIterationStep.
     * @param stateValueGetter Function that returns for each state index (w.r.t. the input transitions) the value (e.g. reward) for that state
     * @param stateValueGetter Function that returns for each global choice index (w.r.t. the input transitions) the value (e.g. reward) for that choice
     */
    void initializeNewValues(ValueGetter const& stateValueGetter, ValueGetter const& actionValueGetter, std::vector<ValueType> const* exitRates = nullptr);

    /*!
     * Performs a single iteration step.
     * @param env The environment.
     * @param dir The optimization direction. Has to be given if there is nondeterminism (otherwise it will be ignored)
     * @param choices If given, the optimal choices will be inserted at the appropriate states.
     *                Note that these choices will be inserted w.r.t. the original model states/choices, i.e. the size of the vector should match the
     * state-count of the input model
     * @pre when calling this the first time, initializeNewValues must have been called before. Moreover, prepareNextIteration must be called between two calls
     * of this.
     */
    void performIterationStep(Environment const& env, storm::solver::OptimizationDirection const* dir = nullptr, std::vector<uint64_t>* choices = nullptr);

    struct ConvergenceCheckResult {
        bool isPrecisionAchieved;
        ValueType currentValue;
    };

    /*!
     * Checks whether the curently computed value achieves the desired precision
     */
    ConvergenceCheckResult checkConvergence(bool relative, ValueType precision) const;

    /*!
     * Must be called between two calls of performIterationStep.
     */
    void prepareNextIteration(Environment const& env);

    /// Prepares the necessary solvers and multipliers for doing the iterations.
    void prepareSolversAndMultipliers(Environment const& env, storm::solver::OptimizationDirection const* dir = nullptr);

    void setInputModelChoices(std::vector<uint64_t>& choices, std::vector<uint64_t> const& localMecChoices, bool setChoiceZeroToMarkovianStates = false,
                              bool setChoiceZeroToProbabilisticStates = false) const;

    /// Returns true iff the given state is a timed state
    bool isTimedState(uint64_t const& inputModelStateIndex) const;

    /// The result for timed states of the most recent iteration
    std::vector<ValueType>& xNew();
    std::vector<ValueType> const& xNew() const;

    /// The result for timed states of the previous iteration
    std::vector<ValueType>& xOld();
    std::vector<ValueType> const& xOld() const;

    /// @return true iff there potentially is a nondeterministic choice at timed states
    bool nondetTs() const;

    /// @return true iff there potentially is a nondeterministic choice at instant states. Returns false if there are no instant states.
    bool nondetIs() const;

    void setComponent(ComponentType component);

    // We need to make sure that states/choices will be processed in ascending order
    typedef std::map<uint64_t, std::set<uint64_t>> InternalComponentType;

    InternalComponentType _component;
    storm::storage::SparseMatrix<ValueType> const& _transitionMatrix;
    storm::storage::BitVector const* _timedStates;  // e.g. Markovian states of a Markov automaton.
    bool _hasInstantStates;
    ValueType _uniformizationRate;
    storm::storage::SparseMatrix<ValueType> _tsTransitions, _tsToIsTransitions, _isTransitions, _isToTsTransitions;
    std::vector<ValueType> _tsx1, _tsx2, _tsChoiceValues;
    bool _tsx1IsCurrent;
    std::vector<ValueType> _isx, _isb, _isChoiceValues;
    std::unique_ptr<storm::solver::Multiplier<ValueType>> _tsMultiplier, _tsToIsMultiplier, _isToTsMultiplier;
    std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> _nondetIsSolver;
    std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> _detIsSolver;
    std::unique_ptr<storm::Environment> _isSolverEnv;
};
}  // namespace internal
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm