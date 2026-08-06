#pragma once

#include "storm/modelchecker/AbstractModelChecker.h"

#include <boost/algorithm/string.hpp>

#include "storm/storage/prism/Program.h"

#include "storm/storage/dd/DdType.h"
#include "storm/storage/dd/Odd.h"

#include "storm/storage/SymbolicModelDescription.h"

#include "storm-gamebased-ar/abstraction/ExplicitQuantitativeResult.h"
#include "storm-gamebased-ar/abstraction/SymbolicQualitativeGameResult.h"
#include "storm-gamebased-ar/abstraction/SymbolicQualitativeGameResultMinMax.h"

#include "storm/logic/Bound.h"

#include "storm/settings/modules/AbstractionSettings.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/macros.h"

#include "storm/utility/ConstantsComparator.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/graph.h"
#include "storm/utility/solver.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType Type, typename ValueType>
class MenuGame;

template<storm::dd::DdType Type, typename ValueType>
class MenuGameAbstractor;

template<storm::dd::DdType Type, typename ValueType>
class MenuGameRefiner;

template<storm::dd::DdType Type>
class SymbolicQualitativeGameResultMinMax;

template<storm::dd::DdType Type, typename ValueType>
class SymbolicQuantitativeGameResult;

class ExplicitQualitativeGameResult;
class ExplicitQualitativeGameResultMinMax;

template<typename ValueType>
class ExplicitQuantitativeResultMinMax;

class ExplicitGameStrategy;
class ExplicitGameStrategyPair;
}  // namespace abstraction

namespace modelchecker {

using storm::gbar::abstraction::ExplicitQualitativeGameResult;
using storm::gbar::abstraction::ExplicitQualitativeGameResultMinMax;
using storm::gbar::abstraction::ExplicitQuantitativeResult;
using storm::gbar::abstraction::ExplicitQuantitativeResultMinMax;
using storm::gbar::abstraction::SymbolicQualitativeGameResult;
using storm::gbar::abstraction::SymbolicQualitativeGameResultMinMax;

namespace detail {
template<typename ValueType>
struct PreviousExplicitResult {
    ExplicitQuantitativeResult<ValueType> values;
    storm::dd::Odd odd;

    void clear() {
        odd = storm::dd::Odd();
        values = ExplicitQuantitativeResult<ValueType>();
    }
};
}  // namespace detail

struct GameBasedMdpModelCheckerOptions {
    GameBasedMdpModelCheckerOptions() = default;
    GameBasedMdpModelCheckerOptions(std::vector<storm::expressions::Expression> const& constraints,
                                    std::vector<std::vector<storm::expressions::Expression>> const& injectedRefinementPredicates)
        : constraints(constraints), injectedRefinementPredicates(injectedRefinementPredicates) {
        // Intentionally left empty.
    }

    std::vector<storm::expressions::Expression> constraints;
    std::vector<std::vector<storm::expressions::Expression>> injectedRefinementPredicates;
};

template<storm::dd::DdType Type, typename ModelType>
class GameBasedMdpModelChecker : public storm::modelchecker::AbstractModelChecker<ModelType> {
   public:
    typedef typename ModelType::ValueType ValueType;

    /*!
     * Constructs a model checker whose underlying model is implicitly given by the provided program. All
     * verification calls will be answererd with respect to this model.
     *
     * @param model The model description that (symbolically) specifies the model to check.
     * @param options Additional options for the abstraction-refinement process.
     * @param smtSolverFactory A factory used to create SMT solver when necessary.
     */
    explicit GameBasedMdpModelChecker(storm::storage::SymbolicModelDescription const& model,
                                      GameBasedMdpModelCheckerOptions const& options = GameBasedMdpModelCheckerOptions(),
                                      std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory =
                                          std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>());

    /// Overridden methods from super class.
    virtual bool canHandle(storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask) const override;
    virtual std::unique_ptr<storm::modelchecker::CheckResult> computeUntilProbabilities(
        Environment const& env, storm::modelchecker::CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) override;
    virtual std::unique_ptr<storm::modelchecker::CheckResult> computeReachabilityProbabilities(
        Environment const& env, storm::modelchecker::CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) override;

   private:
    /*!
     * Performs the core part of the abstraction-refinement loop.
     */
    std::unique_ptr<storm::modelchecker::CheckResult> performGameBasedAbstractionRefinement(
        Environment const& env, storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask,
        storm::expressions::Expression const& constraintExpression, storm::expressions::Expression const& targetStateExpression);

    std::unique_ptr<storm::modelchecker::CheckResult> performSymbolicAbstractionSolutionStep(
        Environment const& env, storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask,
        storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::OptimizationDirection player1Direction,
        storm::dd::Bdd<Type> const& initialStates, storm::dd::Bdd<Type> const& constraintStates, storm::dd::Bdd<Type> const& targetStates,
        storm::gbar::abstraction::MenuGameRefiner<Type, ValueType> const& refiner,
        boost::optional<SymbolicQualitativeGameResultMinMax<Type>>& previousQualitativeResult,
        boost::optional<abstraction::SymbolicQuantitativeGameResult<Type, ValueType>>& previousMinQuantitativeResult);
    std::unique_ptr<storm::modelchecker::CheckResult> performExplicitAbstractionSolutionStep(
        Environment const& env, storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask,
        storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::OptimizationDirection player1Direction,
        storm::dd::Bdd<Type> const& initialStates, storm::dd::Bdd<Type> const& constraintStates, storm::dd::Bdd<Type> const& targetStates,
        storm::gbar::abstraction::MenuGameRefiner<Type, ValueType> const& refiner, boost::optional<detail::PreviousExplicitResult<ValueType>>& previousResult);

    /*!
     * Retrieves the initial predicates for the abstraction.
     */
    std::vector<storm::expressions::Expression> getInitialPredicates(storm::expressions::Expression const& constraintExpression,
                                                                     storm::expressions::Expression const& targetStateExpression);

    /*!
     * Derives the optimization direction of player 1.
     */
    storm::OptimizationDirection getPlayer1Direction(storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask);

    /*!
     * Performs a qualitative check on the the given game to compute the (player 1) states that have probability
     * 0 or 1, respectively, to reach a target state and only visiting constraint states before.
     */
    SymbolicQualitativeGameResultMinMax<Type> computeProb01States(boost::optional<SymbolicQualitativeGameResultMinMax<Type>> const& previousQualitativeResult,
                                                                  storm::gbar::abstraction::MenuGame<Type, ValueType> const& game,
                                                                  storm::OptimizationDirection player1Direction,
                                                                  storm::dd::Bdd<Type> const& transitionMatrixBdd, storm::dd::Bdd<Type> const& constraintStates,
                                                                  storm::dd::Bdd<Type> const& targetStates);

    ExplicitQualitativeGameResultMinMax computeProb01States(
        boost::optional<detail::PreviousExplicitResult<ValueType>> const& previousResult, storm::dd::Odd const& odd,
        storm::OptimizationDirection player1Direction, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
        std::vector<uint64_t> const& player1RowGrouping, storm::storage::SparseMatrix<ValueType> const& player1BackwardTransitions,
        std::vector<uint64_t> const& player2BackwardTransitions, storm::storage::BitVector const& constraintStates,
        storm::storage::BitVector const& targetStates, storage::ExplicitGameStrategyPair& minStrategyPair, storage::ExplicitGameStrategyPair& maxStrategyPair);

    void printStatistics(storm::gbar::abstraction::MenuGameAbstractor<Type, ValueType> const& abstractor,
                         storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, uint64_t refinements, uint64_t peakPlayer1States,
                         uint64_t peakTransitions) const;

    /*
     * Retrieves the expression characterized by the formula. The formula needs to be propositional.
     */
    storm::expressions::Expression getExpression(storm::logic::Formula const& formula);

    /// The options customizing the abstraction-refinement process.
    GameBasedMdpModelCheckerOptions options;

    /// The preprocessed model that contains only one module/automaton and otherwhise corresponds to the semantics
    /// of the original model description.
    storm::storage::SymbolicModelDescription preprocessedModel;

    /// A factory that is used for creating SMT solvers when needed.
    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory;

    /// A comparator that can be used for detecting convergence.
    storm::utility::ConstantsComparator<ValueType> comparator;

    /// A flag indicating whether to reuse the qualitative results.
    bool reuseQualitativeResults;

    /// A flag indicating whether to reuse the quantitative results.
    bool reuseQuantitativeResults;

    /// The maximal number of abstractions to perform.
    uint64_t maximalNumberOfAbstractions;

    /// The mode selected for solving the abstraction.
    storm::settings::modules::AbstractionSettings::SolveMode solveMode;

    /// The currently used abstractor.
    std::shared_ptr<storm::gbar::abstraction::MenuGameAbstractor<Type, ValueType>> abstractor;

    /// The performed number of refinement iterations.
    uint64_t iteration;

    /// A flag indicating whether to fix player 1 strategies.
    bool fixPlayer1Strategy;

    /// A flag indicating whether to fix player 2 strategies.
    bool fixPlayer2Strategy;

    /// A flag that indicates whether debug mode is enabled.
    bool debug;

    /// Data stored for statistics.
    storm::utility::Stopwatch totalAbstractionWatch;
    storm::utility::Stopwatch totalSolutionWatch;
    storm::utility::Stopwatch totalRefinementWatch;
    storm::utility::Stopwatch totalTranslationWatch;
    storm::utility::Stopwatch totalStrategyProcessingWatch;
    storm::utility::Stopwatch setupWatch;
    storm::utility::Stopwatch totalWatch;
};
}  // namespace modelchecker
}  // namespace storm::gbar
