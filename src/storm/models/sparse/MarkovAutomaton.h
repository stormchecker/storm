#pragma once

#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/NondeterministicModel.h"

namespace storm {
namespace models {
namespace sparse {

/*!
 * This class represents a Markov automaton.
 */
template<class ValueType, typename RewardModelType = StandardRewardModel<ValueType>>
class MarkovAutomaton : public NondeterministicModel<ValueType, RewardModelType> {
   public:
    /*!
     * Constructs a model from the given data.
     *
     * For hybrid states (i.e., states with Markovian and probabilistic transitions), it is assumed that the first
     * choice corresponds to the markovian transitions.
     *
     * @param transitionMatrix The matrix representing the transitions in the model in terms of rates (Markovian choices) and probabilities (probabilistic
     * choices).
     * @param stateLabeling The labeling of the states.
     * @param markovianStates A bit vector indicating the Markovian states of the automaton (i.e., states with at least one markovian transition).
     * @param rewardModels A mapping of reward model names to reward models.
     */
    MarkovAutomaton(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::models::sparse::StateLabeling const& stateLabeling,
                    storm::storage::BitVector const& markovianStates,
                    std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Constructs a model from the given data.
     *
     * For hybrid states (i.e., states with Markovian and probabilistic transitions), it is assumed that the first
     * choice corresponds to the markovian transitions.
     *
     * @param transitionMatrix The matrix representing the transitions in the model in terms of rates (Markovian choices) and probabilities (probabilistic
     * choices).
     * @param stateLabeling The labeling of the states.
     * @param markovianStates A bit vector indicating the Markovian states of the automaton (i.e., states with at least one markovian transition).
     * @param rewardModels A mapping of reward model names to reward models.
     */
    MarkovAutomaton(storm::storage::SparseMatrix<ValueType>&& transitionMatrix, storm::models::sparse::StateLabeling&& stateLabeling,
                    storm::storage::BitVector&& markovianStates,
                    std::unordered_map<std::string, RewardModelType>&& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Constructs a model from the given data.
     *
     * @param components The components for this model.
     */
    MarkovAutomaton(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components);
    MarkovAutomaton(storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components);

    MarkovAutomaton(MarkovAutomaton<ValueType, RewardModelType> const& other) = default;
    MarkovAutomaton& operator=(MarkovAutomaton<ValueType, RewardModelType> const& other) = default;

    MarkovAutomaton(MarkovAutomaton<ValueType, RewardModelType>&& other) = default;
    MarkovAutomaton& operator=(MarkovAutomaton<ValueType, RewardModelType>&& other) = default;

    /*!
     * Retrieves whether the Markov automaton is closed.
     *
     * @return True iff the Markov automaton is closed.
     */
    bool isClosed() const;

    /*!
     * Retrieves whether the Markov automaton contains Zeno cycles.
     * A Zeno cyle contains only non-Markovian states and allows to visit infinitely many states in finite time.
     *
     * @return True iff the Markov automaton contains Zeno cycles.
     */
    bool containsZenoCycle() const;

    /*!
     * Retrieves whether the given state is a hybrid state, i.e. Markovian and probabilistic.
     *
     * @param state The state for which determine whether it's hybrid.
     * @return True iff the state is hybrid.
     */
    bool isHybridState(storm::storage::sparse::state_type state) const;

    /*!
     * Retrieves whether the given state is a Markovian state.
     *
     * @param state The state for which determine whether it's Markovian.
     * @return True iff the state is Markovian.
     */
    bool isMarkovianState(storm::storage::sparse::state_type state) const;

    /*!
     * Retrieves whether the given state is a probabilistic state.
     *
     * @param state The state for which determine whether it's probabilistic.
     * @return True iff the state is probabilistic.
     */
    bool isProbabilisticState(storm::storage::sparse::state_type state) const;

    /*!
     * Retrieves the vector representing the exit rates of the states.
     *
     * @return The exit rate vector of the model.
     */
    std::vector<ValueType> const& getExitRates() const;

    /*!
     * Retrieves the vector representing the exit rates of the states.
     *
     * @return The exit rate vector of the model.
     */
    std::vector<ValueType>& getExitRates();

    /*!
     * Retrieves the exit rate of the given state.
     *
     * @param state The state for which retrieve the exit rate.
     * @return The exit rate of the state.
     */
    ValueType const& getExitRate(storm::storage::sparse::state_type state) const;

    /*!
     * Retrieves the maximal exit rate over all states of the model.
     *
     * @return The maximal exit rate of any state of the model.
     */
    ValueType getMaximalExitRate() const;

    /*!
     * Retrieves the set of Markovian states of the model.
     *
     * @return A bit vector representing the Markovian states of the model.
     */
    storm::storage::BitVector const& getMarkovianStates() const;

    /*!
     * Closes the Markov automaton. That is, this applies the maximal progress assumption to all hybrid states.
     */
    void close();

    /*!
     * Determines whether the Markov automaton can be converted to a CTMC without changing any measures.
     */
    bool isConvertibleToCtmc() const;

    bool hasOnlyTrivialNondeterminism() const;

    /*!
     * Convert the MA to a CTMC. May only be called if the MA is convertible to a CTMC.
     *
     * @return The resulting CTMC.
     */
    std::shared_ptr<storm::models::sparse::Ctmc<ValueType, RewardModelType>> convertToCtmc() const;

    virtual void printModelInformationToStream(std::ostream& out) const override;

   private:
    /*!
     * Under the assumption that the Markovian choices of this Markov automaton are expressed in terms of
     * rates in the transition matrix, this procedure turns the rates into the corresponding probabilities by
     * dividing each entry by the sum of the rates for that choice.
     * Also sets the exitRates accordingly and throws an exception if the values for a non-markovian choice do not sum up to one.
     */
    void turnRatesToProbabilities();

    /*!
     * Checks whether the automaton is closed by actually looking at the transition information.
     */
    bool checkIsClosed() const;

    /*!
     * Checks whether a Zeno cycle is present.
     *
     * @return True iff a Zeno cycle is present.
     */
    bool checkContainsZenoCycle() const;

    // A bit vector representing the set of Markovian states.
    storm::storage::BitVector markovianStates;

    // A vector storing the exit rates of all states of the model.
    std::vector<ValueType> exitRates;

    // A flag indicating whether the Markov automaton has been closed, which is typically a prerequisite for model checking.
    bool closed;

    // A flag indicating whether the Markov automaton contains Zeno cycles.
    mutable boost::optional<bool> hasZenoCycle;
};

}  // namespace sparse
}  // namespace models
}  // namespace storm
