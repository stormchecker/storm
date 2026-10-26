#pragma once

#include "storm-gamebased-ar/abstraction/AbstractionOptions.h"
#include "storm/settings/modules/ModuleSettings.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for the abstraction procedures.
 */
class AbstractionSettings : public ModuleSettings {
   public:
    enum class Method { Games, Bisimulation };

    // The remaining enums are defined by the game-based abstraction library (see AbstractionOptions.h) and are merely
    // aliased here so that the settings module stays consistent with the library's option types.
    using PivotSelectionHeuristic = storm::gbar::abstraction::PivotSelectionHeuristic;

    using SplitMode = storm::gbar::abstraction::SplitMode;

    using ReuseMode = storm::gbar::abstraction::ReuseMode;

    using SolveMode = storm::gbar::abstraction::SolveMode;

    using ValidBlockMode = storm::gbar::abstraction::ValidBlockMode;

    /*!
     * Creates a new set of abstraction settings.
     */
    AbstractionSettings();

    /*!
     * Retrieves the selected abstraction refinement method.
     */
    Method getAbstractionRefinementMethod() const;

    /*!
     * Retrieves whether the option to use the decomposition was set.
     *
     * @return True iff the option was set.
     */
    bool isUseDecompositionSet() const;

    /*!
     * Retrieves the selected split mode.
     *
     * @return The selected split mode.
     */
    SplitMode getSplitMode() const;

    /*!
     * Retrieves whether the option to add all guards was set.
     *
     * @return True iff the option was set.
     */
    bool isAddAllGuardsSet() const;

    /*!
     * Retrieves whether the option to add all initial expressions was set.
     *
     * @return True iff the option was set.
     */
    bool isAddAllInitialExpressionsSet() const;

    /*!
     * Sets the option to add all guards to the specified value.
     *
     * @param value The new value.
     */
    void setAddAllGuards(bool value);

    /*!
     * Sets the option to add all initial expressions to the specified value.
     *
     * @param value The new value.
     */
    void setAddAllInitialExpressions(bool value);

    /*!
     * Retrieves whether the option to use interpolation was set.
     *
     * @return True iff the option was set.
     */
    bool isUseInterpolationSet() const;

    /*!
     * Retrieves the precision that is used for detecting convergence.
     *
     * @return The precision to use for detecting convergence.
     */
    double getPrecision() const;

    /*!
     * Retrieves whether to use a relative termination criterion for detecting convergence.
     */
    bool getRelativeTerminationCriterion() const;

    /*!
     * Retrieves the selected heuristic to select pivot blocks.
     *
     * @return The selected heuristic.
     */
    PivotSelectionHeuristic getPivotSelectionHeuristic() const;

    /*!
     * Retrieves the selected reuse mode.
     *
     * @return The selected reuse mode.
     */
    ReuseMode getReuseMode() const;

    /*!
     * Retrieves whether only relevant states are to be considered.
     *
     * @return True iff the option was set.
     */
    bool isRestrictToRelevantStatesSet() const;

    /*!
     * Retrieves the mode with which to solve the games.
     */
    SolveMode getSolveMode() const;

    /*!
     * Retrieves the maximal number of abstractions to perform until giving up on converging.
     *
     * @return The maximal abstraction count.
     */
    uint_fast64_t getMaximalAbstractionCount() const;

    /*
     * Determines whether refinement predicates are to be ranked.
     *
     * @return True iff the refinement predicates are to be ranked.
     */
    bool isRankRefinementPredicatesSet() const;

    /*!
     * Retrieves whether the constraints option was set.
     *
     * @return True if the constraints option was set.
     */
    bool isConstraintsSet() const;

    /*!
     * Retrieves the string that specifies additional constraints.
     *
     * @return The string that defines the constraints.
     */
    std::string getConstraintString() const;

    /*!
     * Retrieves whether to refine eagerly.
     */
    bool isUseEagerRefinementSet() const;

    /*!
     * Retrieves whether the debug option was set.
     */
    bool isDebugSet() const;

    /*!
     * Retrieves whether the option to inject the refinement predicates is set.
     */
    bool isInjectRefinementPredicatesSet() const;

    /*!
     * Retrieves a string containing refinement predicates to inject (if there are any).
     */
    std::string getInjectedRefinementPredicates() const;

    /*!
     * Retrieves whether player 1 strategies are to be fixed.
     */
    bool isFixPlayer1StrategySet() const;

    /*!
     * Retrieves whether player 2 strategies are to be fixed.
     */
    bool isFixPlayer2StrategySet() const;

    /*!
     * Retrieves the selected mode to guarantee valid blocks.
     */
    ValidBlockMode getValidBlockMode() const;

    const static std::string moduleName;

   private:
    const static std::string methodOptionName;
    const static std::string useDecompositionOptionName;
    const static std::string splitModeOptionName;
    const static std::string addAllGuardsOptionName;
    const static std::string addInitialExpressionsOptionName;
    const static std::string useInterpolationOptionName;
    const static std::string precisionOptionName;
    const static std::string relativeOptionName;
    const static std::string pivotHeuristicOptionName;
    const static std::string reuseResultsOptionName;
    const static std::string restrictToRelevantStatesOptionName;
    const static std::string solveModeOptionName;
    const static std::string maximalAbstractionOptionName;
    const static std::string rankRefinementPredicatesOptionName;
    const static std::string constraintsOptionName;
    const static std::string useEagerRefinementOptionName;
    const static std::string debugOptionName;
    const static std::string injectRefinementPredicatesOptionName;
    const static std::string fixPlayer1StrategyOptionName;
    const static std::string fixPlayer2StrategyOptionName;
    const static std::string validBlockModeOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
