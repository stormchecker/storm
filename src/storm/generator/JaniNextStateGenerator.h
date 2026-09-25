#pragma once

#include "storm/generator/Distribution.h"
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

    virtual StateBehavior<ValueType, StateType> const& expand(StateToIdCallback const& stateToIdCallback) override;

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
     * Stores the tuple of locations of the given state in the given vector (whose previous content is discarded).
     */
    void getLocations(CompressedState const& state, std::vector<uint64_t>& result) const;

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
     * Same as above, but stores the result in the given valuation (whose previous content is discarded) to avoid allocations.
     */
    void getTransientVariableValuationAtLocations(std::vector<uint64_t> const& locations, storm::expressions::ExpressionEvaluator<ValueType> const& evaluator,
                                                  TransientVariableValuation<ValueType>& result) const;

    /*!
     * Makes the evaluator hold the (non-transient) variable values of the given state.
     * To avoid rewriting all variables, only the variables that differ from the state that was previously loaded via this method are written.
     * @pre This must only be called during the expansion of a state, i.e., the evaluator holds the values of scratch.evaluatorState.
     *      This is checked in debug mode. In particular, do not modify the (non-transient) variable values of the evaluator by other means while expanding a
     * state.
     */
    void setEvaluatorState(CompressedState const& state);

    /*!
     * Checks whether the (non-transient) variable values in the evaluator coincide with the values in the given state.
     * @note This is expensive and only meant to be used in assertions.
     */
    bool evaluatorHoldsState(CompressedState const& state) const;

    /*!
     * Retrieves all choices possible from the given state.
     *
     * @param locations The current locations of all automata.
     * @param state The state for which to retrieve the silent choices.
     * @param edgeFilter Restricts the kind of edges to be considered.
     * @param behavior The behavior to which the action choices of the state are added.
     */
    void getActionChoices(std::vector<uint64_t> const& locations, CompressedState const& state, StateToIdCallback const& stateToIdCallback,
                          EdgeFilter const& edgeFilter, StateBehavior<ValueType, StateType>& behavior);

    /*!
     * Adds the choice generated by the given edge to the given behavior.
     * @return a reference to the added choice (valid until another choice is added to the behavior)
     */
    Choice<ValueType>& expandNonSynchronizingEdge(storm::jani::Edge const& edge, uint64_t outputActionIndex, uint64_t automatonIndex,
                                                  CompressedState const& state, StateToIdCallback const& stateToIdCallback,
                                                  StateBehavior<ValueType, StateType>& behavior);

    typedef std::vector<std::pair<uint64_t, storm::jani::Edge const*>> EdgeSetWithIndices;
    typedef std::unordered_map<uint64_t, EdgeSetWithIndices> LocationsAndEdges;
    typedef std::vector<std::pair<uint64_t, LocationsAndEdges>> AutomataAndEdges;
    typedef std::pair<boost::optional<uint64_t>, AutomataAndEdges> OutputAndEdges;

    typedef std::pair<uint64_t, EdgeSetWithIndices> AutomatonAndEdgeSet;
    typedef std::vector<AutomatonAndEdgeSet> AutomataEdgeSets;

    void expandSynchronizingEdgeCombination(AutomataEdgeSets const& edgeCombination, uint64_t outputActionIndex, CompressedState const& state,
                                            StateToIdCallback const& stateToIdCallback, StateBehavior<ValueType, StateType>& behavior);
    void generateSynchronizedDistribution(storm::storage::BitVector const& state, AutomataEdgeSets const& edgeCombination,
                                          std::vector<EdgeSetWithIndices::const_iterator> const& iteratorList,
                                          storm::generator::Distribution<StateType, ValueType>& distribution, std::vector<ValueType>& stateActionRewards,
                                          EdgeIndexSet& edgeIndices, StateToIdCallback const& stateToIdCallback);

    /*!
     * Checks the list of enabled edges for multiple synchronized writes to the same global variable.
     */
    void checkGlobalVariableWritesValid(AutomataEdgeSets const& enabledEdges) const;

    /*!
     * Evaluates the reward expressions using the current evaluator
     */
    std::vector<ValueType> evaluateRewardExpressions() const;

    /*!
     * Evaluates the reward expressions using the current evaluator and stores the result in the given vector (whose previous content is discarded).
     */
    void evaluateRewardExpressions(std::vector<ValueType>& result) const;

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

    /*!
     * Scratch memory that is reused across calls in order to avoid (many small) allocations for every explored state.
     * The members are only valid within a single call of the respective functions.
     * @note As a consequence, a JaniNextStateGenerator (in particular its expand method) must not be used concurrently from multiple threads.
     */
    struct ScratchMemory {
        std::vector<uint64_t> locations;
        TransientVariableValuation<ValueType> transientValuation;
        std::vector<AutomataEdgeSets> automataEdgeSets;  // one entry for each element of 'edges'
        std::vector<EdgeSetWithIndices::const_iterator> iteratorList;
        std::vector<EdgeSetWithIndices const*> edgeSets;
        std::vector<EdgeSetWithIndices::const_iterator> firstEnabledEdgeIterators;
        storm::generator::Distribution<StateType, ValueType> distribution;
        std::vector<storm::jani::EdgeDestination const*> destinations;
        std::vector<LocationVariableInformation const*> locationVars;
        CompressedState successorState;
        // The state whose (non-transient) variable values are currently stored in the evaluator while a state is expanded. See setEvaluatorState.
        CompressedState evaluatorState;
    };
    ScratchMemory scratch;

    /// The expressions of the (non-transient) variables (location, boolean, integer variables in that order).
    /// Only used by evaluatorHoldsState (i.e., in assertions). Caching them avoids that the evaluator has to compile the expressions over and over again.
    mutable std::vector<storm::expressions::Expression> variableExpressionsForAssertions;
};

}  // namespace generator
}  // namespace storm
