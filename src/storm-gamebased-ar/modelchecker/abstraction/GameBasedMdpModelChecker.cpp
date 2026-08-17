#include "storm-gamebased-ar/modelchecker/abstraction/GameBasedMdpModelChecker.h"

#include <sstream>

#include "storm-gamebased-ar/abstraction/ExplicitQualitativeGameResultMinMax.h"
#include "storm-gamebased-ar/abstraction/ExplicitQuantitativeResultMinMax.h"
#include "storm-gamebased-ar/abstraction/MenuGameRefiner.h"
#include "storm-gamebased-ar/abstraction/jani/JaniMenuGameAbstractor.h"
#include "storm-gamebased-ar/abstraction/prism/PrismMenuGameAbstractor.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/InvalidModelException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/io/file.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/prctl/helper/SparseDtmcPrctlHelper.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/solver/StandardGameSolver.h"
#include "storm/solver/SymbolicGameSolver.h"
#include "storm/storage/ExplicitGameStrategyPair.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/VariableSetPredicateSplitter.h"
#include "storm/storage/jani/Automaton.h"
#include "storm/storage/jani/AutomatonComposition.h"
#include "storm/storage/jani/Edge.h"
#include "storm/storage/jani/EdgeDestination.h"
#include "storm/storage/jani/Location.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/ParallelComposition.h"
#include "storm/storage/jani/visitor/CompositionInformationVisitor.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm::gbar {
namespace modelchecker {

using detail::PreviousExplicitResult;
using storm::gbar::abstraction::ExplicitQuantitativeResult;
using storm::gbar::abstraction::ExplicitQuantitativeResultMinMax;
using storm::gbar::abstraction::SymbolicQuantitativeGameResult;
using storm::gbar::abstraction::SymbolicQuantitativeGameResultMinMax;

template<storm::dd::DdType Type, typename ModelType>
GameBasedMdpModelChecker<Type, ModelType>::GameBasedMdpModelChecker(storm::storage::SymbolicModelDescription const& model,
                                                                    GameBasedMdpModelCheckerOptions const& options,
                                                                    std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory)
    : options(options),
      smtSolverFactory(smtSolverFactory),
      comparator(storm::utility::convertNumber<ValueType>(storm::settings::getModule<storm::settings::modules::AbstractionSettings>().getPrecision()),
                 storm::settings::getModule<storm::settings::modules::AbstractionSettings>().getRelativeTerminationCriterion()),
      reuseQualitativeResults(false),
      reuseQuantitativeResults(false),
      solveMode(storm::settings::getModule<storm::settings::modules::AbstractionSettings>().getSolveMode()),
      debug(storm::settings::getModule<storm::settings::modules::AbstractionSettings>().isDebugSet()) {
    if (model.hasUndefinedConstants()) {
        auto undefinedConstants = model.getUndefinedConstants();
        std::vector<std::string> undefinedConstantNames;
        for (auto undefinedConstant : undefinedConstants) {
            undefinedConstantNames.emplace_back(undefinedConstant.getName());
        }

        STORM_LOG_WARN_COND(!model.hasUndefinedConstants(),
                            "Model contains undefined constants ("
                                << boost::algorithm::join(undefinedConstantNames, ",")
                                << "). Game-based abstraction can treat such models, but you should make sure that you did not simply forget to define these "
                                   "constants. In particular, it may be necessary to constrain the values of the undefined constants.");
    }

    if (model.isPrismProgram()) {
        storm::prism::Program const& originalProgram = model.asPrismProgram();
        STORM_LOG_THROW(
            originalProgram.getModelType() == storm::prism::Program::ModelType::DTMC || originalProgram.getModelType() == storm::prism::Program::ModelType::MDP,
            storm::exceptions::NotSupportedException, "Currently only DTMCs/MDPs are supported by the game-based model checker.");

        auto flattenStart = std::chrono::high_resolution_clock::now();
        // Flatten the modules if there is more than one.
        if (originalProgram.getNumberOfModules() > 1) {
            preprocessedModel = originalProgram.substituteFormulas().flattenModules(this->smtSolverFactory);
        } else {
            preprocessedModel = originalProgram;
        }
        auto flattenEnd = std::chrono::high_resolution_clock::now();
        STORM_LOG_INFO("Flattened model in " << std::chrono::duration_cast<std::chrono::milliseconds>(flattenEnd - flattenStart).count() << "ms.");

        STORM_LOG_TRACE("Game-based model checker got program " << preprocessedModel.asPrismProgram());
    } else {
        storm::jani::Model const& originalModel = model.asJaniModel();
        STORM_LOG_THROW(originalModel.getModelType() == storm::jani::ModelType::DTMC || originalModel.getModelType() == storm::jani::ModelType::MDP,
                        storm::exceptions::NotSupportedException, "Currently only DTMCs/MDPs are supported by the game-based model checker.");

        // Flatten the parallel composition.
        preprocessedModel = model.asJaniModel().flattenComposition();
    }

    auto const& abstractionSettings = storm::settings::getModule<storm::settings::modules::AbstractionSettings>();
    storm::settings::modules::AbstractionSettings::ReuseMode reuseMode = abstractionSettings.getReuseMode();
    reuseQualitativeResults = reuseMode == storm::settings::modules::AbstractionSettings::ReuseMode::All ||
                              reuseMode == storm::settings::modules::AbstractionSettings::ReuseMode::Qualitative;
    reuseQuantitativeResults = reuseMode == storm::settings::modules::AbstractionSettings::ReuseMode::All ||
                               reuseMode == storm::settings::modules::AbstractionSettings::ReuseMode::Quantitative;
    maximalNumberOfAbstractions = abstractionSettings.getMaximalAbstractionCount();
    fixPlayer1Strategy = abstractionSettings.isFixPlayer1StrategySet();
    fixPlayer2Strategy = abstractionSettings.isFixPlayer2StrategySet();
}

template<storm::dd::DdType Type, typename ModelType>
bool GameBasedMdpModelChecker<Type, ModelType>::canHandle(storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask) const {
    storm::logic::Formula const& formula = checkTask.getFormula();
    storm::logic::FragmentSpecification fragment = storm::logic::reachability();
    return formula.isInFragment(fragment) && checkTask.isOnlyInitialStatesRelevantSet();
}

template<storm::dd::DdType Type, typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> GameBasedMdpModelChecker<Type, ModelType>::computeUntilProbabilities(
    Environment const& env, storm::modelchecker::CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) {
    storm::logic::UntilFormula const& pathFormula = checkTask.getFormula();
    std::map<std::string, storm::expressions::Expression> labelToExpressionMapping;
    if (preprocessedModel.isPrismProgram()) {
        labelToExpressionMapping = preprocessedModel.asPrismProgram().getLabelToExpressionMapping();
    } else {
        storm::jani::Model const& janiModel = preprocessedModel.asJaniModel();
        for (auto const& variable : janiModel.getGlobalVariables().getBooleanVariables()) {
            if (variable.isTransient()) {
                labelToExpressionMapping[variable.getName()] = janiModel.getLabelExpression(variable);
            }
        }
    }

    storm::expressions::Expression constraintExpression =
        pathFormula.getLeftSubformula().toExpression(preprocessedModel.getManager(), labelToExpressionMapping);
    storm::expressions::Expression targetStateExpression =
        pathFormula.getRightSubformula().toExpression(preprocessedModel.getManager(), labelToExpressionMapping);

    return performGameBasedAbstractionRefinement(env, checkTask.template substituteFormula<storm::logic::Formula>(pathFormula), constraintExpression,
                                                 targetStateExpression);
}

template<storm::dd::DdType Type, typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> GameBasedMdpModelChecker<Type, ModelType>::computeReachabilityProbabilities(
    Environment const& env, storm::modelchecker::CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    storm::logic::EventuallyFormula const& pathFormula = checkTask.getFormula();
    std::map<std::string, storm::expressions::Expression> labelToExpressionMapping;
    if (preprocessedModel.isPrismProgram()) {
        labelToExpressionMapping = preprocessedModel.asPrismProgram().getLabelToExpressionMapping();
    } else {
        storm::jani::Model const& janiModel = preprocessedModel.asJaniModel();
        for (auto const& variable : janiModel.getGlobalVariables().getBooleanVariables()) {
            if (variable.isTransient()) {
                labelToExpressionMapping[variable.getName()] = janiModel.getLabelExpression(variable);
            }
        }
    }

    storm::expressions::Expression constraintExpression = preprocessedModel.getManager().boolean(true);
    storm::expressions::Expression targetStateExpression = pathFormula.getSubformula().toExpression(preprocessedModel.getManager(), labelToExpressionMapping);

    return performGameBasedAbstractionRefinement(env, checkTask.template substituteFormula<storm::logic::Formula>(pathFormula), constraintExpression,
                                                 targetStateExpression);
}

template<storm::dd::DdType Type, typename ValueType>
std::unique_ptr<storm::modelchecker::CheckResult> checkForResultAfterQualitativeCheck(
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask, storm::OptimizationDirection player2Direction,
    storm::dd::Bdd<Type> const& initialStates, storm::dd::Bdd<Type> const& prob0, storm::dd::Bdd<Type> const& prob1) {
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    if (checkTask.isBoundSet()) {
        // Despite having a bound, we create a quantitative result so that the next layer can perform the comparison.

        if (player2Direction == storm::OptimizationDirection::Minimize) {
            if (storm::logic::isLowerBound(checkTask.getBoundComparisonType())) {
                if ((prob1 && initialStates) == initialStates) {
                    result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0),
                                                                                                               storm::utility::one<ValueType>());
                }
            } else {
                if (!(prob1 && initialStates).isZero()) {
                    result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0),
                                                                                                               storm::utility::one<ValueType>());
                }
            }
        } else if (player2Direction == storm::OptimizationDirection::Maximize) {
            if (!storm::logic::isLowerBound(checkTask.getBoundComparisonType())) {
                if ((prob0 && initialStates) == initialStates) {
                    result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0),
                                                                                                               storm::utility::zero<ValueType>());
                }
            } else {
                if (!(prob0 && initialStates).isZero()) {
                    result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0),
                                                                                                               storm::utility::zero<ValueType>());
                }
            }
        }
    } else {
        if (player2Direction == storm::OptimizationDirection::Minimize && (prob1 && initialStates) == initialStates) {
            result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0),
                                                                                                       storm::utility::one<ValueType>());
        } else if (player2Direction == storm::OptimizationDirection::Maximize && (prob0 && initialStates) == initialStates) {
            result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0),
                                                                                                       storm::utility::zero<ValueType>());
        }
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
std::unique_ptr<storm::modelchecker::CheckResult> checkForResultAfterQualitativeCheck(
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask, storm::dd::Bdd<Type> const& initialStates,
    SymbolicQualitativeGameResultMinMax<Type> const& qualitativeResult) {
    // Check whether we can already give the answer based on the current information.
    std::unique_ptr<storm::modelchecker::CheckResult> result =
        checkForResultAfterQualitativeCheck<Type, ValueType>(checkTask, storm::OptimizationDirection::Minimize, initialStates,
                                                             qualitativeResult.prob0Min.getPlayer1States(), qualitativeResult.prob1Min.getPlayer1States());
    if (result) {
        return result;
    }
    result = checkForResultAfterQualitativeCheck<Type, ValueType>(checkTask, storm::OptimizationDirection::Maximize, initialStates,
                                                                  qualitativeResult.prob0Max.getPlayer1States(), qualitativeResult.prob1Max.getPlayer1States());
    if (result) {
        return result;
    }
    return result;
}

template<typename ValueType>
std::unique_ptr<storm::modelchecker::CheckResult> checkForResultAfterQualitativeCheck(
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask, storm::OptimizationDirection player2Direction,
    storm::storage::BitVector const& initialStates, storm::storage::BitVector const& prob0, storm::storage::BitVector const& prob1) {
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    if (checkTask.isBoundSet()) {
        // Despite having a bound, we create a quantitative result so that the next layer can perform the comparison.

        if (player2Direction == storm::OptimizationDirection::Minimize) {
            if (storm::logic::isLowerBound(checkTask.getBoundComparisonType())) {
                if (initialStates.isSubsetOf(prob1)) {
                    result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0),
                                                                                                               storm::utility::one<ValueType>());
                }
            } else {
                if (!initialStates.isDisjointFrom(prob1)) {
                    result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0),
                                                                                                               storm::utility::one<ValueType>());
                }
            }
        } else if (player2Direction == storm::OptimizationDirection::Maximize) {
            if (!storm::logic::isLowerBound(checkTask.getBoundComparisonType())) {
                if (initialStates.isSubsetOf(prob0)) {
                    result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0),
                                                                                                               storm::utility::zero<ValueType>());
                }
            } else {
                if (!initialStates.isDisjointFrom(prob0)) {
                    result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0),
                                                                                                               storm::utility::zero<ValueType>());
                }
            }
        }
    } else {
        if (player2Direction == storm::OptimizationDirection::Minimize && initialStates.isSubsetOf(prob1)) {
            result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0),
                                                                                                       storm::utility::one<ValueType>());
        } else if (player2Direction == storm::OptimizationDirection::Maximize && initialStates.isSubsetOf(prob0)) {
            result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0),
                                                                                                       storm::utility::zero<ValueType>());
        }
    }

    return result;
}

template<typename ValueType>
std::unique_ptr<storm::modelchecker::CheckResult> checkForResultAfterQualitativeCheck(
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask, storm::storage::BitVector const& initialStates,
    ExplicitQualitativeGameResultMinMax const& qualitativeResult) {
    // Check whether we can already give the answer based on the current information.
    std::unique_ptr<storm::modelchecker::CheckResult> result =
        checkForResultAfterQualitativeCheck<ValueType>(checkTask, storm::OptimizationDirection::Minimize, initialStates,
                                                       qualitativeResult.prob0Min.getPlayer1States(), qualitativeResult.prob1Min.getPlayer1States());
    if (result) {
        return result;
    }
    result = checkForResultAfterQualitativeCheck<ValueType>(checkTask, storm::OptimizationDirection::Maximize, initialStates,
                                                            qualitativeResult.prob0Max.getPlayer1States(), qualitativeResult.prob1Max.getPlayer1States());
    if (result) {
        return result;
    }
    return result;
}

template<typename ValueType>
std::unique_ptr<storm::modelchecker::CheckResult> checkForResultAfterQuantitativeCheck(
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask, storm::OptimizationDirection const& player2Direction,
    std::pair<ValueType, ValueType> const& initialValueRange) {
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    // If the minimum value exceeds an upper threshold or the maximum value is below a lower threshold, we can
    // return the value because the property will definitely hold. Vice versa, if the minimum value exceeds an
    // upper bound or the maximum value is below a lower bound, the property will definitely not hold and we can
    // return the value.
    if (!checkTask.isBoundSet()) {
        return result;
    }

    ValueType const& lowerValue = initialValueRange.first;
    ValueType const& upperValue = initialValueRange.second;

    storm::logic::ComparisonType comparisonType = checkTask.getBoundComparisonType();
    ValueType threshold = checkTask.getBoundThreshold();

    if (storm::logic::isLowerBound(comparisonType)) {
        if (player2Direction == storm::OptimizationDirection::Minimize) {
            if ((storm::logic::isStrict(comparisonType) && lowerValue > threshold) || (!storm::logic::isStrict(comparisonType) && lowerValue >= threshold)) {
                result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0), lowerValue);
            }
        } else {
            if ((storm::logic::isStrict(comparisonType) && upperValue <= threshold) || (!storm::logic::isStrict(comparisonType) && upperValue < threshold)) {
                result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0), upperValue);
            }
        }
    } else {
        if (player2Direction == storm::OptimizationDirection::Maximize) {
            if ((storm::logic::isStrict(comparisonType) && upperValue < threshold) || (!storm::logic::isStrict(comparisonType) && upperValue <= threshold)) {
                result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0), upperValue);
            }
        } else {
            if ((storm::logic::isStrict(comparisonType) && lowerValue >= threshold) || (!storm::logic::isStrict(comparisonType) && lowerValue > threshold)) {
                result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0), lowerValue);
            }
        }
    }

    return result;
}

template<typename ValueType>
std::unique_ptr<storm::modelchecker::CheckResult> checkForResultAfterQuantitativeCheck(ValueType const& minValue, ValueType const& maxValue,
                                                                                       storm::utility::ConstantsComparator<ValueType> const& comparator) {
    std::unique_ptr<storm::modelchecker::CheckResult> result;

    // If the lower and upper bounds are close enough, we can return the result.
    if (comparator.isEqual(minValue, maxValue)) {
        result = std::make_unique<storm::modelchecker::ExplicitQuantitativeCheckResult<ValueType>>(storm::storage::sparse::state_type(0),
                                                                                                   (minValue + maxValue) / ValueType(2));
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
SymbolicQuantitativeGameResult<Type, ValueType> solveMaybeStates(
    Environment const& env, storm::OptimizationDirection const& player1Direction, storm::OptimizationDirection const& player2Direction,
    storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::dd::Bdd<Type> const& maybeStates, storm::dd::Bdd<Type> const& prob1States,
    boost::optional<SymbolicQuantitativeGameResult<Type, ValueType>> const& startInfo = boost::none) {
    STORM_LOG_TRACE("Performing quantative solution step. Player 1: " << player1Direction << ", player 2: " << player2Direction << ".");

    // Compute the ingredients of the equation system.
    storm::dd::Add<Type, ValueType> maybeStatesAdd = maybeStates.template toAdd<ValueType>();
    storm::dd::Add<Type, ValueType> submatrix = maybeStatesAdd * game.getTransitionMatrix();
    storm::dd::Add<Type, ValueType> prob1StatesAsColumn = prob1States.template toAdd<ValueType>().swapVariables(game.getRowColumnMetaVariablePairs());
    storm::dd::Add<Type, ValueType> subvector = submatrix * prob1StatesAsColumn;
    subvector = subvector.sumAbstract(game.getColumnVariables());

    // Cut away all columns targeting non-maybe states.
    submatrix *= maybeStatesAdd.swapVariables(game.getRowColumnMetaVariablePairs());

    // Cut the starting vector to the maybe states of this query.
    storm::dd::Add<Type, ValueType> startVector;
    if (startInfo) {
        startVector = startInfo.get().values * maybeStatesAdd;
    } else {
        startVector = game.getManager().template getAddZero<ValueType>();
    }

    // Create the solver and solve the equation system.
    storm::solver::SymbolicGameSolverFactory<Type, ValueType> solverFactory;
    std::unique_ptr<storm::solver::SymbolicGameSolver<Type, ValueType>> solver =
        solverFactory.create(submatrix, maybeStates, game.getIllegalPlayer1Mask(), game.getIllegalPlayer2Mask(), game.getRowVariables(),
                             game.getColumnVariables(), game.getRowColumnMetaVariablePairs(), game.getPlayer1Variables(), game.getPlayer2Variables());
    solver->setGeneratePlayersStrategies(true);
    auto values = solver->solveGame(env, player1Direction, player2Direction, startVector, subvector,
                                    startInfo ? boost::make_optional(startInfo.get().getPlayer1Strategy()) : boost::none,
                                    startInfo ? boost::make_optional(startInfo.get().getPlayer2Strategy()) : boost::none);
    return SymbolicQuantitativeGameResult<Type, ValueType>(std::make_pair(storm::utility::zero<ValueType>(), storm::utility::one<ValueType>()), values,
                                                           solver->getPlayer1Strategy(), solver->getPlayer2Strategy());
}

template<storm::dd::DdType Type, typename ValueType>
SymbolicQuantitativeGameResult<Type, ValueType> computeQuantitativeResult(
    Environment const& env, storm::OptimizationDirection player1Direction, storm::OptimizationDirection player2Direction,
    storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, SymbolicQualitativeGameResultMinMax<Type> const& qualitativeResult,
    storm::dd::Add<Type, ValueType> const& initialStatesAdd, storm::dd::Bdd<Type> const& maybeStates,
    boost::optional<SymbolicQuantitativeGameResult<Type, ValueType>> const& startInfo = boost::none) {
    bool min = player2Direction == storm::OptimizationDirection::Minimize;
    SymbolicQuantitativeGameResult<Type, ValueType> result;

    // We fix the strategies. That is, we take the decisions of the strategies obtained in the qualitiative
    // preprocessing if possible.
    storm::dd::Bdd<Type> combinedPlayer1QualitativeStrategies;
    storm::dd::Bdd<Type> combinedPlayer2QualitativeStrategies;
    if (min) {
        combinedPlayer1QualitativeStrategies = (qualitativeResult.prob0Min.getPlayer1Strategy() || qualitativeResult.prob1Min.getPlayer1Strategy());
        combinedPlayer2QualitativeStrategies = (qualitativeResult.prob0Min.getPlayer2Strategy() || qualitativeResult.prob1Min.getPlayer2Strategy());
    } else {
        combinedPlayer1QualitativeStrategies = (qualitativeResult.prob0Max.getPlayer1Strategy() || qualitativeResult.prob1Max.getPlayer1Strategy());
        combinedPlayer2QualitativeStrategies = (qualitativeResult.prob0Max.getPlayer2Strategy() || qualitativeResult.prob1Max.getPlayer2Strategy());
    }

    result.player1Strategy = combinedPlayer1QualitativeStrategies;
    result.player2Strategy = combinedPlayer2QualitativeStrategies;
    result.values = game.getManager().template getAddZero<ValueType>();

    auto start = std::chrono::high_resolution_clock::now();
    if (!maybeStates.isZero()) {
        STORM_LOG_TRACE("Solving " << maybeStates.getNonZeroCount() << " maybe states.");

        // Solve the quantitative values of maybe states.
        result = solveMaybeStates(env, player1Direction, player2Direction, game, maybeStates,
                                  min ? qualitativeResult.prob1Min.getPlayer1States() : qualitativeResult.prob1Max.getPlayer1States(), startInfo);

        // Cut the obtained strategies to the reachable states of the game.
        result.getPlayer1Strategy() &= game.getReachableStates();
        result.getPlayer2Strategy() &= game.getReachableStates();

        // Extend the values of the maybe states by the qualitative values.
        result.values += min ? qualitativeResult.prob1Min.getPlayer1States().template toAdd<ValueType>()
                             : qualitativeResult.prob1Max.getPlayer1States().template toAdd<ValueType>();
    } else {
        STORM_LOG_TRACE("No " << (player2Direction == storm::OptimizationDirection::Minimize ? "min" : "max") << " maybe states.");

        // Extend the values of the maybe states by the qualitative values.
        result.values += min ? qualitativeResult.prob1Min.getPlayer1States().template toAdd<ValueType>()
                             : qualitativeResult.prob1Max.getPlayer1States().template toAdd<ValueType>();
    }

    // Construct an ADD holding the initial values of initial states and extract the bound on the initial states.
    storm::dd::Add<Type, ValueType> initialStateValueAdd = initialStatesAdd * result.values;

    ValueType maxValueOverInitialStates = initialStateValueAdd.getMax();
    initialStateValueAdd += (!game.getInitialStates()).template toAdd<ValueType>();
    ValueType minValueOverInitialStates = initialStateValueAdd.getMin();

    result.initialStatesRange = std::make_pair(minValueOverInitialStates, maxValueOverInitialStates);

    result.player1Strategy =
        combinedPlayer1QualitativeStrategies.existsAbstract(game.getPlayer1Variables()).ite(combinedPlayer1QualitativeStrategies, result.getPlayer1Strategy());
    result.player2Strategy =
        combinedPlayer2QualitativeStrategies.existsAbstract(game.getPlayer2Variables()).ite(combinedPlayer2QualitativeStrategies, result.getPlayer2Strategy());

    auto end = std::chrono::high_resolution_clock::now();
    STORM_LOG_TRACE("Obtained quantitative " << (min ? "lower" : "upper") << " bound "
                                             << (min ? result.getInitialStatesRange().first : result.getInitialStatesRange().second) << " in "
                                             << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

    return result;
}

template<typename ValueType>
ExplicitQuantitativeResult<ValueType> computeQuantitativeResult(
    Environment const& env, storm::OptimizationDirection player1Direction, storm::OptimizationDirection player2Direction,
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<uint64_t> const& player1Groups,
    ExplicitQualitativeGameResultMinMax const& qualitativeResult, storm::storage::BitVector const& maybeStates, storage::ExplicitGameStrategyPair& strategyPair,
    storm::dd::Odd const& odd, ExplicitQuantitativeResult<ValueType> const* startingQuantitativeResult = nullptr,
    storage::ExplicitGameStrategyPair const* startingStrategyPair = nullptr,
    boost::optional<PreviousExplicitResult<ValueType>> const& previousResult = boost::none) {
    bool player2Min = player2Direction == storm::OptimizationDirection::Minimize;
    auto const& player1Prob1States = player2Min ? qualitativeResult.getProb1Min().asExplicitQualitativeGameResult().getPlayer1States()
                                                : qualitativeResult.getProb1Max().asExplicitQualitativeGameResult().getPlayer1States();
    auto const& player2Prob0States = player2Min ? qualitativeResult.getProb0Min().asExplicitQualitativeGameResult().getPlayer2States()
                                                : qualitativeResult.getProb0Max().asExplicitQualitativeGameResult().getPlayer2States();
    auto const& player2Prob1States = player2Min ? qualitativeResult.getProb1Min().asExplicitQualitativeGameResult().getPlayer2States()
                                                : qualitativeResult.getProb1Max().asExplicitQualitativeGameResult().getPlayer2States();

    ExplicitQuantitativeResult<ValueType> result(maybeStates.size());
    storm::utility::vector::setVectorValues(result.getValues(), player1Prob1States, storm::utility::one<ValueType>());

    // If there are no maybe states, there is nothing we need to solve.
    if (maybeStates.empty()) {
        return result;
    }

    // If there is a previous result, unpack the previous values with respect to the new ODD.
    if (previousResult) {
        STORM_LOG_ASSERT(player2Min, "Can only reuse previous values when minimizing.");
        previousResult.get().odd.oldToNewIndex(odd, [&previousResult, &result, player2Min, player1Prob1States](uint64_t oldOffset, uint64_t newOffset) {
            if (!player1Prob1States.get(newOffset)) {
                result.getValues()[newOffset] =
                    player2Min ? previousResult.get().values.getValues()[oldOffset] : previousResult.get().values.getValues()[oldOffset];
            }
        });
    }

    // Otherwise, we need to solve a (sub)game.
    STORM_LOG_TRACE("[" << player1Direction << ", " << player2Direction << "]: Solving " << maybeStates.getNumberOfSetBits() << " maybe states.");

    // Create the game by selecting all maybe player 2 states (non-prob0/1) of all maybe player 1 states.
    std::vector<uint64_t> subPlayer1Groups(maybeStates.getNumberOfSetBits() + 1);
    uint64_t position = 0;
    uint64_t previousPlayer2States = 0;
    storm::storage::BitVector player2MaybeStates(transitionMatrix.getRowGroupCount());
    for (uint64_t state : maybeStates) {
        subPlayer1Groups[position] = previousPlayer2States;

        [[maybe_unused]] bool hasMaybePlayer2Successor = false;
        for (uint64_t player2State = player1Groups[state]; player2State < player1Groups[state + 1]; ++player2State) {
            if (!player2Prob0States.get(player2State) && !player2Prob1States.get(player2State)) {
                player2MaybeStates.set(player2State);
                hasMaybePlayer2Successor = true;
                ++previousPlayer2States;
            }
        }
        STORM_LOG_ASSERT(hasMaybePlayer2Successor, "Player 1 maybe state has no player2 maybe successor.");
        ++position;
    }
    subPlayer1Groups.back() = previousPlayer2States;

    // Create the player 2 matrix using the maybe player 2 states.
    storm::storage::SparseMatrix<ValueType> submatrix = transitionMatrix.getSubmatrix(true, player2MaybeStates, maybeStates);
    std::vector<ValueType> b = transitionMatrix.getConstrainedRowGroupSumVector(player2MaybeStates, player1Prob1States);

    // Set up game solver.
    auto gameSolver = storm::solver::GameSolverFactory<ValueType>().create(env, subPlayer1Groups, submatrix);

    // Prepare the value storage for the maybe states. If the starting values were given, extract them now.
    std::vector<ValueType> values(maybeStates.getNumberOfSetBits());
    if (startingQuantitativeResult) {
        storm::utility::vector::selectVectorValues(values, maybeStates, startingQuantitativeResult->getValues());
    }
    if (previousResult) {
        STORM_LOG_ASSERT(!startingQuantitativeResult, "Cannot take two different hints.");
        storm::utility::vector::selectVectorValues(values, maybeStates, result.getValues());
    }

    // Prepare scheduler storage.
    std::vector<uint64_t> player1Scheduler(subPlayer1Groups.size() - 1);
    std::vector<uint64_t> player2Scheduler(submatrix.getRowGroupCount());
    if (startingStrategyPair) {
        // If the starting strategy pair was provided, we need to extract the choices of the maybe states here.
        uint64_t maybeStatePosition = 0;
        previousPlayer2States = 0;
        for (uint64_t state : maybeStates) {
            uint64_t chosenPlayer2State = startingStrategyPair->getPlayer1Strategy().getChoice(state);

            uint64_t previousPlayer2MaybeStatesForState = 0;
            for (uint64_t player2State = player1Groups[state]; player2State < player1Groups[state + 1]; ++player2State) {
                if (player2MaybeStates.get(player2State)) {
                    if (player2State == chosenPlayer2State) {
                        player1Scheduler[maybeStatePosition] = previousPlayer2MaybeStatesForState;
                    }

                    // Copy over the player 2 action (modulo making it local) as all rows for the player 2 state are taken.
                    if (startingStrategyPair->getPlayer2Strategy().hasDefinedChoice(player2State)) {
                        player2Scheduler[previousPlayer2States] =
                            startingStrategyPair->getPlayer2Strategy().getChoice(player2State) - transitionMatrix.getRowGroupIndices()[player2State];
                    } else {
                        player2Scheduler[previousPlayer2States] = 0;
                    }

                    ++previousPlayer2MaybeStatesForState;
                    ++previousPlayer2States;
                }
            }

            ++maybeStatePosition;
        }
        STORM_LOG_ASSERT(previousPlayer2States == submatrix.getRowGroupCount(), "Expected correct number of player 2 states.");
    }

    // Solve actual game and track schedulers.
    gameSolver->solveGame(env, player1Direction, player2Direction, values, b, &player1Scheduler, &player2Scheduler);

    // Set values according to quantitative result (qualitative result has already been taken care of).
    storm::utility::vector::setVectorValues(result.getValues(), maybeStates, values);

    // Obtain strategies from solver and fuse them with the pre-existing strategy pair for the qualitative result.
    uint64_t previousPlayer1MaybeStates = 0;
    uint64_t previousPlayer2MaybeStates = 0;
    for (uint64_t state : maybeStates) {
        uint64_t previousPlayer2MaybeStatesForState = 0;
        [[maybe_unused]] bool madePlayer1Choice = false;
        for (uint64_t player2State = player1Groups[state]; player2State < player1Groups[state + 1]; ++player2State) {
            if (player1Scheduler[previousPlayer1MaybeStates] == previousPlayer2MaybeStatesForState) {
                strategyPair.getPlayer1Strategy().setChoice(state, player2State);
                madePlayer1Choice = true;
            }

            if (player2MaybeStates.get(player2State)) {
                strategyPair.getPlayer2Strategy().setChoice(player2State,
                                                            transitionMatrix.getRowGroupIndices()[player2State] + player2Scheduler[previousPlayer2MaybeStates]);

                ++previousPlayer2MaybeStatesForState;
                ++previousPlayer2MaybeStates;
            }
        }
        STORM_LOG_ASSERT(madePlayer1Choice, "[" << player1Direction << "]: player 1 state " << state
                                                << " did not make a choice, scheduler: " << player1Scheduler[previousPlayer1MaybeStates] << ".");

        ++previousPlayer1MaybeStates;
    }

    return result;
}

template<storm::dd::DdType Type, typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> GameBasedMdpModelChecker<Type, ModelType>::performGameBasedAbstractionRefinement(
    Environment const& env, storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask,
    storm::expressions::Expression const& constraintExpression, storm::expressions::Expression const& targetStateExpression) {
    STORM_LOG_THROW(checkTask.isOnlyInitialStatesRelevantSet(), storm::exceptions::InvalidPropertyException,
                    "The game-based abstraction refinement model checker can only compute the result for the initial states.");

    // Optimization: do not compute both bounds if not necessary (e.g. if bound given and exceeded, etc.)
    totalWatch.start();

    // Set up initial predicates.
    setupWatch.start();
    std::vector<storm::expressions::Expression> initialPredicates = getInitialPredicates(constraintExpression, targetStateExpression);

    // Derive the optimization direction for player 1 (assuming menu-game abstraction).
    storm::OptimizationDirection player1Direction = getPlayer1Direction(checkTask);

    // Create the abstractor.
    storm::gbar::abstraction::MenuGameAbstractorOptions abstractorOptions(std::move(options.constraints));
    if (preprocessedModel.isPrismProgram()) {
        abstractor = std::make_shared<storm::gbar::abstraction::prism::PrismMenuGameAbstractor<Type, ValueType>>(env, preprocessedModel.asPrismProgram(),
                                                                                                                 smtSolverFactory, abstractorOptions);
    } else {
        abstractor = std::make_shared<storm::gbar::abstraction::jani::JaniMenuGameAbstractor<Type, ValueType>>(env, preprocessedModel.asJaniModel(),
                                                                                                               smtSolverFactory, abstractorOptions);
    }
    std::unique_ptr<storm::modelchecker::CheckResult> result;
    abstractor->getDdManager().execute([&]() {
        if (!constraintExpression.isTrue()) {
            abstractor->addTerminalStates(!constraintExpression);
        }
        abstractor->addTerminalStates(targetStateExpression);
        abstractor->setTargetStates(targetStateExpression);

        // Create a refiner that can be used to refine the abstraction when needed.
        storm::gbar::abstraction::MenuGameRefinerOptions refinerOptions(std::move(options.injectedRefinementPredicates));
        storm::gbar::abstraction::MenuGameRefiner<Type, ValueType> refiner(*abstractor, smtSolverFactory->create(preprocessedModel.getManager()),
                                                                           refinerOptions);
        refiner.refine(initialPredicates, false);

        storm::dd::Bdd<Type> globalConstraintStates = abstractor->getStates(constraintExpression);
        storm::dd::Bdd<Type> globalTargetStates = abstractor->getStates(targetStateExpression);
        setupWatch.stop();

        // Enter the main-loop of abstraction refinement.
        boost::optional<SymbolicQualitativeGameResultMinMax<Type>> previousSymbolicQualitativeResult = boost::none;
        boost::optional<SymbolicQuantitativeGameResult<Type, ValueType>> previousSymbolicMinQuantitativeResult = boost::none;
        boost::optional<PreviousExplicitResult<ValueType>> previousExplicitResult = boost::none;
        uint64_t peakPlayer1States = 0;
        uint64_t peakTransitions = 0;
        for (iteration = 0; iteration < maximalNumberOfAbstractions; ++iteration) {
            auto iterationStart = std::chrono::high_resolution_clock::now();
            STORM_LOG_TRACE("Starting iteration " << iteration << ".");

            // (1) build the abstraction.
            storm::utility::Stopwatch abstractionWatch(true);
            storm::gbar::abstraction::MenuGame<Type, ValueType> game = abstractor->abstract();
            abstractionWatch.stop();
            totalAbstractionWatch.add(abstractionWatch);

            uint64_t numberOfPlayer1States = game.getNumberOfStates();
            peakPlayer1States = std::max(peakPlayer1States, numberOfPlayer1States);
            uint64_t numberOfTransitions = game.getNumberOfTransitions();
            peakTransitions = std::max(peakTransitions, numberOfTransitions);
            STORM_LOG_INFO("Abstraction in iteration "
                           << iteration << " has " << numberOfPlayer1States << " player 1 states (" << game.getInitialStates().getNonZeroCount()
                           << " initial), " << game.getNumberOfPlayer2States() << " player 2 states, " << numberOfTransitions << " transitions, "
                           << game.getBottomStates().getNonZeroCount() << " bottom states, " << abstractor->getNumberOfPredicates() << " predicate(s), "
                           << game.getTransitionMatrix().getNodeCount() << " nodes (transition matrix) (computed in "
                           << abstractionWatch.getTimeInMilliseconds() << "ms).");

            // (2) Prepare initial, constraint and target state BDDs for later use.
            storm::dd::Bdd<Type> initialStates = game.getInitialStates();
            //                STORM_LOG_THROW(initialStates.getNonZeroCount() == 1 || checkTask.isBoundSet(), storm::exceptions::InvalidPropertyException,
            //                "Game-based abstraction refinement requires a bound on the formula for model with " << initialStates.getNonZeroCount() << "
            //                initial states.");
            storm::dd::Bdd<Type> constraintStates = globalConstraintStates && game.getReachableStates();
            storm::dd::Bdd<Type> targetStates = globalTargetStates && game.getReachableStates();
            if (player1Direction == storm::OptimizationDirection::Minimize) {
                targetStates |= game.getBottomStates();
            }

            // #ifdef LOCAL_DEBUG
            //                initialStates.template toAdd<ValueType>().exportToDot("init" + std::to_string(iteration) + ".dot");
            //                targetStates.template toAdd<ValueType>().exportToDot("target" + std::to_string(iteration) + ".dot");
            //                abstractor->exportToDot("game" + std::to_string(iteration) + ".dot", targetStates, game.getManager().getBddOne());
            //                game.getReachableStates().template toAdd<ValueType>().exportToDot("reach" + std::to_string(iteration) + ".dot");
            // #endif

            if (solveMode == storm::settings::modules::AbstractionSettings::SolveMode::Dd) {
                result = performSymbolicAbstractionSolutionStep(env, checkTask, game, player1Direction, initialStates, constraintStates, targetStates, refiner,
                                                                previousSymbolicQualitativeResult, previousSymbolicMinQuantitativeResult);
            } else {
                result = performExplicitAbstractionSolutionStep(env, checkTask, game, player1Direction, initialStates, constraintStates, targetStates, refiner,
                                                                previousExplicitResult);
            }

            if (result) {
                totalWatch.stop();
                printStatistics(*abstractor, game, iteration, peakPlayer1States, peakTransitions);
                return;
            }

            auto iterationEnd = std::chrono::high_resolution_clock::now();
            STORM_LOG_INFO("Iteration " << iteration << " took " << std::chrono::duration_cast<std::chrono::milliseconds>(iterationEnd - iterationStart).count()
                                        << "ms.");
        }
    });
    if (result) {
        return result;
    }
    totalWatch.stop();

    // If this point is reached, we have given up on abstraction.
    STORM_LOG_WARN("Could not derive result, maximal number of abstractions exceeded.");
    return nullptr;
}

template<storm::dd::DdType Type, typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> GameBasedMdpModelChecker<Type, ModelType>::performSymbolicAbstractionSolutionStep(
    Environment const& env, storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask,
    storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::OptimizationDirection player1Direction, storm::dd::Bdd<Type> const& initialStates,
    storm::dd::Bdd<Type> const& constraintStates, storm::dd::Bdd<Type> const& targetStates,
    storm::gbar::abstraction::MenuGameRefiner<Type, ValueType> const& refiner,
    boost::optional<SymbolicQualitativeGameResultMinMax<Type>>& previousQualitativeResult,
    boost::optional<SymbolicQuantitativeGameResult<Type, ValueType>>& previousMinQuantitativeResult) {
    STORM_LOG_TRACE("Using dd-based solving.");

    // Prepare transition matrix BDD.
    storm::dd::Bdd<Type> transitionMatrixBdd = game.getTransitionMatrix().toBdd();

    // (1) compute all states with probability 0/1 wrt. to the two different player 2 goals (min/max).
    storm::utility::Stopwatch qualitativeWatch(true);
    SymbolicQualitativeGameResultMinMax<Type> qualitativeResult =
        computeProb01States(previousQualitativeResult, game, player1Direction, transitionMatrixBdd, constraintStates, targetStates);
    std::unique_ptr<storm::modelchecker::CheckResult> result =
        checkForResultAfterQualitativeCheck<Type, ValueType>(checkTask, initialStates, qualitativeResult);
    if (result) {
        return result;
    }
    previousQualitativeResult = qualitativeResult;
    qualitativeWatch.stop();
    totalSolutionWatch.add(qualitativeWatch);
    STORM_LOG_INFO("Qualitative computation completed in " << qualitativeWatch.getTimeInMilliseconds() << "ms.");

    // (2) compute the states for which we have to determine quantitative information.
    storm::dd::Bdd<Type> maybeMin =
        !(qualitativeResult.prob0Min.getPlayer1States() || qualitativeResult.prob1Min.getPlayer1States()) && game.getReachableStates();
    storm::dd::Bdd<Type> maybeMax =
        !(qualitativeResult.prob0Max.getPlayer1States() || qualitativeResult.prob1Max.getPlayer1States()) && game.getReachableStates();

    // (3) if the initial states are not maybe states, then we can refine at this point.
    storm::dd::Bdd<Type> initialMaybeStates = (initialStates && maybeMin) || (initialStates && maybeMax);
    bool qualitativeRefinement = false;
    if (initialMaybeStates.isZero()) {
        // In this case, we know the result for the initial states for both player 2 minimizing and maximizing.
        STORM_LOG_TRACE("No initial state is a 'maybe' state.");

        STORM_LOG_INFO("Obtained qualitative bounds [0, 1] on the actual value for the initial states (after "
                       << totalWatch.getTimeInMilliseconds() << "ms in iteration " << this->iteration << "). Refining abstraction based on qualitative check.");

        // If we get here, the initial states were all identified as prob0/1 states, but the value (0 or 1)
        // depends on whether player 2 is minimizing or maximizing. Therefore, we need to find a place to refine.
        storm::utility::Stopwatch refinementWatch(true);
        qualitativeRefinement = refiner.refine(game, transitionMatrixBdd, qualitativeResult);
        refinementWatch.stop();
        totalRefinementWatch.add(refinementWatch);
        STORM_LOG_INFO("Qualitative refinement completed in " << refinementWatch.getTimeInMilliseconds() << "ms.");
    }

    // (4) if we arrived at this point and no refinement was made, we need to compute the quantitative solution.
    if (!qualitativeRefinement) {
        // At this point, we know that we cannot answer the query without further numeric computation.
        STORM_LOG_TRACE("Starting numerical solution step.");

        storm::dd::Add<Type, ValueType> initialStatesAdd = initialStates.template toAdd<ValueType>();

        SymbolicQuantitativeGameResultMinMax<Type, ValueType> quantitativeResult;

        // (7) Solve the min values and check whether we can give the answer already.
        storm::utility::Stopwatch quantitativeWatch(true);
        quantitativeResult.min = computeQuantitativeResult(env, player1Direction, storm::OptimizationDirection::Minimize, game, qualitativeResult,
                                                           initialStatesAdd, maybeMin, reuseQuantitativeResults ? previousMinQuantitativeResult : boost::none);
        quantitativeWatch.stop();
        previousMinQuantitativeResult = quantitativeResult.min;
        result =
            checkForResultAfterQuantitativeCheck<ValueType>(checkTask, storm::OptimizationDirection::Minimize, quantitativeResult.min.getInitialStatesRange());
        if (result) {
            totalSolutionWatch.add(quantitativeWatch);
            return result;
        }

        // (8) Solve the max values and check whether we can give the answer already.
        quantitativeWatch.start();
        quantitativeResult.max = computeQuantitativeResult(env, player1Direction, storm::OptimizationDirection::Maximize, game, qualitativeResult,
                                                           initialStatesAdd, maybeMax, boost::make_optional(quantitativeResult.min));
        quantitativeWatch.stop();
        result =
            checkForResultAfterQuantitativeCheck<ValueType>(checkTask, storm::OptimizationDirection::Maximize, quantitativeResult.max.getInitialStatesRange());
        totalSolutionWatch.add(quantitativeWatch);
        if (result) {
            return result;
        }

        ValueType minVal = quantitativeResult.min.getInitialStatesRange().first;
        ValueType maxVal = quantitativeResult.max.getInitialStatesRange().second;
        ValueType difference = maxVal - minVal;
        if (std::is_same<ValueType, double>::value) {
            std::stringstream differenceStream;
            differenceStream.setf(std::ios::fixed, std::ios::floatfield);
            differenceStream.precision(15);
            differenceStream << difference;
            STORM_LOG_INFO("Obtained quantitative bounds [" << minVal << ", " << maxVal << "] (difference " << differenceStream.str()
                                                            << ") on the actual value for the initial states in " << quantitativeWatch.getTimeInMilliseconds()
                                                            << "ms (after " << totalWatch.getTimeInMilliseconds() << "ms in iteration " << this->iteration
                                                            << ").");
        } else {
            STORM_LOG_INFO("Obtained quantitative bounds [" << minVal << ", " << maxVal << "] (approx. [" << storm::utility::convertNumber<double>(minVal)
                                                            << ", " << storm::utility::convertNumber<double>(maxVal) << "], difference " << difference
                                                            << ") on the actual value for the initial states in " << quantitativeWatch.getTimeInMilliseconds()
                                                            << "ms (after " << totalWatch.getTimeInMilliseconds() << "ms in iteration " << this->iteration
                                                            << ").");
        }

        // (9) Check whether the lower and upper bounds are close enough to terminate with an answer.
        result = checkForResultAfterQuantitativeCheck<ValueType>(quantitativeResult.min.getInitialStatesRange().first,
                                                                 quantitativeResult.max.getInitialStatesRange().second, comparator);
        if (result) {
            return result;
        }

        // Make sure that all strategies are still valid strategies.
        STORM_LOG_ASSERT(quantitativeResult.min.getPlayer1Strategy().isZero() ||
                             quantitativeResult.min.getPlayer1Strategy().template toAdd<ValueType>().sumAbstract(game.getPlayer1Variables()).getMax() <= 1,
                         "Player 1 strategy for min is illegal.");
        STORM_LOG_ASSERT(quantitativeResult.max.getPlayer1Strategy().isZero() ||
                             quantitativeResult.max.getPlayer1Strategy().template toAdd<ValueType>().sumAbstract(game.getPlayer1Variables()).getMax() <= 1,
                         "Player 1 strategy for max is illegal.");
        STORM_LOG_ASSERT(quantitativeResult.min.getPlayer2Strategy().isZero() ||
                             quantitativeResult.min.getPlayer2Strategy().template toAdd<ValueType>().sumAbstract(game.getPlayer2Variables()).getMax() <= 1,
                         "Player 2 strategy for min is illegal.");
        STORM_LOG_ASSERT(quantitativeResult.max.getPlayer2Strategy().isZero() ||
                             quantitativeResult.max.getPlayer2Strategy().template toAdd<ValueType>().sumAbstract(game.getPlayer2Variables()).getMax() <= 1,
                         "Player 2 strategy for max is illegal.");

        // (10) If we arrived at this point, it means that we have all qualitative and quantitative
        // information about the game, but we could not yet answer the query. In this case, we need to refine.
        storm::utility::Stopwatch refinementWatch(true);
        refiner.refine(game, transitionMatrixBdd, quantitativeResult);
        refinementWatch.stop();
        totalRefinementWatch.add(refinementWatch);
        STORM_LOG_INFO("Quantitative refinement completed in " << refinementWatch.getTimeInMilliseconds() << "ms.");
    }

    // Return null to indicate no result has been found yet.
    return nullptr;
}

template<typename ValueType>
void postProcessStrategies(storm::OptimizationDirection const& player1Direction, storage::ExplicitGameStrategyPair& minStrategyPair,
                           storage::ExplicitGameStrategyPair& maxStrategyPair, std::vector<uint64_t> const& player1Groups,
                           std::vector<uint64_t> const& player2Groups, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                           storm::storage::BitVector const& constraintStates, storm::storage::BitVector const& targetStates,
                           ExplicitQualitativeGameResultMinMax const& qualitativeResult, bool redirectPlayer1, bool redirectPlayer2, bool sanityCheck) {
    if (!redirectPlayer1 && !redirectPlayer2) {
        return;
    }

    for (uint64_t state = 0; state < player1Groups.size() - 1; ++state) {
        bool isProb0Min = qualitativeResult.getProb0Min().getStates().get(state);

        bool hasMinPlayer1Choice = false;
        uint64_t lowerPlayer1Choice = 0;
        bool hasMaxPlayer1Choice = false;
        uint64_t upperPlayer1Choice = 0;

        if (minStrategyPair.getPlayer1Strategy().hasDefinedChoice(state)) {
            hasMinPlayer1Choice = true;
            lowerPlayer1Choice = minStrategyPair.getPlayer1Strategy().getChoice(state);

            if (maxStrategyPair.getPlayer2Strategy().hasDefinedChoice(lowerPlayer1Choice)) {
                uint64_t lowerPlayer2Choice = minStrategyPair.getPlayer2Strategy().getChoice(lowerPlayer1Choice);
                uint64_t upperPlayer2Choice = maxStrategyPair.getPlayer2Strategy().getChoice(lowerPlayer1Choice);

                if (lowerPlayer2Choice == upperPlayer2Choice) {
                    continue;
                }

                bool redirect = true;
                if (isProb0Min) {
                    for (auto const& entry : transitionMatrix.getRow(upperPlayer2Choice)) {
                        if (!qualitativeResult.getProb0Min().getStates().get(entry.getColumn())) {
                            redirect = false;
                            break;
                        }
                    }
                }

                if (redirectPlayer2 && redirect) {
                    minStrategyPair.getPlayer2Strategy().setChoice(lowerPlayer1Choice, upperPlayer2Choice);
                }
            }
        }

        bool lowerChoiceUnderUpperIsProb0 = false;
        if (maxStrategyPair.getPlayer1Strategy().hasDefinedChoice(state)) {
            upperPlayer1Choice = maxStrategyPair.getPlayer1Strategy().getChoice(state);

            if (lowerPlayer1Choice != upperPlayer1Choice && minStrategyPair.getPlayer2Strategy().hasDefinedChoice(upperPlayer1Choice)) {
                hasMaxPlayer1Choice = true;

                uint64_t lowerPlayer2Choice = minStrategyPair.getPlayer2Strategy().getChoice(upperPlayer1Choice);
                uint64_t upperPlayer2Choice = maxStrategyPair.getPlayer2Strategy().getChoice(upperPlayer1Choice);

                if (lowerPlayer2Choice == upperPlayer2Choice) {
                    continue;
                }

                lowerChoiceUnderUpperIsProb0 = true;
                for (auto const& entry : transitionMatrix.getRow(lowerPlayer2Choice)) {
                    if (!qualitativeResult.getProb0Min().getStates().get(entry.getColumn())) {
                        lowerChoiceUnderUpperIsProb0 = false;
                        break;
                    }
                }

                bool redirect = true;
                if (lowerChoiceUnderUpperIsProb0) {
                    for (auto const& entry : transitionMatrix.getRow(upperPlayer2Choice)) {
                        if (!qualitativeResult.getProb0Min().getStates().get(entry.getColumn())) {
                            redirect = false;
                            break;
                        }
                    }
                }

                if (redirectPlayer2 && redirect) {
                    minStrategyPair.getPlayer2Strategy().setChoice(lowerPlayer1Choice, upperPlayer2Choice);
                }
            }
        }

        if (redirectPlayer1 && player1Direction == storm::OptimizationDirection::Minimize) {
            if (hasMinPlayer1Choice && hasMaxPlayer1Choice && lowerPlayer1Choice != upperPlayer1Choice) {
                if (!isProb0Min || lowerChoiceUnderUpperIsProb0) {
                    minStrategyPair.getPlayer1Strategy().setChoice(state, upperPlayer1Choice);
                }
            }
        }
    }
}

template<typename ValueType>
class ExplicitGameExporter {
   public:
    ExplicitGameExporter() : showNonStrategyAlternatives(false) {
        // Intentionally left empty.
    }

    void exportToJson(std::string const& filename, std::vector<uint64_t> const& player1Groups, std::vector<uint64_t> const& player2Groups,
                      storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& initialStates,
                      storm::storage::BitVector const& constraintStates, storm::storage::BitVector const& targetStates,
                      ExplicitQuantitativeResultMinMax<ValueType> const& quantitativeResult, storage::ExplicitGameStrategyPair const* minStrategyPair,
                      storage::ExplicitGameStrategyPair const* maxStrategyPair) {
        // Export game as json.
        std::ofstream outfile;
        storm::io::openFile(filename, outfile);
        exportGame(outfile, player1Groups, player2Groups, transitionMatrix, initialStates, constraintStates, targetStates, quantitativeResult, minStrategyPair,
                   maxStrategyPair);
        storm::io::closeFile(outfile);
    }

    void setShowNonStrategyAlternatives(bool value) {
        showNonStrategyAlternatives = value;
    }

   private:
    struct NodeData {
        NodeData(uint64_t id, uint64_t player, bool initial, bool target) : id(id), player(player), initial(initial), target(target) {
            // Intentionally left empty.
        }

        uint64_t id;
        uint64_t player;  // 0 = probabilistic player, 1 = player 1, 2 = player 2
        bool initial;
        bool target;
    };

    struct EdgeData {
        EdgeData(uint64_t id, uint64_t source, uint64_t target, ValueType probability, uint64_t label, bool min, bool max)
            : id(id), source(source), target(target), probability(probability), label(label), min(min), max(max) {
            // Intentionally left empty.
        }

        uint64_t id;
        uint64_t source;
        uint64_t target;
        ValueType probability;
        uint64_t label;
        bool min;
        bool max;
    };

    void exportEdge(std::ofstream& out, EdgeData const& data, bool& first) {
        if (!first) {
            out << ",\n";
        } else {
            first = false;
        }
        out << "\t\t{\n";
        out << "\t\t\t\"data\": {\n";
        out << "\t\t\t\t\"id\": \"" << data.id << "\",\n";
        if (data.probability != storm::utility::zero<ValueType>()) {
            out << "\t\t\t\t\"name\": \"" << data.probability << "\",\n";
        } else {
            out << "\t\t\t\t\"name\": \"" << data.label << "\",\n";
        }
        out << "\t\t\t\t\"source\": \"" << data.source << "\",\n";
        out << "\t\t\t\t\"target\": \"" << data.target << "\"\n";
        out << "\t\t\t},\n";
        out << "\t\t\t\"classes\": \"";
        if (data.min && data.max) {
            out << "minMaxEdge";
        } else if (data.min) {
            out << "minEdge";
        } else if (data.max) {
            out << "maxEdge";
        } else {
            out << "edge";
        }
        out << "\"\n";
        out << "\t\t}";
    }

    void exportNode(std::ofstream& out, NodeData const& data, ExplicitQuantitativeResultMinMax<ValueType> const* quantitativeResult, bool& first) {
        if (!first) {
            out << ",\n";
        } else {
            first = false;
        }
        out << "\t\t{\n";
        out << "\t\t\t\"data\": {\n";
        out << "\t\t\t\t\"id\": \"" << data.id << "\",\n";
        out << "\t\t\t\t\"name\": \"" << data.id;
        if (quantitativeResult && data.player == 1) {
            out << " [" << quantitativeResult->getMin().getValues()[data.id] << ", " << quantitativeResult->getMax().getValues()[data.id] << "]";
        }
        out << "\"\n";
        out << "\t\t\t},\n";
        out << "\t\t\t\"group\": \"nodes\",\n";
        out << "\t\t\t\"classes\": \"";
        if (data.player == 1) {
            if (data.initial) {
                out << "initialNode";
            } else if (data.target) {
                out << "targetNode";
            } else {
                out << "node";
            }
        } else if (data.player == 2) {
            out << "pl2node";
        } else if (data.player == 0) {
            out << "plpnode";
        }
        out << "\"\n";
        out << "\t\t}";
    }

    void exportGame(std::ofstream& out, std::vector<uint64_t> const& player1Groups, std::vector<uint64_t> const& player2Groups,
                    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& initialStates,
                    storm::storage::BitVector const& constraintStates, storm::storage::BitVector const& targetStates,
                    ExplicitQuantitativeResultMinMax<ValueType> const& quantitativeResult, storage::ExplicitGameStrategyPair const* minStrategyPair,
                    storage::ExplicitGameStrategyPair const* maxStrategyPair) {
        // To export the game as JSON, we build some data structures through a traversal and then emit them.
        std::vector<NodeData> nodes;
        std::vector<EdgeData> edges;

        std::vector<uint64_t> stack;
        for (uint64_t state : initialStates) {
            stack.push_back(state);
        }
        storm::storage::BitVector reachablePlayer1(player1Groups.size() - 1);

        uint64_t edgeId = 0;
        while (!stack.empty()) {
            uint64_t currentState = stack.back();
            stack.pop_back();

            nodes.emplace_back(currentState, 1, initialStates.get(currentState), targetStates.get(currentState));

            for (uint64_t player2State = player1Groups[currentState]; player2State < player1Groups[currentState + 1]; ++player2State) {
                bool emit = (minStrategyPair || maxStrategyPair) ? this->showNonStrategyAlternatives : true;
                bool min = false;
                bool max = false;

                if (minStrategyPair && minStrategyPair->getPlayer1Strategy().hasDefinedChoice(currentState) &&
                    minStrategyPair->getPlayer1Strategy().getChoice(currentState) == player2State) {
                    emit = true;
                    min = true;
                }
                if (maxStrategyPair && maxStrategyPair->getPlayer1Strategy().hasDefinedChoice(currentState) &&
                    maxStrategyPair->getPlayer1Strategy().getChoice(currentState) == player2State) {
                    emit = true;
                    max = true;
                }

                if (emit) {
                    nodes.emplace_back(player2State, 2, false, false);
                    edges.emplace_back(edgeId++, currentState, player2State, storm::utility::zero<ValueType>(), player2State - player1Groups[currentState], min,
                                       max);

                    for (uint64_t playerPState = player2Groups[player2State]; playerPState < player2Groups[player2State + 1]; ++playerPState) {
                        emit = (minStrategyPair || maxStrategyPair) ? this->showNonStrategyAlternatives : true;
                        min = false;
                        max = false;

                        if (minStrategyPair && minStrategyPair->getPlayer2Strategy().hasDefinedChoice(player2State) &&
                            minStrategyPair->getPlayer2Strategy().getChoice(player2State) == playerPState) {
                            emit = true;
                            min = true;
                        }
                        if (maxStrategyPair && maxStrategyPair->getPlayer2Strategy().hasDefinedChoice(player2State) &&
                            maxStrategyPair->getPlayer2Strategy().getChoice(player2State) == playerPState) {
                            emit = true;
                            max = true;
                        }

                        if (emit) {
                            nodes.emplace_back(playerPState, 0, false, false);
                            edges.emplace_back(edgeId++, player2State, playerPState, storm::utility::zero<ValueType>(),
                                               playerPState - player2Groups[player2State], min, max);

                            for (auto const& entry : transitionMatrix.getRow(playerPState)) {
                                auto player1Successor = entry.getColumn();
                                if (!reachablePlayer1.get(player1Successor)) {
                                    reachablePlayer1.set(player1Successor);
                                    stack.push_back(player1Successor);
                                }

                                edges.emplace_back(edgeId++, playerPState, player1Successor, entry.getValue(), 0, false, false);
                            }
                        }
                    }
                }
            }
        }

        // Finally, export the data structures we built.

        // Export nodes.
        out << "{\n\t\"nodes\": [\n";
        bool first = true;
        for (auto const& node : nodes) {
            exportNode(out, node, &quantitativeResult, first);
        }
        out << "\n\t],\n";

        // Export edges.
        first = true;
        out << "\t\"edges\": [\n";
        for (auto const& edge : edges) {
            exportEdge(out, edge, first);
        }
        out << "\n\t]\n}\n";
    }

    bool showNonStrategyAlternatives;
};

template<typename ValueType>
void postProcessStrategies(uint64_t iteration, storm::OptimizationDirection const& player1Direction, storage::ExplicitGameStrategyPair& minStrategyPair,
                           storage::ExplicitGameStrategyPair& maxStrategyPair, std::vector<uint64_t> const& player1Groups,
                           std::vector<uint64_t> const& player2Groups, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                           storm::storage::BitVector const& initialStates, storm::storage::BitVector const& constraintStates,
                           storm::storage::BitVector const& targetStates, ExplicitQuantitativeResultMinMax<ValueType> const& quantitativeResult,
                           bool redirectPlayer1, bool redirectPlayer2, bool sanityCheck) {
    if (redirectPlayer1 || redirectPlayer2) {
        for (uint64_t state = 0; state < player1Groups.size() - 1; ++state) {
            STORM_LOG_ASSERT(targetStates.get(state) || minStrategyPair.getPlayer1Strategy().hasDefinedChoice(state),
                             "Expected lower player 1 choice in state " << state << ".");
            STORM_LOG_ASSERT(targetStates.get(state) || maxStrategyPair.getPlayer1Strategy().hasDefinedChoice(state),
                             "Expected upper player 1 choice in state " << state << ".");

            bool hasMinPlayer1Choice = false;
            uint64_t lowerPlayer1Choice = 0;
            ValueType lowerValueUnderMinChoicePlayer1 = storm::utility::zero<ValueType>();
            bool hasMaxPlayer1Choice = false;
            uint64_t upperPlayer1Choice = 0;
            ValueType lowerValueUnderMaxChoicePlayer1 = storm::utility::zero<ValueType>();

            if (minStrategyPair.getPlayer1Strategy().hasDefinedChoice(state)) {
                hasMinPlayer1Choice = true;
                lowerPlayer1Choice = minStrategyPair.getPlayer1Strategy().getChoice(state);

                STORM_LOG_ASSERT(minStrategyPair.getPlayer2Strategy().hasDefinedChoice(lowerPlayer1Choice),
                                 "Expected lower player 2 choice for state " << state << " (lower player 1 choice " << lowerPlayer1Choice << ").");
                uint64_t lowerPlayer2Choice = minStrategyPair.getPlayer2Strategy().getChoice(lowerPlayer1Choice);

                ValueType lowerValueUnderLowerChoicePlayer2 =
                    transitionMatrix.multiplyRowWithVector(lowerPlayer2Choice, quantitativeResult.getMin().getValues());
                lowerValueUnderMinChoicePlayer1 = lowerValueUnderLowerChoicePlayer2;

                if (maxStrategyPair.getPlayer2Strategy().hasDefinedChoice(lowerPlayer1Choice)) {
                    uint64_t upperPlayer2Choice = maxStrategyPair.getPlayer2Strategy().getChoice(lowerPlayer1Choice);

                    if (lowerPlayer2Choice != upperPlayer2Choice) {
                        ValueType lowerValueUnderUpperChoicePlayer2 =
                            transitionMatrix.multiplyRowWithVector(upperPlayer2Choice, quantitativeResult.getMin().getValues());

                        if (redirectPlayer2 && lowerValueUnderUpperChoicePlayer2 <= lowerValueUnderLowerChoicePlayer2) {
                            lowerValueUnderMinChoicePlayer1 = lowerValueUnderUpperChoicePlayer2;
                            minStrategyPair.getPlayer2Strategy().setChoice(lowerPlayer1Choice, upperPlayer2Choice);
                        }
                    }
                }
            }

            if (maxStrategyPair.getPlayer1Strategy().hasDefinedChoice(state)) {
                upperPlayer1Choice = maxStrategyPair.getPlayer1Strategy().getChoice(state);

                if (upperPlayer1Choice != lowerPlayer1Choice && minStrategyPair.getPlayer2Strategy().hasDefinedChoice(upperPlayer1Choice)) {
                    hasMaxPlayer1Choice = true;

                    uint64_t lowerPlayer2Choice = minStrategyPair.getPlayer2Strategy().getChoice(upperPlayer1Choice);

                    ValueType lowerValueUnderLowerChoicePlayer2 =
                        transitionMatrix.multiplyRowWithVector(lowerPlayer2Choice, quantitativeResult.getMin().getValues());
                    lowerValueUnderMaxChoicePlayer1 = lowerValueUnderLowerChoicePlayer2;

                    STORM_LOG_ASSERT(maxStrategyPair.getPlayer2Strategy().hasDefinedChoice(upperPlayer1Choice),
                                     "Expected upper player 2 choice for state " << state << " (upper player 1 choice " << upperPlayer1Choice << ").");
                    uint64_t upperPlayer2Choice = maxStrategyPair.getPlayer2Strategy().getChoice(upperPlayer1Choice);

                    if (lowerPlayer2Choice != upperPlayer2Choice) {
                        ValueType lowerValueUnderUpperChoicePlayer2 =
                            transitionMatrix.multiplyRowWithVector(upperPlayer2Choice, quantitativeResult.getMin().getValues());

                        if (redirectPlayer2 && lowerValueUnderUpperChoicePlayer2 <= lowerValueUnderLowerChoicePlayer2) {
                            minStrategyPair.getPlayer2Strategy().setChoice(upperPlayer1Choice, upperPlayer2Choice);
                        }
                    }
                }
            }

            if (redirectPlayer1 && player1Direction == storm::OptimizationDirection::Minimize) {
                if (hasMinPlayer1Choice && hasMaxPlayer1Choice && lowerPlayer1Choice != upperPlayer1Choice) {
                    if (lowerValueUnderMaxChoicePlayer1 <= lowerValueUnderMinChoicePlayer1) {
                        minStrategyPair.getPlayer1Strategy().setChoice(state, upperPlayer1Choice);
                    }
                }
            }
        }
    }

    if (sanityCheck) {
        storm::utility::ConstantsComparator<ValueType> sanityComparator(storm::utility::convertNumber<ValueType>(1e-6), true);

        ///////// SANITY CHECK: apply lower strategy, obtain DTMC matrix and model check it. the values should
        ///////// still be the lower ones.
        storm::storage::SparseMatrixBuilder<ValueType> dtmcMatrixBuilder(player1Groups.size() - 1, player1Groups.size() - 1);
        for (uint64_t state = 0; state < player1Groups.size() - 1; ++state) {
            if (targetStates.get(state)) {
                dtmcMatrixBuilder.addNextValue(state, state, storm::utility::one<ValueType>());
            } else {
                STORM_LOG_ASSERT(minStrategyPair.getPlayer1Strategy().hasDefinedChoice(state), "Expected min player 1 choice in state " << state << ".");
                STORM_LOG_ASSERT(minStrategyPair.getPlayer2Strategy().hasDefinedChoice(minStrategyPair.getPlayer1Strategy().getChoice(state)),
                                 "Expected max player 2 choice in state " << state << " with player 2 choice "
                                                                          << maxStrategyPair.getPlayer1Strategy().getChoice(state) << ".");
                uint64_t player2Choice = minStrategyPair.getPlayer2Strategy().getChoice(minStrategyPair.getPlayer1Strategy().getChoice(state));
                for (auto const& entry : transitionMatrix.getRow(player2Choice)) {
                    dtmcMatrixBuilder.addNextValue(state, entry.getColumn(), entry.getValue());
                }
            }
        }
        auto dtmcMatrix = dtmcMatrixBuilder.build();
        std::vector<ValueType> sanityValues = storm::modelchecker::helper::SparseDtmcPrctlHelper<ValueType>::computeUntilProbabilities(
            Environment(), storm::solver::SolveGoal<ValueType>(), dtmcMatrix, dtmcMatrix.transpose(), constraintStates, targetStates, false);

        ValueType maxDiff = storm::utility::zero<ValueType>();
        uint64_t maxState = 0;
        for (uint64_t state = 0; state < player1Groups.size() - 1; ++state) {
            ValueType diff = storm::utility::abs(ValueType(sanityValues[state] - quantitativeResult.getMin().getValues()[state]));
            if (diff > maxDiff) {
                maxState = state;
                maxDiff = diff;
            }
        }
        STORM_LOG_TRACE("Got maximal deviation of " << maxDiff << ".");
        STORM_LOG_WARN_COND(sanityComparator.isZero(maxDiff),
                            "Deviation " << maxDiff << " between computed value (" << quantitativeResult.getMin().getValues()[maxState]
                                         << ") and sanity check value (" << sanityValues[maxState] << ") in state " << maxState
                                         << " appears to be too high. (Obtained bounds were [" << quantitativeResult.getMin().getValues()[maxState] << ", "
                                         << quantitativeResult.getMax().getValues()[maxState] << "].)");

        ///////// SANITY CHECK: apply upper strategy, obtain DTMC matrix and model check it. the values should
        ///////// still be the upper ones.
        dtmcMatrixBuilder = storm::storage::SparseMatrixBuilder<ValueType>(player1Groups.size() - 1, player1Groups.size() - 1);
        for (uint64_t state = 0; state < player1Groups.size() - 1; ++state) {
            if (targetStates.get(state)) {
                dtmcMatrixBuilder.addNextValue(state, state, storm::utility::one<ValueType>());
            } else {
                STORM_LOG_ASSERT(maxStrategyPair.getPlayer1Strategy().hasDefinedChoice(state), "Expected max player 1 choice in state " << state << ".");
                STORM_LOG_ASSERT(maxStrategyPair.getPlayer2Strategy().hasDefinedChoice(maxStrategyPair.getPlayer1Strategy().getChoice(state)),
                                 "Expected max player 2 choice in state " << state << " with player 2 choice "
                                                                          << maxStrategyPair.getPlayer1Strategy().getChoice(state) << ".");
                uint64_t player2Choice = maxStrategyPair.getPlayer2Strategy().getChoice(maxStrategyPair.getPlayer1Strategy().getChoice(state));

                for (auto const& entry : transitionMatrix.getRow(player2Choice)) {
                    dtmcMatrixBuilder.addNextValue(state, entry.getColumn(), entry.getValue());
                }
            }
        }
        dtmcMatrix = dtmcMatrixBuilder.build();
        sanityValues = storm::modelchecker::helper::SparseDtmcPrctlHelper<ValueType>::computeUntilProbabilities(
            Environment(), storm::solver::SolveGoal<ValueType>(), dtmcMatrix, dtmcMatrix.transpose(), constraintStates, targetStates, false);

        maxDiff = storm::utility::zero<ValueType>();
        maxState = 0;
        for (uint64_t state = 0; state < player1Groups.size() - 1; ++state) {
            ValueType diff = storm::utility::abs(ValueType(sanityValues[state] - quantitativeResult.getMax().getValues()[state]));
            if (diff > maxDiff) {
                maxState = state;
                maxDiff = diff;
            }
        }
        STORM_LOG_TRACE("Got maximal deviation of " << maxDiff << ".");
        STORM_LOG_WARN_COND(sanityComparator.isZero(maxDiff),
                            "Deviation " << maxDiff << " between computed value (" << quantitativeResult.getMax().getValues()[maxState]
                                         << ") and sanity check value (" << sanityValues[maxState] << ") in state " << maxState
                                         << " appears to be too high. (Obtained bounds were [" << quantitativeResult.getMin().getValues()[maxState] << ", "
                                         << quantitativeResult.getMax().getValues()[maxState] << "].)");
    }
}

template<storm::dd::DdType Type, typename ModelType>
std::unique_ptr<storm::modelchecker::CheckResult> GameBasedMdpModelChecker<Type, ModelType>::performExplicitAbstractionSolutionStep(
    Environment const& env, storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask,
    storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::OptimizationDirection player1Direction,
    storm::dd::Bdd<Type> const& initialStatesBdd, storm::dd::Bdd<Type> const& constraintStatesBdd, storm::dd::Bdd<Type> const& targetStatesBdd,
    storm::gbar::abstraction::MenuGameRefiner<Type, ValueType> const& refiner, boost::optional<PreviousExplicitResult<ValueType>>& previousResult) {
    STORM_LOG_TRACE("Using sparse solving.");

    // (0) Start by transforming the necessary symbolic elements to explicit ones.
    storm::utility::Stopwatch translationWatch(true);
    storm::dd::Odd odd = game.getReachableStates().createOdd();

    std::vector<std::set<storm::expressions::Variable>> labelingVariableSets = {game.getPlayer1Variables(), game.getPlayer2Variables()};
    typename storm::dd::Add<Type, ValueType>::MatrixAndLabeling matrixAndLabeling = game.getTransitionMatrix().toLabeledMatrix(
        game.getRowVariables(), game.getColumnVariables(), game.getNondeterminismVariables(), odd, odd, labelingVariableSets);
    auto& transitionMatrix = matrixAndLabeling.matrix;
    auto& player1Labeling = matrixAndLabeling.labelings.front();
    auto& player2Labeling = matrixAndLabeling.labelings.back();

    // Create the player 2 row grouping from the labeling.
    std::vector<uint64_t> tmpPlayer2RowGrouping;
    for (uint64_t player1State = 0; player1State < transitionMatrix.getRowGroupCount(); ++player1State) {
        uint64_t lastLabel = std::numeric_limits<uint64_t>::max();
        for (uint64_t row = transitionMatrix.getRowGroupIndices()[player1State]; row < transitionMatrix.getRowGroupIndices()[player1State + 1]; ++row) {
            if (player1Labeling[row] != lastLabel) {
                tmpPlayer2RowGrouping.emplace_back(row);
                lastLabel = player1Labeling[row];
            }
        }
    }
    tmpPlayer2RowGrouping.emplace_back(player1Labeling.size());

    std::vector<uint64_t> player1RowGrouping = transitionMatrix.swapRowGroupIndices(std::move(tmpPlayer2RowGrouping));
    auto const& player2RowGrouping = transitionMatrix.getRowGroupIndices();

    // Create the player 1 groups and backward transitions (for both players).
    std::vector<uint64_t> player1Groups(player1RowGrouping.size());
    storm::storage::SparseMatrix<ValueType> player1BackwardTransitions = transitionMatrix.transpose(true);
    std::vector<uint64_t> player2BackwardTransitions(transitionMatrix.getRowGroupCount());

    uint64_t player2State = 0;
    for (uint64_t player1State = 0; player1State < player1RowGrouping.size() - 1; ++player1State) {
        while (player1RowGrouping[player1State + 1] > player2RowGrouping[player2State]) {
            player2BackwardTransitions[player2State] = player1State;
            ++player2State;
        }

        player1Groups[player1State + 1] = player2State;
    }

    // Lift the player 1 labeling from rows to row groups (player 2 states).
    for (uint64_t player1State = 0; player1State < player1Groups.size() - 1; ++player1State) {
        for (uint64_t player2State = player1Groups[player1State]; player2State < player1Groups[player1State + 1]; ++player2State) {
            player1Labeling[player2State] = player1Labeling[player2RowGrouping[player2State]];
        }
    }
    player1Labeling.resize(player2RowGrouping.size() - 1);

    // Create explicit representations of important state sets.
    storm::storage::BitVector initialStates = initialStatesBdd.toVector(odd);
    storm::storage::BitVector constraintStates = constraintStatesBdd.toVector(odd);
    storm::storage::BitVector targetStates = targetStatesBdd.toVector(odd);
    translationWatch.stop();
    totalTranslationWatch.add(translationWatch);
    STORM_LOG_INFO("Translation to explicit representation completed in " << translationWatch.getTimeInMilliseconds() << "ms.");

    // Prepare the two strategies.
    storage::ExplicitGameStrategyPair minStrategyPair(initialStates.size(), transitionMatrix.getRowGroupCount());
    storage::ExplicitGameStrategyPair maxStrategyPair(initialStates.size(), transitionMatrix.getRowGroupCount());

    // (1) compute all states with probability 0/1 wrt. to the two different player 2 goals (min/max).
    storm::utility::Stopwatch qualitativeWatch(true);
    ExplicitQualitativeGameResultMinMax qualitativeResult =
        computeProb01States(previousResult, odd, player1Direction, transitionMatrix, player1Groups, player1BackwardTransitions, player2BackwardTransitions,
                            constraintStates, targetStates, minStrategyPair, maxStrategyPair);
    qualitativeWatch.stop();
    totalSolutionWatch.add(qualitativeWatch);
    STORM_LOG_INFO("Qualitative computation completed in " << qualitativeWatch.getTimeInMilliseconds() << "ms.");

    std::unique_ptr<storm::modelchecker::CheckResult> result = checkForResultAfterQualitativeCheck<ValueType>(checkTask, initialStates, qualitativeResult);
    if (result) {
        return result;
    }

    // (2) compute the states for which we have to determine quantitative information.
    storm::storage::BitVector maybeMin = ~(qualitativeResult.getProb0Min().getStates() | qualitativeResult.getProb1Min().getStates());
    storm::storage::BitVector maybeMax = ~(qualitativeResult.getProb0Max().getStates() | qualitativeResult.getProb1Max().getStates());

    // (3) if the initial states are not maybe states, then we can refine at this point.
    storm::storage::BitVector initialMaybeStates = initialStates & (maybeMin | maybeMax);
    bool qualitativeRefinement = false;
    if (initialMaybeStates.empty()) {
        // In this case, we know the result for the initial states for both player 2 minimizing and maximizing.
        STORM_LOG_TRACE("No initial state is a 'maybe' state.");

        STORM_LOG_INFO("Obtained qualitative bounds [0, 1] on the actual value for the initial states (after "
                       << totalWatch.getTimeInMilliseconds() << "ms in iteration " << this->iteration << "). Refining abstraction based on qualitative check.");

        // Post-process strategies for better refinements.
        storm::utility::Stopwatch strategyProcessingWatch(true);
        postProcessStrategies(player1Direction, minStrategyPair, maxStrategyPair, player1Groups, player2RowGrouping, transitionMatrix, constraintStates,
                              targetStates, qualitativeResult, this->fixPlayer1Strategy, this->fixPlayer2Strategy, this->debug);
        strategyProcessingWatch.stop();
        totalStrategyProcessingWatch.add(strategyProcessingWatch);
        STORM_LOG_DEBUG("Postprocessed strategies in " << strategyProcessingWatch.getTimeInMilliseconds() << "ms.");

        // If we get here, the initial states were all identified as prob0/1 states, but the value (0 or 1)
        // depends on whether player 2 is minimizing or maximizing. Therefore, we need to find a place to refine.
        storm::utility::Stopwatch refinementWatch(true);
        qualitativeRefinement = refiner.refine(game, odd, transitionMatrix, player1Groups, player1Labeling, player2Labeling, initialStates, constraintStates,
                                               targetStates, qualitativeResult, minStrategyPair, maxStrategyPair);
        refinementWatch.stop();
        totalRefinementWatch.add(refinementWatch);
        STORM_LOG_INFO("Qualitative refinement completed in " << refinementWatch.getTimeInMilliseconds() << "ms.");
    }

    ExplicitQuantitativeResultMinMax<ValueType> quantitativeResult;

    // (4) if we arrived at this point and no refinement was made, we need to compute the quantitative solution.
    if (!qualitativeRefinement) {
        // At this point, we know that we cannot answer the query without further numeric computation.
        STORM_LOG_TRACE("Starting numerical solution step.");

        // (7) Solve the min values and check whether we can give the answer already.
        storm::utility::Stopwatch quantitativeWatch(true);
        quantitativeResult.setMin(computeQuantitativeResult<ValueType>(env, player1Direction, storm::OptimizationDirection::Minimize, transitionMatrix,
                                                                       player1Groups, qualitativeResult, maybeMin, minStrategyPair, odd, nullptr, nullptr,
                                                                       this->reuseQuantitativeResults ? previousResult : boost::none));

        // Dispose of previous result as we now reused it.
        if (previousResult) {
            previousResult.get().clear();
        }
        quantitativeWatch.stop();
        result = checkForResultAfterQuantitativeCheck<ValueType>(checkTask, storm::OptimizationDirection::Minimize,
                                                                 quantitativeResult.getMin().getRange(initialStates));
        if (result) {
            totalSolutionWatch.add(quantitativeWatch);
            return result;
        }

        // (8) Solve the max values and check whether we can give the answer already.
        quantitativeWatch.start();
        quantitativeResult.setMax(computeQuantitativeResult(env, player1Direction, storm::OptimizationDirection::Maximize, transitionMatrix, player1Groups,
                                                            qualitativeResult, maybeMax, maxStrategyPair, odd, &quantitativeResult.getMin(), &minStrategyPair));
        quantitativeWatch.stop();
        result = checkForResultAfterQuantitativeCheck<ValueType>(checkTask, storm::OptimizationDirection::Maximize,
                                                                 quantitativeResult.getMax().getRange(initialStates));
        totalSolutionWatch.add(quantitativeWatch);
        if (result) {
            return result;
        }

        ValueType minVal = quantitativeResult.getMin().getRange(initialStates).first;
        ValueType maxVal = quantitativeResult.getMax().getRange(initialStates).second;
        ValueType difference = maxVal - minVal;
        if (std::is_same<ValueType, double>::value) {
            std::stringstream differenceStream;
            differenceStream.setf(std::ios::fixed, std::ios::floatfield);
            differenceStream.precision(15);
            differenceStream << difference;
            STORM_LOG_INFO("Obtained quantitative bounds [" << minVal << ", " << maxVal << "] (difference " << differenceStream.str()
                                                            << ") on the actual value for the initial states in " << quantitativeWatch.getTimeInMilliseconds()
                                                            << "ms (after " << totalWatch.getTimeInMilliseconds() << "ms in iteration " << this->iteration
                                                            << ").");
        } else {
            STORM_LOG_INFO("Obtained quantitative bounds [" << minVal << ", " << maxVal << "] (approx. [" << storm::utility::convertNumber<double>(minVal)
                                                            << ", " << storm::utility::convertNumber<double>(maxVal) << "], difference " << difference
                                                            << ") on the actual value for the initial states in " << quantitativeWatch.getTimeInMilliseconds()
                                                            << "ms (after " << totalWatch.getTimeInMilliseconds() << "ms in iteration " << this->iteration
                                                            << ").");
        }

        // (9) Check whether the lower and upper bounds are close enough to terminate with an answer.
        result = checkForResultAfterQuantitativeCheck<ValueType>(quantitativeResult.getMin().getRange(initialStates).first,
                                                                 quantitativeResult.getMax().getRange(initialStates).second, comparator);
        if (result) {
            return result;
        }

        // Post-process strategies for better refinements.
        storm::utility::Stopwatch strategyProcessingWatch(true);
        postProcessStrategies(this->iteration, player1Direction, minStrategyPair, maxStrategyPair, player1Groups, player2RowGrouping, transitionMatrix,
                              initialStates, constraintStates, targetStates, quantitativeResult, this->fixPlayer1Strategy, this->fixPlayer2Strategy,
                              this->debug);
        strategyProcessingWatch.stop();
        totalStrategyProcessingWatch.add(strategyProcessingWatch);
        STORM_LOG_DEBUG("Postprocessed strategies in " << strategyProcessingWatch.getTimeInMilliseconds() << "ms.");

        // Make sure that all strategies are still valid strategies.
        STORM_LOG_ASSERT(minStrategyPair.getNumberOfUndefinedPlayer1States() <= targetStates.getNumberOfSetBits(),
                         "Expected at most " << targetStates.getNumberOfSetBits() << " (number of target states) player 1 states with undefined choice but got "
                                             << minStrategyPair.getNumberOfUndefinedPlayer1States() << ".");
        STORM_LOG_ASSERT(maxStrategyPair.getNumberOfUndefinedPlayer1States() <= targetStates.getNumberOfSetBits(),
                         "Expected at most " << targetStates.getNumberOfSetBits() << " (number of target states) player 1 states with undefined choice but got "
                                             << maxStrategyPair.getNumberOfUndefinedPlayer1States() << ".");

        // (10) If we arrived at this point, it means that we have all qualitative and quantitative
        // information about the game, but we could not yet answer the query. In this case, we need to refine.
        storm::utility::Stopwatch refinementWatch(true);
        refiner.refine(game, odd, transitionMatrix, player1Groups, player1Labeling, player2Labeling, initialStates, constraintStates, targetStates,
                       quantitativeResult, minStrategyPair, maxStrategyPair);
        refinementWatch.stop();
        totalRefinementWatch.add(refinementWatch);
        STORM_LOG_INFO("Quantitative refinement completed in " << refinementWatch.getTimeInMilliseconds() << "ms.");

        if (this->reuseQuantitativeResults) {
            PreviousExplicitResult<ValueType> nextPreviousResult;
            nextPreviousResult.values = std::move(quantitativeResult.getMin());
            nextPreviousResult.odd = odd;
            previousResult = std::move(nextPreviousResult);
            STORM_LOG_TRACE("Prepared next previous result to reuse values.");
        }
    }

    return nullptr;
}

template<storm::dd::DdType Type, typename ModelType>
std::vector<storm::expressions::Expression> GameBasedMdpModelChecker<Type, ModelType>::getInitialPredicates(
    storm::expressions::Expression const& constraintExpression, storm::expressions::Expression const& targetStateExpression) {
    std::vector<storm::expressions::Expression> initialPredicates;
    if (preprocessedModel.isJaniModel()) {
        storm::expressions::VariableSetPredicateSplitter splitter(preprocessedModel.asJaniModel().getAllLocationExpressionVariables());

        std::vector<storm::expressions::Expression> splitExpressions = splitter.split(targetStateExpression);
        initialPredicates.insert(initialPredicates.end(), splitExpressions.begin(), splitExpressions.end());

        splitExpressions = splitter.split(constraintExpression);
        initialPredicates.insert(initialPredicates.end(), splitExpressions.begin(), splitExpressions.end());
    } else {
        if (!targetStateExpression.isTrue() && !targetStateExpression.isFalse()) {
            initialPredicates.push_back(targetStateExpression);
        }
        if (!constraintExpression.isTrue() && !constraintExpression.isFalse()) {
            initialPredicates.push_back(constraintExpression);
        }
    }
    return initialPredicates;
}

template<storm::dd::DdType Type, typename ModelType>
storm::OptimizationDirection GameBasedMdpModelChecker<Type, ModelType>::getPlayer1Direction(
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& checkTask) {
    if (preprocessedModel.getModelType() == storm::storage::SymbolicModelDescription::ModelType::DTMC) {
        return storm::OptimizationDirection::Maximize;
    } else if (checkTask.isOptimizationDirectionSet()) {
        return checkTask.getOptimizationDirection();
    } else if (checkTask.isBoundSet() && preprocessedModel.getModelType() != storm::storage::SymbolicModelDescription::ModelType::DTMC) {
        return storm::logic::isLowerBound(checkTask.getBoundComparisonType()) ? storm::OptimizationDirection::Minimize : storm::OptimizationDirection::Maximize;
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidPropertyException, "Could not derive player 1 optimization direction.");
    return storm::OptimizationDirection::Maximize;
}

template<storm::dd::DdType Type>
bool checkQualitativeStrategies(bool prob0, SymbolicQualitativeGameResult<Type> const& result, storm::dd::Bdd<Type> const& targetStates) {
    if (prob0) {
        STORM_LOG_ASSERT(result.hasPlayer1Strategy() && (result.getPlayer1States().isZero() || !result.getPlayer1Strategy().isZero()),
                         "Unable to proceed without strategy.");
    } else {
        STORM_LOG_ASSERT(result.hasPlayer1Strategy() && ((result.getPlayer1States() && !targetStates).isZero() || !result.getPlayer1Strategy().isZero()),
                         "Unable to proceed without strategy.");
    }

    STORM_LOG_ASSERT(result.hasPlayer2Strategy() && (result.getPlayer2States().isZero() || !result.getPlayer2Strategy().isZero()),
                     "Unable to proceed without strategy.");

    return true;
}

template<storm::dd::DdType Type>
bool checkQualitativeStrategies(SymbolicQualitativeGameResultMinMax<Type> const& qualitativeResult, storm::dd::Bdd<Type> const& targetStates) {
    bool result = true;
    result &= checkQualitativeStrategies(true, qualitativeResult.prob0Min, targetStates);
    result &= checkQualitativeStrategies(false, qualitativeResult.prob1Min, targetStates);
    result &= checkQualitativeStrategies(true, qualitativeResult.prob0Max, targetStates);
    result &= checkQualitativeStrategies(false, qualitativeResult.prob1Max, targetStates);
    return result;
}

template<storm::dd::DdType Type, typename ModelType>
ExplicitQualitativeGameResultMinMax GameBasedMdpModelChecker<Type, ModelType>::computeProb01States(
    boost::optional<PreviousExplicitResult<ValueType>> const& previousResult, storm::dd::Odd const& odd, storm::OptimizationDirection player1Direction,
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<uint64_t> const& player1Groups,
    storm::storage::SparseMatrix<ValueType> const& player1BackwardTransitions, std::vector<uint64_t> const& player2BackwardTransitions,
    storm::storage::BitVector const& constraintStates, storm::storage::BitVector const& targetStates, storage::ExplicitGameStrategyPair& minStrategyPair,
    storage::ExplicitGameStrategyPair& maxStrategyPair) {
    ExplicitQualitativeGameResultMinMax result;

    //            ExplicitQualitativeGameResult problematicStates = storm::utility::graph::performProb0(transitionMatrix, player1Groups,
    //            player1BackwardTransitions, player2BackwardTransitions, constraintStates, targetStates, storm::OptimizationDirection::Minimize,
    //            storm::OptimizationDirection::Minimize);

    result.prob0Min =
        storm::utility::graph::performProb0(transitionMatrix, player1Groups, player1BackwardTransitions, player2BackwardTransitions, constraintStates,
                                            targetStates, player1Direction, storm::OptimizationDirection::Minimize, &minStrategyPair);
    result.prob1Min =
        storm::utility::graph::performProb1(transitionMatrix, player1Groups, player1BackwardTransitions, player2BackwardTransitions, constraintStates,
                                            targetStates, player1Direction, storm::OptimizationDirection::Minimize, &minStrategyPair);
    result.prob0Max =
        storm::utility::graph::performProb0(transitionMatrix, player1Groups, player1BackwardTransitions, player2BackwardTransitions, constraintStates,
                                            targetStates, player1Direction, storm::OptimizationDirection::Maximize, &maxStrategyPair);
    result.prob1Max =
        storm::utility::graph::performProb1(transitionMatrix, player1Groups, player1BackwardTransitions, player2BackwardTransitions, constraintStates,
                                            targetStates, player1Direction, storm::OptimizationDirection::Maximize, &maxStrategyPair);

    STORM_LOG_INFO("[" << player1Direction << ", " << storm::OptimizationDirection::Minimize << "]: " << result.prob0Min.player1States.getNumberOfSetBits()
                       << " 'no', " << result.prob1Min.player1States.getNumberOfSetBits() << " 'yes'.");
    STORM_LOG_INFO("[" << player1Direction << ", " << storm::OptimizationDirection::Maximize << "]: " << result.prob0Max.player1States.getNumberOfSetBits()
                       << " 'no', " << result.prob1Max.player1States.getNumberOfSetBits() << " 'yes'.");

    return result;
}

template<storm::dd::DdType Type, typename ModelType>
SymbolicQualitativeGameResultMinMax<Type> GameBasedMdpModelChecker<Type, ModelType>::computeProb01States(
    boost::optional<SymbolicQualitativeGameResultMinMax<Type>> const& previousQualitativeResult,
    storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, storm::OptimizationDirection player1Direction,
    storm::dd::Bdd<Type> const& transitionMatrixBdd, storm::dd::Bdd<Type> const& constraintStates, storm::dd::Bdd<Type> const& targetStates) {
    SymbolicQualitativeGameResultMinMax<Type> result;

    if (reuseQualitativeResults) {
        // Depending on the player 1 direction, we choose a different order of operations.
        if (player1Direction == storm::OptimizationDirection::Minimize) {
            // (1) min/min: compute prob0 using the game functions
            result.prob0Min = storm::utility::graph::performProb0(game, transitionMatrixBdd, constraintStates, targetStates, player1Direction,
                                                                  storm::OptimizationDirection::Minimize, true, true);

            // (2) min/min: compute prob1 using the MDP functions
            storm::dd::Bdd<Type> candidates = game.getReachableStates() && !result.prob0Min.player1States;
            storm::dd::Bdd<Type> prob1MinMinMdp = storm::utility::graph::performProb1A(
                game, transitionMatrixBdd, previousQualitativeResult ? previousQualitativeResult.get().prob1Min.player1States : targetStates, candidates);

            // (3) min/min: compute prob1 using the game functions
            result.prob1Min = storm::utility::graph::performProb1(game, transitionMatrixBdd, constraintStates, targetStates, player1Direction,
                                                                  storm::OptimizationDirection::Minimize, true, true, boost::make_optional(prob1MinMinMdp));

            // (4) min/max: compute prob 0 using the game functions
            result.prob0Max = storm::utility::graph::performProb0(game, transitionMatrixBdd, constraintStates, targetStates, player1Direction,
                                                                  storm::OptimizationDirection::Maximize, true, true);

            // (5) min/max: compute prob 1 using the game functions
            // We know that only previous prob1 states can now be prob 1 states again, because the upper bound
            // values can only decrease over iterations.
            boost::optional<storm::dd::Bdd<Type>> prob1Candidates;
            if (previousQualitativeResult) {
                prob1Candidates = previousQualitativeResult.get().prob1Max.player1States;
            }
            result.prob1Max = storm::utility::graph::performProb1(game, transitionMatrixBdd, constraintStates, targetStates, player1Direction,
                                                                  storm::OptimizationDirection::Maximize, true, true, prob1Candidates);
        } else {
            // (1) max/max: compute prob0 using the game functions
            result.prob0Max = storm::utility::graph::performProb0(game, transitionMatrixBdd, constraintStates, targetStates, player1Direction,
                                                                  storm::OptimizationDirection::Maximize, true, true);

            // (2) max/max: compute prob1 using the MDP functions, reuse prob1 states of last iteration to constrain the candidate states.
            storm::dd::Bdd<Type> candidates = game.getReachableStates() && !result.prob0Max.player1States;
            if (previousQualitativeResult) {
                candidates &= previousQualitativeResult.get().prob1Max.player1States;
            }
            storm::dd::Bdd<Type> prob1MaxMaxMdp = storm::utility::graph::performProb1E(game, transitionMatrixBdd, constraintStates, targetStates, candidates);

            // (3) max/max: compute prob1 using the game functions, reuse prob1 states from the MDP precomputation
            result.prob1Max = storm::utility::graph::performProb1(game, transitionMatrixBdd, constraintStates, targetStates, player1Direction,
                                                                  storm::OptimizationDirection::Maximize, true, true, boost::make_optional(prob1MaxMaxMdp));

            // (4) max/min: compute prob0 using the game functions
            result.prob0Min = storm::utility::graph::performProb0(game, transitionMatrixBdd, constraintStates, targetStates, player1Direction,
                                                                  storm::OptimizationDirection::Minimize, true, true);

            // (5) max/min: compute prob1 using the game functions, use prob1 from max/max as the candidate set
            result.prob1Min = storm::utility::graph::performProb1(game, transitionMatrixBdd, constraintStates, targetStates, player1Direction,
                                                                  storm::OptimizationDirection::Minimize, true, true, boost::make_optional(prob1MaxMaxMdp));
        }
    } else {
        result.prob0Min = storm::utility::graph::performProb0(game, transitionMatrixBdd, constraintStates, targetStates, player1Direction,
                                                              storm::OptimizationDirection::Minimize, true, true);
        result.prob1Min = storm::utility::graph::performProb1(game, transitionMatrixBdd, constraintStates, targetStates, player1Direction,
                                                              storm::OptimizationDirection::Minimize, true, true);
        result.prob0Max = storm::utility::graph::performProb0(game, transitionMatrixBdd, constraintStates, targetStates, player1Direction,
                                                              storm::OptimizationDirection::Maximize, true, true);
        result.prob1Max = storm::utility::graph::performProb1(game, transitionMatrixBdd, constraintStates, targetStates, player1Direction,
                                                              storm::OptimizationDirection::Maximize, true, true);
    }

    STORM_LOG_INFO("[" << player1Direction << ", " << storm::OptimizationDirection::Minimize << "]: " << result.prob0Min.player1States.getNonZeroCount()
                       << " 'no', " << result.prob1Min.player1States.getNonZeroCount() << " 'yes'.");
    STORM_LOG_INFO("[" << player1Direction << ", " << storm::OptimizationDirection::Maximize << "]: " << result.prob0Max.player1States.getNonZeroCount()
                       << " 'no', " << result.prob1Max.player1States.getNonZeroCount() << " 'yes'.");

    STORM_LOG_ASSERT(checkQualitativeStrategies(result, targetStates), "Qualitative strategies appear to be broken.");
    return result;
}

template<storm::dd::DdType Type, typename ModelType>
void GameBasedMdpModelChecker<Type, ModelType>::printStatistics(storm::gbar::abstraction::MenuGameAbstractor<Type, ValueType> const& abstractor,
                                                                storm::gbar::abstraction::MenuGame<Type, ValueType> const& game, uint64_t refinements,
                                                                uint64_t peakPlayer1States, uint64_t peakTransitions) const {
    storm::gbar::abstraction::AbstractionInformation<Type> const& abstractionInformation = abstractor.getAbstractionInformation();

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    oss << '\n';
    oss << "Statistics:\n";
    oss << "    * size of final game: " << game.getReachableStates().getNonZeroCount() << " player 1 states, " << game.getTransitionMatrix().getNonZeroCount()
        << " transitions\n";
    oss << "    * peak size of game: " << peakPlayer1States << " player 1 states, " << peakTransitions << " transitions\n";
    oss << "    * refinements: " << refinements << '\n';
    oss << "    * predicates: " << abstractionInformation.getNumberOfPredicates() << "\n\n";

    uint64_t totalAbstractionTimeMillis = totalAbstractionWatch.getTimeInMilliseconds();
    uint64_t totalTranslationTimeMillis = totalTranslationWatch.getTimeInMilliseconds();
    uint64_t totalStrategyProcessingTimeMillis = totalStrategyProcessingWatch.getTimeInMilliseconds();
    uint64_t totalSolutionTimeMillis = totalSolutionWatch.getTimeInMilliseconds();
    uint64_t totalRefinementTimeMillis = totalRefinementWatch.getTimeInMilliseconds();
    uint64_t setupTime = setupWatch.getTimeInMilliseconds();
    uint64_t totalTimeMillis = totalWatch.getTimeInMilliseconds();

    oss << "Time breakdown:\n";
    oss << "    * setup: " << setupTime << "ms (" << 100 * static_cast<double>(setupTime) / totalTimeMillis << "%)\n";
    oss << "    * abstraction: " << totalAbstractionTimeMillis << "ms (" << 100 * static_cast<double>(totalAbstractionTimeMillis) / totalTimeMillis << "%)\n";
    if (this->solveMode == storm::settings::modules::AbstractionSettings::SolveMode::Sparse) {
        oss << "    * translation: " << totalTranslationTimeMillis << "ms (" << 100 * static_cast<double>(totalTranslationTimeMillis) / totalTimeMillis
            << "%)\n";
        if (fixPlayer1Strategy || fixPlayer2Strategy) {
            oss << "    * strategy processing: " << totalStrategyProcessingTimeMillis << "ms ("
                << 100 * static_cast<double>(totalStrategyProcessingTimeMillis) / totalTimeMillis << "%)\n";
        }
    }
    oss << "    * solution: " << totalSolutionTimeMillis << "ms (" << 100 * static_cast<double>(totalSolutionTimeMillis) / totalTimeMillis << "%)\n";
    oss << "    * refinement: " << totalRefinementTimeMillis << "ms (" << 100 * static_cast<double>(totalRefinementTimeMillis) / totalTimeMillis << "%)\n";
    oss << "    ---------------------------------------------\n";
    oss << "    * total: " << totalTimeMillis << "ms\n\n";

    STORM_LOG_STATISTICS(oss.str());
}

template<storm::dd::DdType Type, typename ModelType>
storm::expressions::Expression GameBasedMdpModelChecker<Type, ModelType>::getExpression(storm::logic::Formula const& formula) {
    STORM_LOG_THROW(formula.isBooleanLiteralFormula() || formula.isAtomicExpressionFormula() || formula.isAtomicLabelFormula(),
                    storm::exceptions::InvalidPropertyException, "The target states have to be given as label or an expression.");
    storm::expressions::Expression result;
    if (formula.isAtomicLabelFormula()) {
        result = preprocessedModel.asPrismProgram().getLabelExpression(formula.asAtomicLabelFormula().getLabel());
    } else if (formula.isAtomicExpressionFormula()) {
        result = formula.asAtomicExpressionFormula().getExpression();
    } else {
        result =
            formula.asBooleanLiteralFormula().isTrueFormula() ? preprocessedModel.getManager().boolean(true) : preprocessedModel.getManager().boolean(false);
    }
    return result;
}

template class GameBasedMdpModelChecker<storm::dd::DdType::CUDD, storm::models::symbolic::Dtmc<storm::dd::DdType::CUDD, double>>;
template class GameBasedMdpModelChecker<storm::dd::DdType::CUDD, storm::models::symbolic::Mdp<storm::dd::DdType::CUDD, double>>;
template class GameBasedMdpModelChecker<storm::dd::DdType::Sylvan, storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, double>>;
template class GameBasedMdpModelChecker<storm::dd::DdType::Sylvan, storm::models::symbolic::Mdp<storm::dd::DdType::Sylvan, double>>;

template class GameBasedMdpModelChecker<storm::dd::DdType::Sylvan, storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class GameBasedMdpModelChecker<storm::dd::DdType::Sylvan, storm::models::symbolic::Mdp<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
}  // namespace modelchecker
}  // namespace storm::gbar
