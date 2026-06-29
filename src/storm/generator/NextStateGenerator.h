#pragma once

#include <vector>

#include "storm/adapters/IntervalForward.h"
#include "storm/builder/BuilderOptions.h"
#include "storm/builder/RewardModelInformation.h"
#include "storm/generator/CompressedState.h"
#include "storm/generator/StateBehavior.h"
#include "storm/generator/VariableInformation.h"
#include "storm/storage/PlayerIndex.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/sparse/ChoiceOrigins.h"
#include "storm/storage/sparse/StateStorage.h"
#include "storm/storage/valuations/Valuations.h"
#include "storm/utility/ConstantsComparator.h"

namespace storm {
namespace expressions {
template<typename V>
class ExpressionEvaluator;
class SimpleValuation;
}  // namespace expressions
namespace generator {
typedef storm::builder::BuilderOptions NextStateGeneratorOptions;

enum class ModelType { DTMC, CTMC, MDP, MA, POMDP, SMG };

template<typename ValueType, typename StateType = uint32_t>
class NextStateGenerator;

/*!
 * Action masks are arguments you can give to the state generator that limit which states are generated.
 *
 */
template<typename ValueType, typename StateType = uint32_t>
class ActionMask {
   public:
    virtual ~ActionMask() = default;
    /**
     * This method is called to check whether an action should be expanded.
     * The current state is obtained from the generator.
     *
     * @param generator the generator that is to be masked
     * @param actionIndex the actionIndex in Prism Programs; i.e. id of actions.
     * @return true if the mask allows building the action/edge/command
     */
    virtual bool query(storm::generator::NextStateGenerator<ValueType, StateType> const& generator, uint64_t actionIndex) = 0;
};

/*!
 * A particular instance of the action mask that uses a callback function
 * to evaluate whether an action should be expanded.
 */
template<typename ValueType, typename StateType = uint32_t>
class StateValuationFunctionMask : public ActionMask<ValueType, StateType> {
   public:
    StateValuationFunctionMask(std::function<bool(storm::expressions::SimpleValuation const&, uint64_t)> const& f);
    virtual ~StateValuationFunctionMask() = default;
    bool query(storm::generator::NextStateGenerator<ValueType, StateType> const& generator, uint64_t actionIndex) override;

   private:
    std::function<bool(storm::expressions::SimpleValuation, uint64_t)> func;
};

template<typename ValueType, typename StateType>
class NextStateGenerator {
   public:
    typedef std::function<StateType(CompressedState const&)> StateToIdCallback;

    NextStateGenerator(storm::expressions::ExpressionManager const& expressionManager, VariableInformation const& variableInformation,
                       NextStateGeneratorOptions const& options, std::shared_ptr<ActionMask<ValueType, StateType>> const& = nullptr);

    /*!
     * Creates a new next state generator. This version of the constructor default-constructs the variable information.
     * Hence, the subclass is responsible for suitably initializing it in its constructor.
     */
    NextStateGenerator(storm::expressions::ExpressionManager const& expressionManager, NextStateGeneratorOptions const& options,
                       std::shared_ptr<ActionMask<ValueType, StateType>> const& = nullptr);

    virtual ~NextStateGenerator();

    uint64_t getStateSize() const;
    virtual ModelType getModelType() const = 0;
    virtual bool isDeterministicModel() const = 0;
    virtual bool isDiscreteTimeModel() const = 0;
    virtual bool isPartiallyObservable() const = 0;
    virtual std::vector<StateType> getInitialStates(StateToIdCallback const& stateToIdCallback) = 0;

    /// Initializes the out-of-bounds state and states with overlapping guards.
    void initializeSpecialStates();

    /// Initializes state valuations by adding the appropriate variables.
    virtual storm::storage::sparse::Valuations initializeStateValuations() const;

    void load(CompressedState const& state);
    virtual StateBehavior<ValueType, StateType> expand(StateToIdCallback const& stateToIdCallback) = 0;
    bool satisfies(storm::expressions::Expression const& expression) const;

    /// Adds the valuation for the currently loaded state to the given builder
    virtual void addStateValuation(storm::storage::sparse::state_type const& currentStateIndex, storm::storage::sparse::Valuations& valuations) const;
    /// Adds the valuation for the currently loaded state
    virtual storm::storage::sparse::Valuations makeObservationValuation() const;

    virtual std::size_t getNumberOfRewardModels() const = 0;
    virtual storm::builder::RewardModelInformation getRewardModelInformation(uint64_t const& index) const = 0;

    std::string stateToString(CompressedState const& state) const;

    storm::json<ValueType> currentStateToJson(bool onlyObservable = false) const;
    storm::expressions::SimpleValuation currentStateToSimpleValuation() const;

    uint32_t observabilityClass(CompressedState const& state) const;

    virtual std::map<std::string, storm::storage::PlayerIndex> getPlayerNameToIndexMap() const;

    virtual storm::models::sparse::StateLabeling label(storm::storage::sparse::StateStorage<StateType> const& stateStorage,
                                                       std::vector<StateType> const& initialStateIndices = {},
                                                       std::vector<StateType> const& deadlockStateIndices = {},
                                                       std::vector<StateType> const& unexploredStateIndices = {}) = 0;

    NextStateGeneratorOptions const& getOptions() const;

    VariableInformation const& getVariableInformation() const;

    virtual std::shared_ptr<storm::storage::sparse::ChoiceOrigins> generateChoiceOrigins(std::vector<boost::any>& dataForChoiceOrigins) const;

    /*!
     * Performs a remapping of all values stored by applying the given remapping.
     *
     * @param remapping The remapping to apply.
     */
    void remapStateIds(std::function<StateType(StateType const&)> const& remapping);

   protected:
    using BaseValueType = storm::IntervalBaseType<ValueType>;

    /*!
     * Checks if the input label has a special purpose (e.g. "init", "deadlock", "unexplored", "overlap_guards", "out_of_bounds").
     */
    bool isSpecialLabel(std::string const& label) const;

    /*!
     * Creates the state labeling for the given states using the provided labels and expressions.
     */
    storm::models::sparse::StateLabeling label(storm::storage::sparse::StateStorage<StateType> const& stateStorage,
                                               std::vector<StateType> const& initialStateIndices, std::vector<StateType> const& deadlockStateIndices,
                                               std::vector<StateType> const& unexploredStateIndices,
                                               std::vector<std::pair<std::string, storm::expressions::Expression>> labelsAndExpressions);

    /*!
     * Sets the values of all transient variables in the current state to the given evaluator.
     * @pre The values of non-transient variables have been set in the provided evaluator
     * @param state The current state
     * @param evaluator the evaluator to which the values will be set
     * @post The values of all transient variables are set in the given evaluator (including the transient variables without an explicit assignment in the
     * current locations).
     */
    virtual void unpackTransientVariableValuesIntoEvaluator(CompressedState const& state,
                                                            storm::expressions::ExpressionEvaluator<BaseValueType>& evaluator) const;

    virtual storm::storage::BitVector evaluateObservationLabels(CompressedState const& state) const = 0;

    virtual void extendStateInformation(storm::json<BaseValueType>& stateInfo) const;

    virtual storm::storage::sparse::Valuations initializeObservationValuations() const;

    void postprocess(StateBehavior<ValueType, StateType>& result);

    /// The options to be used for next-state generation.
    NextStateGeneratorOptions options;

    /// The expression manager used for evaluating expressions.
    std::shared_ptr<storm::expressions::ExpressionManager const> expressionManager;

    /// The expressions that define terminal states.
    std::vector<std::pair<storm::expressions::Expression, bool>> terminalStates;

    /// Information about how the variables are packed.
    VariableInformation variableInformation;

    /// An evaluator used to evaluate expressions.
    std::unique_ptr<storm::expressions::ExpressionEvaluator<BaseValueType>> evaluator;

    /// The currently loaded state.
    CompressedState const* state;

    /// A comparator used to approximately compare constants, e.g., whether they sum to one..
    storm::utility::ConstantsComparator<ValueType> comparator;

    /// The mask to compute the observability class (Constructed upon first use)
    mutable storm::storage::BitVector mask;

    /// The observability classes handed out so far.
    // TODO consider using a BitVectorHashMap for this?
    mutable std::unordered_map<storm::storage::BitVector, uint32_t> observabilityMap;
    /// A state that encodes the outOfBoundsState
    CompressedState outOfBoundsState;

    /// A map that stores the indices of states with overlapping guards.
    boost::optional<std::vector<uint64_t>> overlappingGuardStates;

    std::shared_ptr<ActionMask<ValueType, StateType>> actionMask;
};
}  // namespace generator
}  // namespace storm
