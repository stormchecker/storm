#pragma once

#include <cstdint>
#include <vector>

#include "storm/expressions/Expression.h"

namespace storm::gbar {
namespace abstraction {

enum class PivotSelectionHeuristic { NearestMaximalDeviation, MostProbablePath, MaxWeightedDeviation };

enum class SplitMode { All, None, NonGuard };

enum class ReuseMode { All, None, Qualitative, Quantitative };

enum class SolveMode { Dd, Sparse };

enum class ValidBlockMode { MorePredicates, BlockEnumeration };

/*!
 * Options that configure the game-based abstraction-refinement procedure. The default values correspond to the
 * defaults of the (formerly used) abstraction settings.
 */
struct AbstractionOptions {
    AbstractionOptions() = default;

    AbstractionOptions(std::vector<storm::expressions::Expression>&& constraints,
                       std::vector<std::vector<storm::expressions::Expression>>&& injectedRefinementPredicates)
        : constraints(std::move(constraints)), injectedRefinementPredicates(std::move(injectedRefinementPredicates)) {
        // Intentionally left empty.
    }

    // Options for the abstractor.
    /// A flag indicating whether the abstractor restricts the abstraction to relevant states.
    bool restrictToRelevantStates = true;

    /// A flag indicating whether the abstractor restricts the initial states to valid blocks.
    bool restrictToValidBlocks = false;

    /// A flag indicating whether decomposition is to be used during the abstraction.
    bool useDecomposition = true;

    // Options for the refiner.
    /// A flag indicating whether interpolation is to be used to eliminate spurious pivot blocks.
    bool useInterpolation = true;

    /// The mode that determines which predicates are split into atoms for the refinement.
    SplitMode splitMode = SplitMode::All;

    /// The heuristic to use for pivot block selection.
    PivotSelectionHeuristic pivotSelectionHeuristic = PivotSelectionHeuristic::NearestMaximalDeviation;

    /// A flag indicating whether refinement predicates are to be ranked.
    bool rankRefinementPredicates = false;

    /// A flag indicating whether to refine eagerly.
    bool useEagerRefinement = false;

    /// A flag indicating whether all guards are added as initial predicates.
    bool addAllGuards = true;

    /// A flag indicating whether all initial expressions are added as initial predicates.
    bool addAllInitialExpressions = true;

    // Options for the model checking procedure.
    /// The precision used for detecting convergence.
    double precision = 1e-03;

    /// A flag indicating whether to use a relative termination criterion.
    bool relativeTerminationCriterion = false;

    /// The mode with which to solve the abstractions.
    SolveMode solveMode = SolveMode::Sparse;

    /// The mode determining which results are reused.
    ReuseMode reuseMode = ReuseMode::All;

    /// The maximal number of abstractions to perform until giving up on converging.
    uint_fast64_t maximalNumberOfAbstractions = 20000;

    /// A flag indicating whether player 1 strategies are to be fixed.
    bool fixPlayer1Strategy = true;

    /// A flag indicating whether player 2 strategies are to be fixed.
    bool fixPlayer2Strategy = true;

    /// A flag indicating whether debug mode is enabled.
    bool debug = false;

    // Predicates.
    /// Additional constraints used by the abstraction.
    std::vector<storm::expressions::Expression> constraints;

    /// Predicates that are injected (starting with the *last* one in this list) instead of the derived ones.
    std::vector<std::vector<storm::expressions::Expression>> injectedRefinementPredicates;
};

}  // namespace abstraction
}  // namespace storm::gbar
