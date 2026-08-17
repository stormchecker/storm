#pragma once

#include "storm/models/symbolic/DeterministicModel.h"

namespace storm {
namespace models {
namespace symbolic {

/*!
 * This class represents a continuous-time Markov chain.
 */
template<storm::dd::DdType Type, typename ValueType = double>
class Ctmc : public DeterministicModel<Type, ValueType> {
   public:
    typedef typename DeterministicModel<Type, ValueType>::RewardModelType RewardModelType;

    Ctmc(Ctmc<Type, ValueType> const& other) = default;
    Ctmc& operator=(Ctmc<Type, ValueType> const& other) = default;

    Ctmc(Ctmc<Type, ValueType>&& other) = default;
    Ctmc& operator=(Ctmc<Type, ValueType>&& other) = default;

    /*!
     * Constructs a model from the given data.
     *
     * @param manager The manager responsible for the decision diagrams.
     * @param reachableStates A DD representing the reachable states.
     * @param initialStates A DD representing the initial states of the model.
     * @param deadlockStates A DD representing the deadlock states of the model.
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param rowVariables The set of row meta variables used in the DDs.
     * @param rowExpressionAdapter An object that can be used to translate expressions in terms of the row
     * meta variables.
     * @param columVariables The set of column meta variables used in the DDs.
     * @param rowColumnMetaVariablePairs All pairs of row/column meta variables.
     * @param labelToExpressionMap A mapping from label names to their defining expressions.
     * @param rewardModels The reward models associated with the model.
     */
    Ctmc(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
         storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix, std::set<storm::expressions::Variable> const& rowVariables,
         std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter,
         std::set<storm::expressions::Variable> const& columnVariables,
         std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
         std::map<std::string, storm::expressions::Expression> labelToExpressionMap = std::map<std::string, storm::expressions::Expression>(),
         std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Constructs a model from the given data.
     *
     * @param manager The manager responsible for the decision diagrams.
     * @param reachableStates A DD representing the reachable states.
     * @param initialStates A DD representing the initial states of the model.
     * @param deadlockStates A DD representing the deadlock states of the model.
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param exitRateVector The vector specifying the exit rates for the states.
     * @param rowVariables The set of row meta variables used in the DDs.
     * @param rowExpressionAdapter An object that can be used to translate expressions in terms of the row
     * meta variables.
     * @param columVariables The set of column meta variables used in the DDs.
     * @param rowColumnMetaVariablePairs All pairs of row/column meta variables.
     * @param labelToExpressionMap A mapping from label names to their defining expressions.
     * @param rewardModels The reward models associated with the model.
     */
    Ctmc(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
         storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix, boost::optional<storm::dd::Add<Type, ValueType>> exitRateVector,
         std::set<storm::expressions::Variable> const& rowVariables,
         std::shared_ptr<storm::adapters::AddExpressionAdapter<Type, ValueType>> rowExpressionAdapter,
         std::set<storm::expressions::Variable> const& columnVariables,
         std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
         std::map<std::string, storm::expressions::Expression> labelToExpressionMap = std::map<std::string, storm::expressions::Expression>(),
         std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Constructs a model from the given data.
     *
     * @param manager The manager responsible for the decision diagrams.
     * @param reachableStates A DD representing the reachable states.
     * @param initialStates A DD representing the initial states of the model.
     * @param deadlockStates A DD representing the deadlock states of the model.
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param rowVariables The set of row meta variables used in the DDs.
     * @param columVariables The set of column meta variables used in the DDs.
     * @param rowColumnMetaVariablePairs All pairs of row/column meta variables.
     * @param labelToBddMap A mapping from label names to their defining BDDs.
     * @param rewardModels The reward models associated with the model.
     */
    Ctmc(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
         storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix, std::set<storm::expressions::Variable> const& rowVariables,
         std::set<storm::expressions::Variable> const& columnVariables,
         std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
         std::map<std::string, storm::dd::Bdd<Type>> labelToBddMap = std::map<std::string, storm::dd::Bdd<Type>>(),
         std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Constructs a model from the given data.
     *
     * @param manager The manager responsible for the decision diagrams.
     * @param reachableStates A DD representing the reachable states.
     * @param initialStates A DD representing the initial states of the model.
     * @param deadlockStates A DD representing the deadlock states of the model.
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param exitRateVector The vector specifying the exit rates for the states.
     * @param rowVariables The set of row meta variables used in the DDs.
     * @param columVariables The set of column meta variables used in the DDs.
     * @param rowColumnMetaVariablePairs All pairs of row/column meta variables.
     * @param labelToBddMap A mapping from label names to their defining BDDs.
     * @param rewardModels The reward models associated with the model.
     */
    Ctmc(std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates, storm::dd::Bdd<Type> initialStates,
         storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix, boost::optional<storm::dd::Add<Type, ValueType>> exitRateVector,
         std::set<storm::expressions::Variable> const& rowVariables, std::set<storm::expressions::Variable> const& columnVariables,
         std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
         std::map<std::string, storm::dd::Bdd<Type>> labelToBddMap = std::map<std::string, storm::dd::Bdd<Type>>(),
         std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Retrieves the exit rate vector of the CTMC.
     *
     * @return The exit rate vector.
     */
    storm::dd::Add<Type, ValueType> const& getExitRateVector() const;

    virtual void reduceToStateBasedRewards() override;

    /*!
     * @return the probabilistic transition matrix P
     * @note getTransitionMatrix() retrieves the exit rate matrix R, where R(s,s') = r(s) * P(s,s')
     */
    storm::dd::Add<Type, ValueType> computeProbabilityMatrix() const;

    template<typename NewValueType>
    std::shared_ptr<Ctmc<Type, NewValueType>> toValueType() const;

   private:
    mutable boost::optional<storm::dd::Add<Type, ValueType>> exitRates;
};

}  // namespace symbolic
}  // namespace models
}  // namespace storm
