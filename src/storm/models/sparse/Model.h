#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "storm/models/Model.h"
#include "storm/models/ModelRepresentation.h"
#include "storm/models/sparse/ChoiceLabeling.h"
#include "storm/models/sparse/ModelForward.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/ChoiceOrigins.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/storage/sparse/StateType.h"
#include "storm/storage/valuations/Valuations.h"

namespace storm {
namespace storage {
class BitVector;
}

namespace models {
namespace sparse {

/*!
 * Base class for all sparse models.
 */
template<class CValueType, class CRewardModelType>
class Model : public storm::models::Model<CValueType> {
   public:
    typedef CValueType ValueType;
    typedef CRewardModelType RewardModelType;
    static const storm::models::ModelRepresentation Representation = ModelRepresentation::Sparse;

    Model(Model<ValueType, RewardModelType> const& other) = default;
    Model& operator=(Model<ValueType, RewardModelType> const& other) = default;

    /*!
     * Constructs a model from the given data.
     *
     * @param modelType the type of this model
     * @param components The components for this model.
     */
    Model(ModelType modelType, storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components);
    Model(ModelType modelType, storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components);

    virtual ~Model() = default;

    /*!
     * Retrieves the backward transition relation of the model, i.e. a set of transitions between states
     * that correspond to the reversed transition relation of this model.
     *
     * @return A sparse matrix that represents the backward transitions of this model.
     */
    storm::storage::SparseMatrix<ValueType> getBackwardTransitions() const;

    /*!
     * Returns the number of states of the model.
     *
     * @return The number of states of the model.
     */
    virtual uint_fast64_t getNumberOfStates() const override;

    /*!
     * Returns the number of (non-zero) transitions of the model.
     *
     * @return The number of (non-zero) transitions of the model.
     */
    virtual uint_fast64_t getNumberOfTransitions() const override;

    /*!
     * Returns the number of choices ine the model.
     *
     * @return The number of choices in of the model.
     */
    virtual uint_fast64_t getNumberOfChoices() const override;

    /*!
     * Retrieves the initial states of the model.
     *
     * @return The initial states of the model represented by a bit vector.
     */
    storm::storage::BitVector const& getInitialStates() const;

    /*!
     * Overwrites the initial states of the model.
     *
     * @param states the new initial states
     */
    void setInitialStates(storm::storage::BitVector const& states);

    /*!
     * Returns the sets of states labeled with the given label.
     *
     * @param label The label for which to get the labeled states.
     * @return The set of states labeled with the requested label in the form of a bit vector.
     */
    storm::storage::BitVector const& getStates(std::string const& label) const;

    /*!
     * Retrieves whether the given label is a valid label in this model.
     *
     * @param label The label to be checked for validity.
     * @return True if the given label is valid in this model.
     */
    bool hasLabel(std::string const& label) const;

    /*!
     * Retrieves the matrix representing the transitions of the model.
     *
     * @return A matrix representing the transitions of the model.
     */
    storm::storage::SparseMatrix<ValueType> const& getTransitionMatrix() const;

    /*!
     * Retrieves the matrix representing the transitions of the model.
     *
     * @return A matrix representing the transitions of the model.
     */
    storm::storage::SparseMatrix<ValueType>& getTransitionMatrix();

    /*!
     * Retrieves the reward models.
     *
     * @return A mapping from reward model names to the reward models.
     */
    std::unordered_map<std::string, RewardModelType> const& getRewardModels() const;

    /*!
     * Retrieves the reward models.
     *
     * @return A mapping from reward model names to the reward models.
     */
    std::unordered_map<std::string, RewardModelType>& getRewardModels();

    /*!
     * Retrieves whether the model has a reward model with the given name.
     *
     * @return True iff the model has a reward model with the given name.
     */
    virtual bool hasRewardModel(std::string const& rewardModelName) const override;

    /*!
     * Retrieves the reward model with the given name, if one exists. Otherwise, an exception is thrown.
     *
     * @return The reward model with the given name, if it exists.
     */
    RewardModelType const& getRewardModel(std::string const& rewardModelName) const;

    /*!
     * Retrieves the reward model with the given name, if one exists. Otherwise, an exception is thrown.
     *
     * @return The reward model with the given name, if it exists.
     */
    RewardModelType& getRewardModel(std::string const& rewardModelName);

    /*!
     * Retrieves the unique reward model, if there exists exactly one. Otherwise, an exception is thrown.
     *
     * @return The requested reward model.
     */
    RewardModelType const& getUniqueRewardModel() const;

    /*!
     * Retrieves the unique reward model, if there exists exactly one. Otherwise, an exception is thrown.
     *
     * @return The requested reward model.
     */
    RewardModelType& getUniqueRewardModel();

    /*!
     * Retrieves whether the model has a unique reward model.
     *
     * @return True iff the model has a unique reward model.
     */
    virtual bool hasUniqueRewardModel() const override;

    /*!
     * Retrieves the name of the unique reward model, if there exists exactly one. Otherwise, an exception is thrown.
     *
     * @return The name of the unique reward model.
     */
    virtual std::string const& getUniqueRewardModelName() const override;

    /*!
     * Retrieves whether the model has at least one reward model.
     *
     * @return True iff the model has a reward model.
     */
    bool hasRewardModel() const;

    /*!
     * Retrieves the number of reward models associated with this model.
     *
     * @return The number of reward models associated with this model.
     */
    uint_fast64_t getNumberOfRewardModels() const;

    /*!
     * Adds a reward model to the model. Notice that this operation is only valid if the reward model matches the number of
     * states and/or choices of the model.
     * Moreover, it is required that no reward model with the same name exists in the model.
     */
    void addRewardModel(std::string const& rewardModelName, RewardModelType const& rewModel);

    /*!
     * Removes the reward model with the given name from the model.
     * @return true, iff such a reward model existed
     */
    bool removeRewardModel(std::string const& rewardModelName);

    /*!
     * Removes all reward models whose name is not in the given set
     */
    void restrictRewardModels(std::set<std::string> const& keptRewardModels);

    /*!
     * Returns the state labeling associated with this model.
     *
     * @return The state labeling associated with this model.
     */
    storm::models::sparse::StateLabeling const& getStateLabeling() const;

    /*!
     * Returns the state labeling associated with this model.
     *
     * @return The state labeling associated with this model.
     */
    storm::models::sparse::StateLabeling& getStateLabeling();

    /*!
     * Retrieves whether this model has a labeling of the choices.
     *
     * @return True iff this model has a labeling of the choices.
     */
    bool hasChoiceLabeling() const;

    /*!
     * Retrieves the labels for the choices of the model. Note that calling this method is only valid if the
     * model has a choice labeling.
     *
     * @return The labels for the choices of the model.
     */
    storm::models::sparse::ChoiceLabeling const& getChoiceLabeling() const;

    /*!
     * Retrieves an optional value that contains the choice labeling if there is one.
     *
     * @return The labels for the choices, if they're saved.
     */
    std::optional<storm::models::sparse::ChoiceLabeling> const& getOptionalChoiceLabeling() const;

    /*!
     * Retrieves an optional value that contains the choice labeling if there is one.
     *
     * @return The labels for the choices, if they're saved.
     */
    std::optional<storm::models::sparse::ChoiceLabeling>& getOptionalChoiceLabeling();

    /*!
     * Retrieves whether this model was build with state valuations.
     *
     * @return True iff this model has state valuations.
     */
    bool hasStateValuations() const;

    /*!
     * Retrieves the valuations of the states of the model. Note that calling this method is only valid if the
     * model has state valuations.
     *
     * @return The valuations of the states of the model.
     */
    storm::storage::sparse::Valuations const& getStateValuations() const;

    /*!
     * Retrieves an optional value that contains the state valuations if there are some.
     *
     * @return The state valuations, if they're saved.
     */
    std::optional<storm::storage::sparse::Valuations> const& getOptionalStateValuations() const;

    /*!
     * Retrieves an optional value that contains the state valuations if there are some.
     *
     * @return The state valuations, if they're saved.
     */
    std::optional<storm::storage::sparse::Valuations>& getOptionalStateValuations();

    /*!
     * Retrieves whether this model was build with choice origins.
     *
     * @return True iff this model has choice origins.
     */
    bool hasChoiceOrigins() const;

    /*!
     * Retrieves the origins of the choices of the model. Note that calling this method is only valid if the
     * model has choice origins.
     *
     * @return The origins of the choices of the model.
     */
    std::shared_ptr<storm::storage::sparse::ChoiceOrigins> const& getChoiceOrigins() const;

    /*!
     * Retrieves an optional value that contains the choice origins if there are some.
     *
     * @return The choice origins, if they're saved.
     */
    std::optional<std::shared_ptr<storm::storage::sparse::ChoiceOrigins>> const& getOptionalChoiceOrigins() const;

    /*!
     * Retrieves an optional value that contains the choice origins if there are some.
     *
     * @return The choice origins, if they're saved.
     */
    std::optional<std::shared_ptr<storm::storage::sparse::ChoiceOrigins>>& getOptionalChoiceOrigins();

    /*!
     * Prints information about the model to the specified stream.
     *
     * @param out The stream the information is to be printed to.
     */
    virtual void printModelInformationToStream(std::ostream& out) const override;

    bool isSinkState(uint64_t sink) const;

    /*!
     * Exports the model to the dot-format and prints the result to the given stream.
     *
     * @param outStream The stream to which the model is to be written.
     * @param maxWidthLabel Maximal width of the labeling before a linebreak is inserted. Value 0 represents no linebreaks.
     * @param includeLabling If set to true, the states will be exported with their labels.
     * @param subsystem If not null, this represents the subsystem that is to be exported.
     * @param firstValue If not null, the values in this vector are attached to the states.
     * @param secondValue If not null, the values in this vector are attached to the states.
     * @param stateColoring If not null, this is a mapping from states to color codes.
     * @param colors A mapping of color codes to color names.
     * @param finalizeOutput A flag that sets whether or not the dot stream is closed with a curly brace.
     * @return A string containing the exported model in dot-format.
     */
    virtual void writeDotToStream(std::ostream& outStream, size_t maxWidthLabel = 30, bool includeLabeling = true,
                                  storm::storage::BitVector const* subsystem = nullptr, std::vector<ValueType> const* firstValue = nullptr,
                                  std::vector<ValueType> const* secondValue = nullptr, std::vector<uint64_t> const* stateColoring = nullptr,
                                  std::vector<std::string> const* colors = nullptr, std::vector<uint_fast64_t>* scheduler = nullptr,
                                  bool finalizeOutput = true) const;

    /*!
     * Writes a JSON representation of the model to the given stream
     */
    virtual void writeJsonToStream(std::ostream& outStream) const;

    /*!
     * Retrieves the set of labels attached to the given state.
     *
     * @param state The state whose labels to retrieve.
     * @return The set of labels attach to the given state.
     */
    std::set<std::string> getLabelsOfState(storm::storage::sparse::state_type state) const;

    virtual bool isSparseModel() const override;

    virtual bool supportsParameters() const override;

    /*!
     * Checks whether the model has parameters.
     * Performance warning: the worst-case complexity is linear in the number of transitions.
     *
     * @return True iff the model has parameters.
     */
    virtual bool hasParameters() const override;

    virtual bool isExact() const override;

    /*!
     * Does it support uncertainty (e.g., via interval-valued entries).
     * Notice that while parametric Markov models may be seen as uncertain, within storm,these models are not called uncertain.
     *
     * @return
     */
    virtual bool supportsUncertainty() const override;

    /*!
     * Checks whether the model actually is uncertain, i.e., whether there is a non-singleton transition relation.
     * Performance warning: the worst-case complexity is linear in the number of transitions.
     * Notice that while parametric Markov models may be seen as uncertain, within storm,these models are not called uncertain.
     *
     * @return True iff the model can be instantiated in more than one way.
     */
    virtual bool hasUncertainty() const;

    virtual std::size_t hash() const;

   protected:
    RewardModelType& rewardModel(std::string const& rewardModelName);

    /*!
     * Prints the information header (number of states and transitions) of the model to the specified stream.
     *
     * @param out The stream the information is to be printed to.
     */
    void printModelInformationHeaderToStream(std::ostream& out) const;

    /*!
     * Prints the information footer (reward models, labels and size in memory) of the model to the
     * specified stream.
     *
     * @param out The stream the information is to be printed to.
     */
    void printModelInformationFooterToStream(std::ostream& out) const;

    /*!
     * Prints information about the reward models to the specified stream.
     *
     * @param out The stream the information is to be printed to.
     */
    void printRewardModelsInformationToStream(std::ostream& out) const;

    /*!
     * Return a string that is additonally added to the state information in the dot stream.
     * @param state
     * @return
     */
    virtual std::string additionalDotStateInfo(uint64_t state) const;

   private:
    // Upon construction of a model, this function asserts that the specified components are valid
    void assertValidityOfComponents(storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components) const;

    //  A matrix representing transition relation.
    storm::storage::SparseMatrix<ValueType> transitionMatrix;

    // The labeling of the states.
    storm::models::sparse::StateLabeling stateLabeling;

    // The reward models of the model.
    std::unordered_map<std::string, RewardModelType> rewardModels;

    // If set, a vector representing the labels of choices.
    std::optional<storm::models::sparse::ChoiceLabeling> choiceLabeling;

    // if set, retrieves for each state the variable valuation that this state represents
    std::optional<storm::storage::sparse::Valuations> stateValuations;

    // if set, gives information about where each choice originates w.r.t. the input model description
    std::optional<std::shared_ptr<storm::storage::sparse::ChoiceOrigins>> choiceOrigins;
};

/*!
 * Get all probability parameters occurring on transitions.
 * @param model Model.
 * @return Set of parameters.
 */
std::set<storm::RationalFunctionVariable> getProbabilityParameters(Model<storm::RationalFunction> const& model);

/*!
 * Get all parameters occurring in rewards.
 * @param model Model.
 * @return Set of parameters.
 */
std::set<storm::RationalFunctionVariable> getRewardParameters(Model<storm::RationalFunction> const& model);

/*!
 * Get all parameters occurring in rates.
 * @param model Model.
 * @return Set of parameters. If model is not a continuous time model, the returned set is empty.
 */
std::set<storm::RationalFunctionVariable> getRateParameters(Model<storm::RationalFunction> const& model);

/*!
 * Get all parameters (probability, rewards, and rates) occurring in the model.
 * @param model Model.
 * @return Set of parameters.
 */
std::set<storm::RationalFunctionVariable> getAllParameters(Model<storm::RationalFunction> const& model);
}  // namespace sparse
}  // namespace models
}  // namespace storm
