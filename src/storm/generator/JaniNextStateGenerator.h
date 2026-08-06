#pragma once

#include "storm/generator/NextStateGenerator.h"
#include "storm/generator/TransientVariableInformation.h"

#include "storm/storage/BoostTypes.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/OrderedAssignments.h"
#include "storm/storage/jani/eliminator/ArrayEliminator.h"

namespace storm {
namespace jani {
class Edge;
class EdgeDestination;
}  // namespace jani

namespace generator {
template<typename StateType, typename ValueType>
class Distribution;

template<typename ValueType, typename StateType = uint32_t>
class JaniNextStateGenerator : public NextStateGenerator<ValueType, StateType> {
   public:
    static_assert(!storm::IsIntervalType<ValueType>, "JaniNextStateGenerator does not support interval types.");
    typedef typename NextStateGenerator<ValueType, StateType>::StateToIdCallback StateToIdCallback;
    typedef storm::storage::FlatSet<uint_fast64_t> EdgeIndexSet;
    enum class EdgeFilter { All, WithRate, WithoutRate };

    JaniNextStateGenerator(storm::jani::Model const& model, NextStateGeneratorOptions const& options = NextStateGeneratorOptions());

    /*!
     * Returns the jani features with which this builder can deal natively.
     */
    static storm::jani::ModelFeatures getSupportedJaniFeatures();

    /*!
     * A quick check to detect whether the given model is not supported.
     * This method only over-approximates the set of models that can be handled, i.e., if this
     * returns true, the model might still be unsupported.
     */
    static bool canHandle(storm::jani::Model const& model);

    virtual ModelType getModelType() const override;
    virtual bool isDeterministicModel() const override;
    virtual bool isDiscreteTimeModel() const override;
    virtual bool isPartiallyObservable() const override;
    virtual std::vector<StateType> getInitialStates(StateToIdCallback const& stateToIdCallback) override;

    /// Initializes state valuations by adding the appropriate variables.
    virtual storm::storage::sparse::Valuations initializeStateValuations() const override;

    virtual StateBehavior<ValueType, StateType> expand(StateToIdCallback const& stateToIdCallback) override;

    /// Adds the valuation for the currently loaded state to the given builder
    virtual void addStateValuation(storm::storage::sparse::state_type const& currentStateIndex, storm::storage::sparse::Valuations& valuations) const override;

    virtual std::size_t getNumberOfRewardModels() const override;
    virtual storm::builder::RewardModelInformation getRewardModelInformation(uint64_t const& index) const override;

    virtual storm::models::sparse::StateLabeling label(storm::storage::sparse::StateStorage<StateType> const& stateStorage,
                                                       std::vector<StateType> const& initialStateIndices = {},
                                                       std::vector<StateType> const& deadlockStateIndices = {},
                                                       std::vector<StateType> const& unexploredStateIndices = {}) override;

    virtual std::shared_ptr<storm::storage::sparse::ChoiceOrigins> generateChoiceOrigins(std::vector<boost::any>& dataForChoiceOrigins) const override;

    /*!
     * Sets the values of all transient variables in the current state to the given evaluator.
     * @pre The values of non-transient variables have been set in the provided evaluator
     * @param state The current state
     * @param evaluator the evaluator to which the values will be set
     * @post The values of all transient variables are set in the given evaluator (including the transient variables without an explicit assignment in the
     * current locations).
     */
    virtual void unpackTransientVariableValuesIntoEvaluator(CompressedState const& state,
                                                            storm::expressions::ExpressionEvaluator<ValueType>& evaluator) const override;

   private:
    /*!
     * Retrieves the location index from the given state.
     */
    uint64_t getLocation(CompressedState const& state, LocationVariableInformation const& locationVariable) const;

    /*!
     * Sets the location index from the given state.
     */
    void setLocation(CompressedState& state, LocationVariableInformation const& locationVariable, uint64_t locationIndex) const;

    /*!
     * Retrieves the tuple of locations of the given state.
     */
    std::vector<uint64_t> getLocations(CompressedState const& state) const;

    /*!
     * A delegate constructor that is used to preprocess the model before the constructor of the superclass is
     * being called. The last argument is only present to distinguish the signature of this constructor from the
     * public one.
     */
    JaniNextStateGenerator(storm::jani::Model const& model, NextStateGeneratorOptions const& options, bool flag);

    /*!
     * Applies an update to the state currently loaded into the evaluator and applies the resulting values to
     * the given compressed state.
     * @params state The state to which to apply the new values.
     * @params destination The update to apply.
     * @params locationVariable The location variable that is being updated.
     * @params assignmentLevel The assignmentLevel that is to be considered for the update.
     * @return The resulting state.
     */
    void applyUpdate(CompressedState& state, storm::jani::EdgeDestination const& destination,
                     storm::generator::LocationVariableInformation const& locationVariable, int64_t assignmentlevel,
                     storm::expressions::ExpressionEvaluator<ValueType> const& expressionEvaluator);

    /*!
     * Applies an update to the state currently loaded into the evaluator and applies the resulting values to
     * the given compressed state.
     * @params state The state to which to apply the new values.
     * @params destination The update to apply.
     * @params locationVariable The location variable that is being updated.
     * @params assignmentLevel The assignmentLevel that is to be considered for the update.
     * @return The resulting state.
     */
    void applyTransientUpdate(TransientVariableValuation<ValueType>& transientValuation, storm::jani::detail::ConstAssignments const& transientAssignments,
                              storm::expressions::ExpressionEvaluator<ValueType> const& expressionEvaluator) const;

    /**
     * Required method to overload, but currently throws an error as POMDPs are not yet specified in JANI.
     * Furthermore, it might be that these observation labels will not be used and that one uses transient variables instead.
     *
     * @param state
     * @return
     */
    virtual storm::storage::BitVector evaluateObservationLabels(CompressedState const& state) const override;

    /*!
     * Computes the values of the transient variables assigned in the given locations.
     * @note Only the the transient variables with an explicit assignment in the provided locations are contained in the returned struct.
     * @pre The values of non-transient variables have been set in the provided evaluator
     * @return a struct containing the values of the transient variables within the given locations
     */
    TransientVariableValuation<ValueType> getTransientVariableValuationAtLocations(std::vector<uint64_t> const& locations,
                                                                                   storm::expressions::ExpressionEvaluator<ValueType> const& evaluator) const;

    /*!
     * Retrieves all choices possible from the given state.
     *
     * @param locations The current locations of all automata.
     * @param state The state for which to retrieve the silent choices.
     * @param edgeFilter Restricts the kind of edges to be considered.
     * @return The action choices of the state.
     */
    std::vector<Choice<ValueType>> getActionChoices(std::vector<uint64_t> const& locations, CompressedState const& state, StateToIdCallback stateToIdCallback,
                                                    EdgeFilter const& edgeFilter = EdgeFilter::All);

    /*!
     * Retrieves the choice generated by the given edge.
     */
    Choice<ValueType> expandNonSynchronizingEdge(storm::jani::Edge const& edge, uint64_t outputActionIndex, uint64_t automatonIndex,
                                                 CompressedState const& state, StateToIdCallback stateToIdCallback);

    typedef std::vector<std::pair<uint64_t, storm::jani::Edge const*>> EdgeSetWithIndices;
    typedef std::unordered_map<uint64_t, EdgeSetWithIndices> LocationsAndEdges;
    typedef std::vector<std::pair<uint64_t, LocationsAndEdges>> AutomataAndEdges;
    typedef std::pair<boost::optional<uint64_t>, AutomataAndEdges> OutputAndEdges;

    typedef std::pair<uint64_t, EdgeSetWithIndices> AutomatonAndEdgeSet;
    typedef std::vector<AutomatonAndEdgeSet> AutomataEdgeSets;

    void expandSynchronizingEdgeCombination(AutomataEdgeSets const& edgeCombination, uint64_t outputActionIndex, CompressedState const& state,
                                            StateToIdCallback stateToIdCallback, std::vector<Choice<ValueType>>& newChoices);
    void generateSynchronizedDistribution(storm::storage::BitVector const& state, AutomataEdgeSets const& edgeCombination,
                                          std::vector<EdgeSetWithIndices::const_iterator> const& iteratorList,
                                          storm::generator::Distribution<StateType, ValueType>& distribution, std::vector<ValueType>& stateActionRewards,
                                          EdgeIndexSet& edgeIndices, StateToIdCallback stateToIdCallback);

    /*!
     * Checks the list of enabled edges for multiple synchronized writes to the same global variable.
     */
    void checkGlobalVariableWritesValid(AutomataEdgeSets const& enabledEdges) const;

    /*!
     * Evaluates the reward expressions using the current evaluator
     */
    std::vector<ValueType> evaluateRewardExpressions() const;

    /*!
     * Evaluates the reward expressions using the current evaluator, multiplies them by the given factor and adds it to the given vector.
     */
    void addEvaluatedRewardExpressions(std::vector<ValueType>& rewards, ValueType const& factor) const;

    /*!
     * Builds the information structs for the reward models.
     */
    void buildRewardModelInformation();

    /*!
     * Creates the internal information about synchronizing edges.
     */
    void createSynchronizationInformation();

    /*!
     * Checks the underlying model for validity for this next-state generator.
     */
    void checkValid() const;

    /// The model used for the generation of next states.
    storm::jani::Model model;

    /// The automata that are put into parallel by this generator.
    std::vector<std::reference_wrapper<storm::jani::Automaton const>> parallelAutomata;

    /// The vector storing the edges that need to be explored (synchronously or asynchronously).
    std::vector<OutputAndEdges> edges;

    /// The names and defining expressions of reward models that need to be considered.
    std::vector<std::pair<std::string, storm::expressions::Expression>> rewardExpressions;

    /// A vector storing information about the corresponding reward models (variables).
    std::vector<storm::builder::RewardModelInformation> rewardModelInformation;

    /// A flag that stores whether at least one of the selected reward models has state-action rewards.
    bool hasStateActionRewards;

    /// A flag that stores whether we shall evaluate reward expressions at edges
    bool evaluateRewardExpressionsAtEdges;

    /// A flag that stores whether we shall evaluate reward expressions at edge destinations
    bool evaluateRewardExpressionsAtDestinations;

    /// Data from eliminated array expressions. These are required to keep references to array variables in LValues alive.
    storm::jani::ArrayEliminatorData arrayEliminatorData;

    /// Information about the transient variables of the model.
    TransientVariableInformation<ValueType> transientVariableInformation;
};

}  // namespace generator
}  // namespace storm
