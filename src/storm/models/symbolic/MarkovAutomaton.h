#pragma once

#include <optional>

#include "storm/models/symbolic/NondeterministicModel.h"

namespace storm {
namespace models {
namespace symbolic {

/*!
 * This class represents a discrete-time Markov decision process.
 */
template<storm::dd::DdType Type, typename ValueType = double>
class MarkovAutomaton : public NondeterministicModel<Type, ValueType> {
   public:
    typedef typename NondeterministicModel<Type, ValueType>::RewardModelType RewardModelType;

    MarkovAutomaton(MarkovAutomaton<Type, ValueType> const& other) = default;
    MarkovAutomaton& operator=(MarkovAutomaton<Type, ValueType> const& other) = default;
    MarkovAutomaton(MarkovAutomaton<Type, ValueType>&& other) = default;
    MarkovAutomaton& operator=(MarkovAutomaton<Type, ValueType>&& other) = default;

    /*!
     * Constructs a model from the given data.
     *
     * @param manager The manager responsible for the decision diagrams.
     * @param markovianMarker A DD that can be used to split the Markovian and probabilistic behavior.
     * @param reachableStates A DD representing the reachable states.
     * @param initialStates A DD representing the initial states of the model.
     * @param deadlockStates A DD representing the deadlock states of the model.
     * @param transitionMatrix The matrix representing the transitions in the model as a probabilistic matrix.
     * @param rowVariables The set of row meta variables used in the DDs.
     * @param rowExpressionAdapter An object that can be used to translate expressions in terms of the row
     * meta variables.
     * @param columVariables The set of column meta variables used in the DDs.
     * @param rowColumnMetaVariablePairs All pairs of row/column meta variables.
     * @param nondeterminismVariables The meta variables used to encode the nondeterminism in the model.
     * @param labelToExpressionMap A mapping from label names to their defining expressions.
     * @param rewardModels The reward models associated with the model.
     */
    MarkovAutomaton(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> markovianMarker, storm::dd::Bdd<Type> reachableStates,
                    storm::dd::Bdd<Type> initialStates, storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
                    std::set<storm::expressions::Variable> const& rowVariables,
                    std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter,
                    std::set<storm::expressions::Variable> const& columnVariables,
                    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                    std::set<storm::expressions::Variable> const& nondeterminismVariables,
                    std::map<std::string, storm::expressions::Expression> labelToExpressionMap = std::map<std::string, storm::expressions::Expression>(),
                    std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Constructs a model from the given data.
     *
     * @param manager The manager responsible for the decision diagrams.
     * @param markovianMarker A DD that can be used to split the Markovian and probabilistic behavior.
     * @param reachableStates A DD representing the reachable states.
     * @param initialStates A DD representing the initial states of the model.
     * @param deadlockStates A DD representing the deadlock states of the model.
     * @param transitionMatrix The matrix representing the transitions in the model. Must be a probabilistic matrix if exitRateVector is given.
     * If exitRateVector is not given, transitions at Markovian choices are interpreted as transition rates.
     * @param rowVariables The set of row meta variables used in the DDs.
     * @param columVariables The set of column meta variables used in the DDs.
     * @param rowColumnMetaVariablePairs All pairs of row/column meta variables.
     * @param nondeterminismVariables The meta variables used to encode the nondeterminism in the model.
     * @param labelToBddMap A mapping from label names to their defining BDDs.
     * @param rewardModels The reward models associated with the model.
     * @param exitRateVector The exit rate of each Markovian state. If given, the transition matrix must be probabilistic.
     */
    MarkovAutomaton(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> markovianMarker, storm::dd::Bdd<Type> reachableStates,
                    storm::dd::Bdd<Type> initialStates, storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
                    std::set<storm::expressions::Variable> const& rowVariables, std::set<storm::expressions::Variable> const& columnVariables,
                    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                    std::set<storm::expressions::Variable> const& nondeterminismVariables,
                    std::map<std::string, storm::dd::Bdd<Type>> labelToBddMap = std::map<std::string, storm::dd::Bdd<Type>>(),
                    std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>(),
                    std::optional<storm::dd::Add<Type, ValueType>> exitRateVector = std::nullopt);

    storm::dd::Bdd<Type> const& getMarkovianMarker() const;
    storm::dd::Bdd<Type> const& getMarkovianStates() const;
    storm::dd::Bdd<Type> const& getMarkovianChoices() const;
    storm::dd::Bdd<Type> const& getProbabilisticStates() const;

    bool hasHybridStates() const;
    bool isClosed();

    MarkovAutomaton<Type, ValueType> close();

    storm::dd::Add<Type, ValueType> const& getExitRateVector() const;

    template<typename NewValueType>
    std::shared_ptr<MarkovAutomaton<Type, NewValueType>> toValueType() const;

   private:
    /*!
     * Computes the Markovian choices/states and probabilistic states from the markovian marker and the
     * (already normalized) transition matrix. Does not touch the exit rate vector or the transition matrix.
     */
    void computeMarkovianChoicesAndStates();

    /*!
     * Derive the state exit rates and transition probabilities from a transition matrix with rates
     */
    void convertToExitRatesAndProbabilities();

    storm::dd::Bdd<Type> markovianMarker;
    storm::dd::Bdd<Type> markovianStates;
    storm::dd::Bdd<Type> markovianChoices;
    storm::dd::Bdd<Type> probabilisticStates;
    storm::dd::Add<Type, ValueType> exitRateVector;
};

}  // namespace symbolic
}  // namespace models
}  // namespace storm
