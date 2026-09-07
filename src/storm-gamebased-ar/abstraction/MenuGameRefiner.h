#pragma once

#include <boost/optional.hpp>
#include <functional>
#include <memory>
#include <vector>

#include "storm-gamebased-ar/abstraction/RefinementCommand.h"
#include "storm-gamebased-ar/abstraction/SymbolicQualitativeGameResultMinMax.h"
#include "storm-gamebased-ar/abstraction/SymbolicQuantitativeGameResultMinMax.h"
#include "storm/settings/modules/AbstractionSettings.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/expressions/EquivalenceChecker.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/FullPredicateSplitter.h"
#include "storm/utility/solver.h"

namespace storm {
namespace dd {
class Odd;
}

namespace storage {
class BitVector;
class ExplicitGameStrategyPair;
}  // namespace storage
}  // namespace storm

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType Type, typename ValueType>
class MenuGameAbstractor;

template<storm::dd::DdType Type, typename ValueType>
class MenuGame;

class RefinementPredicates {
   public:
    enum class Source { WeakestPrecondition, InitialGuard, InitialExpression, Guard, Interpolation, Manual };

    RefinementPredicates() = default;
    RefinementPredicates(Source const& source, std::vector<storm::expressions::Expression> const& predicates);

    Source getSource() const;
    std::vector<storm::expressions::Expression> const& getPredicates() const;
    void addPredicates(std::vector<storm::expressions::Expression> const& newPredicates);

   private:
    Source source;
    std::vector<storm::expressions::Expression> predicates;
};

template<storm::dd::DdType Type, typename ValueType>
struct SymbolicMostProbablePathsResult {
    SymbolicMostProbablePathsResult() = default;
    SymbolicMostProbablePathsResult(storm::dd::Add<Type, ValueType> const& maxProbabilities, storm::dd::Bdd<Type> const& spanningTree);

    storm::dd::Add<Type, ValueType> maxProbabilities;
    storm::dd::Bdd<Type> spanningTree;
};

template<storm::dd::DdType Type, typename ValueType>
struct SymbolicPivotStateResult {
    SymbolicPivotStateResult(storm::dd::Bdd<Type> const& pivotState, storm::OptimizationDirection fromDirection,
                             boost::optional<SymbolicMostProbablePathsResult<Type, ValueType>> const& symbolicMostProbablePathsResult = boost::none);

    storm::dd::Bdd<Type> pivotState;
    storm::OptimizationDirection fromDirection;
    boost::optional<SymbolicMostProbablePathsResult<Type, ValueType>> symbolicMostProbablePathsResult;
};

template<typename ValueType>
struct ExplicitPivotStateResult {
    ExplicitPivotStateResult() = default;
    ExplicitPivotStateResult(uint64_t pivotState, ValueType distance, std::vector<std::pair<uint64_t, uint64_t>>&& predecessors)
        : pivotState(pivotState), distance(distance), predecessors(std::move(predecessors)) {
        // Intentionally left empty.
    }

    uint64_t pivotState;

    /// The distance with which the state in question is reached.
    ValueType distance;

    /// The value filled in for states without predecessors in the search.
    static const uint64_t NO_PREDECESSOR;

    /// The predecessors for the states to obtain the given distance.
    std::vector<std::pair<uint64_t, uint64_t>> predecessors;
};

class ExplicitQualitativeGameResultMinMax;

template<typename ValueType>
class ExplicitQuantitativeResultMinMax;

struct MenuGameRefinerOptions {
    MenuGameRefinerOptions() = default;

    MenuGameRefinerOptions(std::vector<std::vector<storm::expressions::Expression>>&& refinementPredicates)
        : refinementPredicates(std::move(refinementPredicates)) {
        // Intentionally left empty.
    }

    std::vector<std::vector<storm::expressions::Expression>> refinementPredicates;
};

template<storm::dd::DdType Type, typename ValueType>
class MenuGameRefiner {
   public:
    /*!
     * Creates a refiner for the provided abstractor.
     */
    MenuGameRefiner(MenuGameAbstractor<Type, ValueType>& abstractor, std::unique_ptr<storm::solver::SmtSolver>&& smtSolver,
                    MenuGameRefinerOptions const& options = MenuGameRefinerOptions());

    /*!
     * Refines the abstractor with the given predicates.
     *
     * @param predicates The predicates to use for refinement.
     * @param allowInjection If true, the refiner is free to inject manually-specified refinement predicates
     * instead of the provided ones.
     */
    void refine(std::vector<storm::expressions::Expression> const& predicates, bool allowInjection = true) const;

    /*!
     * Refines the abstractor based on the qualitative result by trying to derive suitable predicates.
     *
     * @param True if predicates for refinement could be derived, false otherwise.
     */
    bool refine(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::dd::Bdd<Type> const& transitionMatrixBdd,
                SymbolicQualitativeGameResultMinMax<Type> const& qualitativeResult) const;

    /*!
     * Refines the abstractor based on the qualitative result by trying to derive suitable predicates.
     *
     * @param True if predicates for refinement could be derived, false otherwise.
     */
    bool refine(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::dd::Odd const& odd,
                storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<uint64_t> const& player1Grouping,
                std::vector<uint64_t> const& player1Labeling, std::vector<uint64_t> const& player2Labeling, storm::storage::BitVector const& initialStates,
                storm::storage::BitVector const& constraintStates, storm::storage::BitVector const& targetStates,
                ExplicitQualitativeGameResultMinMax const& qualitativeResult, storage::ExplicitGameStrategyPair const& minStrategyPair,
                storage::ExplicitGameStrategyPair const& maxStrategyPair) const;

    /*!
     * Refines the abstractor based on the quantitative result by trying to derive suitable predicates.
     *
     * @param True if predicates for refinement could be derived, false otherwise.
     */
    bool refine(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::dd::Odd const& odd,
                storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<uint64_t> const& player1Grouping,
                std::vector<uint64_t> const& player1Labeling, std::vector<uint64_t> const& player2Labeling, storm::storage::BitVector const& initialStates,
                storm::storage::BitVector const& constraintStates, storm::storage::BitVector const& targetStates,
                ExplicitQuantitativeResultMinMax<ValueType> const& qualitativeResult, storage::ExplicitGameStrategyPair const& minStrategyPair,
                storage::ExplicitGameStrategyPair const& maxStrategyPair) const;

    /*!
     * Refines the abstractor based on the quantitative result by trying to derive suitable predicates.
     *
     * @param True if predicates for refinement could be derived, false otherwise.
     */
    bool refine(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::dd::Bdd<Type> const& transitionMatrixBdd,
                SymbolicQuantitativeGameResultMinMax<Type, ValueType> const& quantitativeResult) const;

    /*!
     * Retrieves whether all guards were added.
     */
    bool addedAllGuards() const;

   private:
    RefinementPredicates derivePredicatesFromDifferingChoices(storm::dd::Bdd<Type> const& player1Choice, storm::dd::Bdd<Type> const& lowerChoice,
                                                              storm::dd::Bdd<Type> const& upperChoice) const;
    RefinementPredicates derivePredicatesFromChoice(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::dd::Bdd<Type> const& pivotState,
                                                    storm::dd::Bdd<Type> const& player1Choice, storm::dd::Bdd<Type> const& choice,
                                                    storm::dd::Bdd<Type> const& choiceSuccessors) const;
    RefinementPredicates derivePredicatesFromPivotState(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::dd::Bdd<Type> const& pivotState,
                                                        storm::dd::Bdd<Type> const& minPlayer1Strategy, storm::dd::Bdd<Type> const& minPlayer2Strategy,
                                                        storm::dd::Bdd<Type> const& maxPlayer1Strategy, storm::dd::Bdd<Type> const& maxPlayer2Strategy) const;

    /*!
     * Preprocesses the predicates.
     */
    std::vector<storm::expressions::Expression> preprocessPredicates(std::vector<storm::expressions::Expression> const& predicates,
                                                                     RefinementPredicates::Source const& source) const;

    /*!
     * Creates a set of refinement commands that amounts to splitting all player 1 choices with the given set of predicates.
     */
    std::vector<RefinementCommand> createGlobalRefinement(std::vector<storm::expressions::Expression> const& predicates) const;

    boost::optional<RefinementPredicates> derivePredicatesFromInterpolationFromTrace(
        storm::expressions::ExpressionManager& interpolationManager, std::vector<std::vector<storm::expressions::Expression>> const& trace,
        std::map<storm::expressions::Variable, storm::expressions::Expression> const& variableSubstitution) const;

    boost::optional<RefinementPredicates> derivePredicatesFromInterpolation(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game,
                                                                            SymbolicPivotStateResult<Type, ValueType> const& symbolicPivotStateResult,
                                                                            storm::dd::Bdd<Type> const& minPlayer1Strategy,
                                                                            storm::dd::Bdd<Type> const& minPlayer2Strategy,
                                                                            storm::dd::Bdd<Type> const& maxPlayer1Strategy,
                                                                            storm::dd::Bdd<Type> const& maxPlayer2Strategy) const;
    std::pair<std::vector<std::vector<storm::expressions::Expression>>, std::map<storm::expressions::Variable, storm::expressions::Expression>> buildTrace(
        storm::expressions::ExpressionManager& expressionManager, storm::gbar::abstraction::MenuGame<Type, ValueType> const& game,
        storm::dd::Bdd<Type> const& spanningTree, storm::dd::Bdd<Type> const& pivotState) const;

    std::pair<std::vector<uint64_t>, std::vector<uint64_t>> buildReversedLabeledPath(ExplicitPivotStateResult<ValueType> const& pivotStateResult) const;
    std::pair<std::vector<std::vector<storm::expressions::Expression>>, std::map<storm::expressions::Variable, storm::expressions::Expression>>
    buildTraceFromReversedLabeledPath(storm::expressions::ExpressionManager& expressionManager, std::vector<uint64_t> const& reversedPath,
                                      std::vector<uint64_t> const& reversedLabels, storm::dd::Odd const& odd,
                                      std::vector<uint64_t> const* stateToOffset = nullptr) const;
    boost::optional<RefinementPredicates> derivePredicatesFromInterpolation(storm::gbar::abstraction::MenuGame<Type, ValueType> const& game,
                                                                            ExplicitPivotStateResult<ValueType> const& pivotStateResult,
                                                                            storm::dd::Odd const& odd) const;

    boost::optional<RefinementPredicates> derivePredicatesFromInterpolationReversedPath(storm::dd::Odd const& odd,
                                                                                        storm::expressions::ExpressionManager& interpolationManager,
                                                                                        std::vector<uint64_t> const& reversedPath,
                                                                                        std::vector<uint64_t> const& stateToOffset,
                                                                                        std::vector<uint64_t> const& player1Labels) const;

    void performRefinement(std::vector<RefinementCommand> const& refinementCommands, bool allowInjection = true) const;

    /// The underlying abstractor to refine.
    std::reference_wrapper<MenuGameAbstractor<Type, ValueType>> abstractor;

    /// A flag indicating whether interpolation shall be used to rule out spurious pivot blocks.
    bool useInterpolation;

    /// A flag indicating whether all predicates shall be split before using them for refinement.
    bool splitAll;

    /// A flag indicating whether predicates derived from weakest preconditions shall be split before using them for refinement.
    bool splitPredicates;

    /// A flag indicating whether predicates are to be ranked.
    bool rankPredicates;

    /// A flag indicating whether predicates are to be generated eagerly.
    bool addPredicatesEagerly;

    /// A flag indicating whether all guards have been used to refine the abstraction.
    bool addedAllGuardsFlag;

    /// A vector of vectors of refinement predicates that are injected (starting with the *last* one in this
    /// list). If empty, the predicates are derived as usual.
    mutable std::vector<std::vector<storm::expressions::Expression>> refinementPredicatesToInject;

    /// The heuristic to use for pivot block selection.
    storm::settings::modules::AbstractionSettings::PivotSelectionHeuristic pivotSelectionHeuristic;

    /// An object that can be used for splitting predicates.
    mutable storm::expressions::FullPredicateSplitter splitter;

    /// An object that can be used to determine whether predicates are equivalent.
    mutable storm::expressions::EquivalenceChecker equivalenceChecker;
};

}  // namespace abstraction
}  // namespace storm::gbar
