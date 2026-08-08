#pragma once

#include <set>
#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/expressions/Expression.h"

namespace storm {
namespace expressions {
class ExpressionManager;
}

namespace models {
namespace sparse {
class StateLabeling;
}
}  // namespace models

namespace logic {
class Formula;
}

namespace builder {

class LabelOrExpression {
   public:
    LabelOrExpression(storm::expressions::Expression const& expression);
    LabelOrExpression(std::string const& label);

    bool isLabel() const;
    std::string const& getLabel() const;
    bool isExpression() const;
    storm::expressions::Expression const& getExpression() const;

   private:
    /// An optional label for the expression.
    boost::variant<std::string, storm::expressions::Expression> labelOrExpression;
};

class BuilderOptions {
   public:
    /*!
     * Creates an object representing the default options.
     */
    BuilderOptions(bool buildAllRewardModels = false, bool buildAllLabels = false);

    /*!
     * Creates an object representing the suggested building options assuming that the given formula is the
     * only one to check. Additional formulas may be preserved by calling <code>preserveFormula</code>.
     *
     * @param formula The formula based on which to choose the building options.
     */
    BuilderOptions(storm::logic::Formula const& formula,
                   storm::storage::SymbolicModelDescription const& modelDescription = storm::storage::SymbolicModelDescription());

    /*!
     * Creates an object representing the suggested building options assuming that the given formulas are
     * the only ones to check. Additional formulas may be preserved by calling <code>preserveFormula</code>.
     *
     * @param formula Thes formula based on which to choose the building options.
     */
    BuilderOptions(std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
                   storm::storage::SymbolicModelDescription const& modelDescription = storm::storage::SymbolicModelDescription());

    /*!
     * Changes the options in a way that ensures that the given formula can be checked on the model once it
     * has been built.
     *
     * @param formula The formula that is to be ''preserved''.
     */
    void preserveFormula(storm::logic::Formula const& formula,
                         storm::storage::SymbolicModelDescription const& modelDescription = storm::storage::SymbolicModelDescription());

    /*!
     * Analyzes the given formula and sets an expression for the states states of the model that can be
     * treated as terminal states. Note that this may interfere with checking properties different than the
     * one provided.
     *
     * @param formula The formula used to (possibly) derive an expression for the terminal states of the
     * model.
     */
    void setTerminalStatesFromFormula(storm::logic::Formula const& formula);

    /*!
     * Which reward models are built
     * @return
     */
    std::set<std::string> const& getRewardModelNames() const;

    /*!
     * Which labels are built
     * @return
     */
    std::set<std::string> const& getLabelNames() const;

    /*!
     * Which expression labels are built
     * @return
     */
    std::vector<std::pair<std::string, storm::expressions::Expression>> const& getExpressionLabels() const;
    std::vector<std::pair<LabelOrExpression, bool>> const& getTerminalStates() const;
    bool hasTerminalStates() const;
    void clearTerminalStates();
    bool isApplyMaximalProgressAssumptionSet() const;
    bool isBuildChoiceLabelsSet() const;
    bool isBuildStateValuationsSet() const;
    bool isBuildObservationValuationsSet() const;
    bool isBuildChoiceOriginsSet() const;
    bool isBuildAllRewardModelsSet() const;
    bool isBuildAllLabelsSet() const;
    bool isExplorationChecksSet() const;
    bool isInferObservationsFromActionsSet() const;
    bool isShowProgressSet() const;
    bool isScaleAndLiftTransitionRewardsSet() const;
    bool isAddOutOfBoundsStateSet() const;
    uint64_t getReservedBitsForUnboundedVariables() const;
    bool isAddOverlappingGuardLabelSet() const;
    uint64_t getShowProgressDelay() const;
    /**
     * Some distributions may not sum to one. This tolerance allows handling this case.
     * @return The tolerance
     */
    double getStochasticTolerance() const;

    /**
     * Should all reward models be built? If not set, only required reward models are build.
     * @param newValue The new value (default true)
     * @return this
     */
    BuilderOptions& setBuildAllRewardModels(bool newValue = true);
    /**
     * Add an additional reward model to build
     * @param newValue The name of the extra reward model
     * @return this
     */
    BuilderOptions& addRewardModel(std::string const& rewardModelName);
    /**
     * Should all reward models be built? If not set, only required reward models are build.
     * @param newValue The new value (default true)
     * @return this
     */
    BuilderOptions& setBuildAllLabels(bool newValue = true);
    BuilderOptions& addLabel(storm::expressions::Expression const& expression);
    BuilderOptions& addLabel(std::string const& labelName);
    BuilderOptions& addTerminalExpression(storm::expressions::Expression const& expression, bool value);
    BuilderOptions& addTerminalLabel(std::string const& label, bool value);
    /**
     * Should the maximal progress assumption be applied when building a Markov Automaton?
     * @param newValue If this is true, Markovian edges are not explored from probabilistic states
     * @return this
     */
    BuilderOptions& setApplyMaximalProgressAssumption(bool newValue = true);

    /**
     * Should the choice labels be built?
     * @param newValue The new value (default true)
     * @return this
     */
    BuilderOptions& setBuildChoiceLabels(bool newValue = true);
    /**
     * Should the state valuation mapping be built?
     * @param newValue The new value (default true)
     * @return this
     */
    BuilderOptions& setBuildStateValuations(bool newValue = true);

    /**
     * Should a observation valuation mapping be built?
     * @param newValue The new value (default true)
     * @return this
     */
    BuilderOptions& setBuildObservationValuations(bool newValue = true);

    /**
     * Should the origins the different choices be built?
     * @param newValue The new value (default true)
     * @return this
     */
    BuilderOptions& setBuildChoiceOrigins(bool newValue = true);
    /**
     * Should extra checks be performed during exploration
     * @param newValue The new value (default true)
     * @return this
     */
    BuilderOptions& setExplorationChecks(bool newValue = true);

    /**
     * Should extra checks be performed during exploration
     * @param newValue The new value (default true)
     * @return this
     */
    BuilderOptions& setScaleAndLiftTransitionRewards(bool newValue = true);

    /**
     * Should a state for out of bounds be constructed
     * @param newValue The new value (default true)
     * @return this
     */
    BuilderOptions& setAddOutOfBoundsState(bool newValue = true);

    /**
     * Should a state be labelled for overlapping guards
     * @param newValue the new value (default true)
     */
    BuilderOptions& setAddOverlappingGuardsLabel(bool newValue = true);

    /**
     * Sets the number of bits that will be reserved for unbounded integer variables.
     */
    BuilderOptions& setReservedBitsForUnboundedVariables(uint64_t value);

    /**
     * Sets the tolerance used for checking whether a distribution sums to one.
     * @param newValue The new tolerance
     * @return this
     */
    BuilderOptions& setStochasticTolerance(double newValue);

    /**
     * Sets whether the progress of state space exploration should be printed.
     * @param newValue The new value (default true)
     * @return this
     */
    BuilderOptions& setShowProgress(bool newValue = true);

    /**
     * Sets the delay (in seconds) between progress reports during state space exploration.
     * @param newValue The new delay
     * @return this
     */
    BuilderOptions& setShowProgressDelay(uint64_t newValue);

    /**
     * Substitutes all expressions occurring in these options.
     */
    BuilderOptions& substituteExpressions(std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const& substitutionFunction);

   private:
    /// A flag that indicates whether all reward models are to be built. In this case, the reward model names are
    /// to be ignored.
    bool buildAllRewardModels;

    /// The names of the reward models to generate.
    std::set<std::string> rewardModelNames;

    /// A flag that indicates whether all labels are to be built. In this case, the label names are to be ignored.
    bool buildAllLabels;

    /// A set of labels to build.
    std::set<std::string> labelNames;

    /// The expression that are to be used for creating the state labeling.
    std::vector<std::pair<std::string, storm::expressions::Expression>> expressionLabels;

    /// If one of these labels/expressions evaluates to the given bool, the builder can abort the exploration.
    std::vector<std::pair<LabelOrExpression, bool>> terminalStates;

    /// A flag indicating whether the maximal progress assumption is applied when building a Markov Automaton.
    /// If this is true, Markovian edges are not explored from probabilistic states.
    bool applyMaximalProgressAssumption;

    /// A flag indicating whether or not to build choice labels.
    bool buildChoiceLabels;

    /// A flag indicating whether or not to build for each state the variable valuation from which it originates.
    bool buildStateValuations;

    /// A flag indicating whether or not to build observation valuations
    bool buildObservationValuations;

    // A flag that indicates whether or not to generate the information from which parts of the model specification
    // each choice originates.
    bool buildChoiceOrigins;

    /// A flag that stores whether potentially occurring transition rewards should be scaled and lifted to the edge
    bool scaleAndLiftTransitionRewards;

    /// A flag that stores whether exploration checks are to be performed.
    bool explorationChecks;

    /// For POMDPs, should we allow inference of observation classes from different enabled actions.
    bool inferObservationsFromActions;

    /// A flag for states with overlapping guards
    bool addOverlappingGuardsLabel;

    /// A flag indicating that the an additional state for out of bounds should be created.
    bool addOutOfBoundsState;

    /// Indicates the number of bits that are reserved for the storage of unbounded integer variables.
    uint64_t reservedBitsForUnboundedVariables;

    /// A flag that stores whether the progress of exploration is to be printed.
    bool showProgress;

    /// The delay for printing progress information.
    uint64_t showProgressDelay;

    /// The tolerance that is used for checking whether something is a distribution.
    double stochasticTolerance;
};

}  // namespace builder
}  // namespace storm
