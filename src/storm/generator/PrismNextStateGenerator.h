#pragma once

#include "storm/generator/NextStateGenerator.h"

#include "storm/storage/BoostTypes.h"
#include "storm/storage/prism/Program.h"

namespace storm {
namespace generator {
template<typename StateType, typename ValueType>
class Distribution;

template<typename ValueType, typename StateType = uint32_t>
class PrismNextStateGenerator : public NextStateGenerator<ValueType, StateType> {
   public:
    typedef typename NextStateGenerator<ValueType, StateType>::StateToIdCallback StateToIdCallback;
    typedef storm::storage::FlatSet<uint_fast64_t> CommandSet;
    enum class CommandFilter { All, Markovian, Probabilistic };

    PrismNextStateGenerator(storm::prism::Program const& program, NextStateGeneratorOptions const& options = NextStateGeneratorOptions(),
                            std::shared_ptr<ActionMask<ValueType, StateType>> const& = nullptr);

    /*!
     * A quick check to detect whether the given model is not supported.
     * This method only over-approximates the set of models that can be handled, i.e., if this
     * returns true, the model might still be unsupported.
     */
    static bool canHandle(storm::prism::Program const& program);

    virtual ModelType getModelType() const override;
    virtual bool isDeterministicModel() const override;
    virtual bool isDiscreteTimeModel() const override;
    virtual bool isPartiallyObservable() const override;
    virtual std::vector<StateType> getInitialStates(StateToIdCallback const& stateToIdCallback) override;

    virtual StateBehavior<ValueType, StateType> expand(StateToIdCallback const& stateToIdCallback) override;
    bool evaluateBooleanExpressionInCurrentState(storm::expressions::Expression const&) const;

    virtual std::size_t getNumberOfRewardModels() const override;
    virtual storm::builder::RewardModelInformation getRewardModelInformation(uint64_t const& index) const override;
    virtual std::map<std::string, storm::storage::PlayerIndex> getPlayerNameToIndexMap() const override;

    virtual storm::models::sparse::StateLabeling label(storm::storage::sparse::StateStorage<StateType> const& stateStorage,
                                                       std::vector<StateType> const& initialStateIndices = {},
                                                       std::vector<StateType> const& deadlockStateIndices = {},
                                                       std::vector<StateType> const& unexploredStateIndices = {}) override;

    virtual std::shared_ptr<storm::storage::sparse::ChoiceOrigins> generateChoiceOrigins(std::vector<boost::any>& dataForChoiceOrigins) const override;

   private:
    using BaseValueType = typename NextStateGenerator<ValueType, StateType>::BaseValueType;
    void checkValid() const;

    /*!
     * A delegate constructor that is used to preprocess the program before the constructor of the superclass is
     * being called. The last argument is only present to distinguish the signature of this constructor from the
     * public one.
     */
    PrismNextStateGenerator(storm::prism::Program const& program, NextStateGeneratorOptions const& options,
                            std::shared_ptr<ActionMask<ValueType, StateType>> const&, bool flag);

    /*!
     * Applies an update to the state currently loaded into the evaluator and applies the resulting values to
     * the given compressed state.
     * @params state The state to which to apply the new values.
     * @params update The update to apply.
     * @return The resulting state.
     */
    CompressedState applyUpdate(CompressedState const& state, storm::prism::Update const& update);

    /*!
     * Retrieves all commands that are labeled with the given label and enabled in the given state, grouped by
     * modules.
     *
     * This function will iterate over all modules and retrieve all commands that are labeled with the given
     * action and active (i.e. enabled) in the current state. The result is a list of lists of commands in which
     * the inner lists contain all commands of exactly one module. If a module does not have *any* (including
     * disabled) commands, there will not be a list of commands of that module in the result. If, however, the
     * module has a command with a relevant label, but no enabled one, nothing is returned to indicate that there
     * is no legal transition possible.
     *
     * @param The program in which to search for active commands.
     * @param state The current state.
     * @param actionIndex The index of the action label to select.
     * @return A list of lists of active commands or nothing.
     */
    boost::optional<std::vector<std::vector<std::reference_wrapper<storm::prism::Command const>>>> getActiveCommandsByActionIndex(
        uint_fast64_t const& actionIndex, CommandFilter const& commandFilter = CommandFilter::All);

    /*!
     * Retrieves all choices that are definitively asynchronous, possible from the given state.
     *
     * @param state The state for which to retrieve the unlabeled choices.
     * @return The asynchronous choices of the state.
     */
    std::vector<Choice<ValueType>> getAsynchronousChoices(CompressedState const& state, StateToIdCallback stateToIdCallback,
                                                          CommandFilter const& commandFilter = CommandFilter::All);

    /*!
     * Retrieves all (potentially) synchronous choices possible from the given state.
     * Note that these may include choices that run asynchronously for this state.
     *
     * @param choices The new choices are inserted in this vector
     * @param state The state for which to retrieve the unlabeled choices.
     * @return The synchronous choices of the state.
     */
    void addSynchronousChoices(std::vector<Choice<ValueType>>& choices, CompressedState const& state, StateToIdCallback stateToIdCallback,
                               CommandFilter const& commandFilter = CommandFilter::All);

    /*!
     * Generates self-loops for all actions of the given state. Necessary for POMDPs.
     *
     * @param state The state for which to retrieve the unlabeled choices.
     * @return The choices representing self-loops for all actions of the state.
     */
    std::vector<Choice<ValueType>> getSelfLoopsForAllActions(CompressedState const& state, StateToIdCallback stateToIdCallback,
                                                             CommandFilter const& commandFilter = CommandFilter::All);

    /*!
     * Extend the Json struct with additional information about the state.
     */
    virtual void extendStateInformation(storm::json<BaseValueType>& stateInfo) const override;

    /*!
     * Evaluate observation labels
     */
    storm::storage::BitVector evaluateObservationLabels(CompressedState const& state) const override;

    /*!
     * A recursive helper function to generate a synchronziing distribution.
     */
    void generateSynchronizedDistribution(storm::storage::BitVector const& state, ValueType const& probability, uint64_t position,
                                          std::vector<std::vector<std::reference_wrapper<storm::prism::Command const>>::const_iterator> const& iteratorList,
                                          storm::generator::Distribution<StateType, ValueType>& distribution, StateToIdCallback stateToIdCallback);

    bool isCommandPotentiallySynchronizing(prism::Command const& command) const;

    // The program used for the generation of next states.
    storm::prism::Program program;

    // The reward models that need to be considered.
    std::vector<std::reference_wrapper<storm::prism::RewardModel const>> rewardModels;

    // A flag that stores whether at least one of the selected reward models has state-action rewards.
    bool hasStateActionRewards;

    // Mappings from module/action indices to the programs players
    std::vector<storm::storage::PlayerIndex> moduleIndexToPlayerIndexMap;
    std::map<uint_fast64_t, storm::storage::PlayerIndex> actionIndexToPlayerIndexMap;
};

}  // namespace generator
}  // namespace storm
