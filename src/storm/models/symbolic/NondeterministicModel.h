#pragma once

#include "storm/models/symbolic/Model.h"

namespace storm {
namespace models {
namespace symbolic {

/*!
 * Base class for all nondeterministic symbolic models.
 */
template<storm::dd::DdType Type, typename ValueType = double>
class NondeterministicModel : public Model<Type, ValueType> {
   public:
    typedef typename Model<Type, ValueType>::RewardModelType RewardModelType;

    NondeterministicModel(NondeterministicModel<Type, ValueType> const& other) = default;
    NondeterministicModel& operator=(NondeterministicModel<Type, ValueType> const& other) = default;

    NondeterministicModel(NondeterministicModel<Type, ValueType>&& other) = default;
    NondeterministicModel& operator=(NondeterministicModel<Type, ValueType>&& other) = default;

    /*!
     * Constructs a model from the given data.
     *
     * @param modelType The type of the model.
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
     * @param nondeterminismVariables The meta variables used to encode the nondeterminism in the model.
     * @param labelToExpressionMap A mapping from label names to their defining expressions.
     * @param rewardModels The reward models associated with the model.
     */
    NondeterministicModel(storm::models::ModelType const& modelType, std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates,
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
     * @param modelType The type of the model.
     * @param manager The manager responsible for the decision diagrams.
     * @param reachableStates A DD representing the reachable states.
     * @param initialStates A DD representing the initial states of the model.
     * @param deadlockStates A DD representing the deadlock states of the model.
     * @param transitionMatrix The matrix representing the transitions in the model.
     * @param rowVariables The set of row meta variables used in the DDs.
     * @param columVariables The set of column meta variables used in the DDs.
     * @param rowColumnMetaVariablePairs All pairs of row/column meta variables.
     * @param nondeterminismVariables The meta variables used to encode the nondeterminism in the model.
     * @param labelToBddMap A mapping from label names to their defining BDDs.
     * @param rewardModels The reward models associated with the model.
     */
    NondeterministicModel(storm::models::ModelType const& modelType, std::shared_ptr<storm::dd::DdManager<Type>> manager, storm::dd::Bdd<Type> reachableStates,
                          storm::dd::Bdd<Type> initialStates, storm::dd::Bdd<Type> deadlockStates, storm::dd::Add<Type, ValueType> transitionMatrix,
                          std::set<storm::expressions::Variable> const& rowVariables, std::set<storm::expressions::Variable> const& columnVariables,
                          std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                          std::set<storm::expressions::Variable> const& nondeterminismVariables,
                          std::map<std::string, storm::dd::Bdd<Type>> labelToBddMap = std::map<std::string, storm::dd::Bdd<Type>>(),
                          std::unordered_map<std::string, RewardModelType> const& rewardModels = std::unordered_map<std::string, RewardModelType>());

    /*!
     * Retrieves the number of nondeterministic choices in the model.
     *
     * @return The number of nondeterministic choices in the model.
     */
    virtual uint_fast64_t getNumberOfChoices() const override;

    /*!
     * Retrieves the meta variables used to encode the nondeterminism in the model.
     *
     * @return The meta variables used to encode the nondeterminism in the model.
     */
    virtual std::set<storm::expressions::Variable> const& getNondeterminismVariables() const override;

    /*!
     * Retrieves a BDD characterizing all illegal nondeterminism encodings in the model.
     *
     * @return A BDD characterizing all illegal nondeterminism encodings in the model.
     */
    storm::dd::Bdd<Type> const& getIllegalMask() const;

    /*!
     * Retrieves a BDD characterizing the illegal successors for each choice.
     *
     * @return A BDD characterizing the illegal successors for each choice.
     */
    storm::dd::Bdd<Type> getIllegalSuccessorMask() const;

    /*!
     * Retrieves the matrix qualitatively (i.e. without probabilities) representing the transitions of the
     * model.
     *
     * @param keepNondeterminism If false, the matrix will abstract from the nondeterminism variables.
     * @return A matrix representing the qualitative transitions of the model.
     */
    virtual storm::dd::Bdd<Type> getQualitativeTransitionMatrix(bool keepNondeterminism = true) const override;

    virtual void printModelInformationToStream(std::ostream& out) const override;

    virtual void reduceToStateBasedRewards() override;

   protected:
    virtual void printDdVariableInformationToStream(std::ostream& out) const override;

    // A mask that characterizes all illegal nondeterministic choices.
    storm::dd::Bdd<Type> illegalMask;

   private:
    void createIllegalMask();

    // The meta variables encoding the nondeterminism in the model.
    std::set<storm::expressions::Variable> nondeterminismVariables;
};

}  // namespace symbolic
}  // namespace models
}  // namespace storm
