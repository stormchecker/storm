#include "storm/modelchecker/prctl/helper/SparseMdpPrctlHelper.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UncheckedRequirementException.h"
#include "storm/io/export.h"
#include "storm/modelchecker/helper/DiscountingHelper.h"
#include "storm/modelchecker/hints/ExplicitModelCheckerHint.h"
#include "storm/modelchecker/prctl/helper/BaierUpperRewardBoundsComputer.h"
#include "storm/modelchecker/prctl/helper/DsMpiUpperRewardBoundsComputer.h"
#include "storm/modelchecker/prctl/helper/SemanticSolutionType.h"
#include "storm/modelchecker/prctl/helper/SparseMdpEndComponentInformation.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/ModelCheckerSettings.h"
#include "storm/solver/LpSolver.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/solver/multiplier/Multiplier.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/Scheduler.h"
#include "storm/transformer/EndComponentEliminator.h"
#include "storm/utility/ProgressMeasurement.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType, typename SolutionType>
std::map<storm::storage::sparse::state_type, SolutionType> SparseMdpPrctlHelper<ValueType, SolutionType>::computeRewardBoundedValues(
    Environment const& env, OptimizationDirection dir, rewardbounded::MultiDimensionalRewardUnfolding<ValueType, true>& rewardUnfolding,
    storm::storage::BitVector const& initialStates) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support computing reward bounded values with interval models.");
    } else {
        storm::utility::Stopwatch swAll(true), swBuild, swCheck;

        // Get lower and upper bounds for the solution.
        auto lowerBound = rewardUnfolding.getLowerObjectiveBound();
        auto upperBound = rewardUnfolding.getUpperObjectiveBound();

        // Initialize epoch models
        auto initEpoch = rewardUnfolding.getStartEpoch();
        auto epochOrder = rewardUnfolding.getEpochComputationOrder(initEpoch);

        // initialize data that will be needed for each epoch
        std::vector<ValueType> x, b;
        std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> minMaxSolver;

        ValueType precision = rewardUnfolding.getRequiredEpochModelPrecision(
            initEpoch, storm::utility::convertNumber<ValueType>(storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision()));
        Environment preciseEnv = env;
        preciseEnv.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(precision));

        // In case of cdf export we store the necessary data.
        std::vector<std::vector<ValueType>> cdfData;

        storm::utility::ProgressMeasurement progress("epochs");
        progress.setMaxCount(epochOrder.size());
        progress.startNewMeasurement(0);
        uint64_t numCheckedEpochs = 0;
        for (auto const& epoch : epochOrder) {
            swBuild.start();
            auto& epochModel = rewardUnfolding.setCurrentEpoch(epoch);
            swBuild.stop();
            swCheck.start();
            rewardUnfolding.setSolutionForCurrentEpoch(epochModel.analyzeSingleObjective(preciseEnv, dir, x, b, minMaxSolver, lowerBound, upperBound));
            swCheck.stop();
            if (storm::settings::getModule<storm::settings::modules::IOSettings>().isExportCdfSet() &&
                !rewardUnfolding.getEpochManager().hasBottomDimension(epoch)) {
                std::vector<ValueType> cdfEntry;
                for (uint64_t i = 0; i < rewardUnfolding.getEpochManager().getDimensionCount(); ++i) {
                    uint64_t offset = rewardUnfolding.getDimension(i).boundType == helper::rewardbounded::DimensionBoundType::LowerBound ? 1 : 0;
                    cdfEntry.push_back(storm::utility::convertNumber<ValueType>(rewardUnfolding.getEpochManager().getDimensionOfEpoch(epoch, i) + offset) *
                                       rewardUnfolding.getDimension(i).scalingFactor);
                }
                cdfEntry.push_back(rewardUnfolding.getInitialStateResult(epoch));
                cdfData.push_back(std::move(cdfEntry));
            }
            ++numCheckedEpochs;
            progress.updateProgress(numCheckedEpochs);
            if (storm::utility::resources::isTerminate()) {
                break;
            }
        }

        std::map<storm::storage::sparse::state_type, ValueType> result;
        for (auto initState : initialStates) {
            result[initState] = rewardUnfolding.getInitialStateResult(initEpoch, initState);
        }

        swAll.stop();

        if (storm::settings::getModule<storm::settings::modules::IOSettings>().isExportCdfSet()) {
            std::vector<std::string> headers;
            for (uint64_t i = 0; i < rewardUnfolding.getEpochManager().getDimensionCount(); ++i) {
                headers.push_back(rewardUnfolding.getDimension(i).formula->toString());
            }
            headers.push_back("Result");
            storm::io::exportDataToCSVFile<ValueType, std::string, std::string>(
                storm::settings::getModule<storm::settings::modules::IOSettings>().getExportCdfDirectory() + "cdf.csv", cdfData, headers);
        }

        if (storm::settings::getModule<storm::settings::modules::CoreSettings>().isShowStatisticsSet()) {
            STORM_PRINT_AND_LOG("---------------------------------\n");
            STORM_PRINT_AND_LOG("Statistics:\n");
            STORM_PRINT_AND_LOG("---------------------------------\n");
            STORM_PRINT_AND_LOG("          #checked epochs: " << epochOrder.size() << ".\n");
            STORM_PRINT_AND_LOG("             overall Time: " << swAll << ".\n");
            STORM_PRINT_AND_LOG("Epoch Model building Time: " << swBuild << ".\n");
            STORM_PRINT_AND_LOG("Epoch Model checking Time: " << swCheck << ".\n");
            STORM_PRINT_AND_LOG("---------------------------------\n");
        }

        return result;
    }
}

template<typename ValueType, typename SolutionType>
std::vector<SolutionType> SparseMdpPrctlHelper<ValueType, SolutionType>::computeNextProbabilities(
    Environment const& env, OptimizationDirection dir, UncertaintyResolutionMode uncertaintyResolutionMode,
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& nextStates) {
    // Create the vector with which to multiply and initialize it correctly.
    std::vector<SolutionType> result(transitionMatrix.getRowGroupCount());
    storm::utility::vector::setVectorValues(result, nextStates, storm::utility::one<SolutionType>());

    auto multiplier = storm::solver::MultiplierFactory<ValueType, SolutionType>().create(env, transitionMatrix);
    multiplier->multiplyAndReduce(env, dir, result, nullptr, result, uncertaintyResolutionMode);

    return result;
}

template<typename ValueType, typename SolutionType = ValueType>
std::vector<uint_fast64_t> computeValidSchedulerHint(Environment const& env, SemanticSolutionType const& type,
                                                     storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                     storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                     storm::storage::BitVector const& maybeStates, storm::storage::BitVector const& filterStates,
                                                     storm::storage::BitVector const& targetStates,
                                                     boost::optional<storm::storage::BitVector> const& selectedChoices) {
    storm::storage::Scheduler<SolutionType> validScheduler(maybeStates.size());

    if (type == SemanticSolutionType::UntilProbabilities) {
        storm::utility::graph::computeSchedulerProbGreater0E(transitionMatrix, backwardTransitions, filterStates, targetStates, validScheduler,
                                                             selectedChoices);
    } else if (type == SemanticSolutionType::ExpectedRewards) {
        storm::utility::graph::computeSchedulerProb1E(maybeStates | targetStates, transitionMatrix, backwardTransitions, filterStates, targetStates,
                                                      validScheduler, selectedChoices);
    } else {
        STORM_LOG_ASSERT(false, "Unexpected equation system type.");
    }

    // Extract the relevant parts of the scheduler for the solver.
    std::vector<uint64_t> schedulerHint;
    schedulerHint.reserve(maybeStates.getNumberOfSetBits());
    if (selectedChoices) {
        // There might be unselected choices so the local choice indices from the scheduler need to be adapted
        for (auto maybeState : maybeStates) {
            auto choice = validScheduler.getChoice(maybeState).getDeterministicChoice();
            auto const groupStart = transitionMatrix.getRowGroupIndices()[maybeState];
            auto const origGlobalChoiceIndex = groupStart + choice;
            STORM_LOG_ASSERT(selectedChoices->get(origGlobalChoiceIndex), "The computed scheduler selects an illegal choice.");
            // Count the number of unselected choices in [groupStart, origGlobalChoiceIndex) and subtract that from choice
            for (auto pos = selectedChoices->getNextUnsetIndex(groupStart); pos < origGlobalChoiceIndex; pos = selectedChoices->getNextUnsetIndex(pos + 1)) {
                --choice;
            }
            schedulerHint.push_back(choice);
        }
    } else {
        for (auto maybeState : maybeStates) {
            schedulerHint.push_back(validScheduler.getChoice(maybeState).getDeterministicChoice());
        }
    }
    return schedulerHint;
}

template<typename ValueType>
struct SparseMdpHintType {
    SparseMdpHintType() : eliminateEndComponents(false), computeUpperBounds(false), uniqueSolution(false), noEndComponents(false) {
        // Intentionally left empty.
    }

    bool hasSchedulerHint() const {
        return static_cast<bool>(schedulerHint);
    }

    bool hasValueHint() const {
        return static_cast<bool>(valueHint);
    }

    bool hasLowerResultBound() const {
        return static_cast<bool>(lowerResultBound);
    }

    ValueType const& getLowerResultBound() const {
        return lowerResultBound.get();
    }

    bool hasUpperResultBound() const {
        return static_cast<bool>(upperResultBound);
    }

    bool hasUpperResultBounds() const {
        return static_cast<bool>(upperResultBounds);
    }

    ValueType const& getUpperResultBound() const {
        return upperResultBound.get();
    }

    std::vector<ValueType>& getUpperResultBounds() {
        return upperResultBounds.get();
    }

    std::vector<ValueType> const& getUpperResultBounds() const {
        return upperResultBounds.get();
    }

    std::vector<uint64_t>& getSchedulerHint() {
        return schedulerHint.get();
    }

    std::vector<ValueType>& getValueHint() {
        return valueHint.get();
    }

    bool getEliminateEndComponents() const {
        return eliminateEndComponents;
    }

    bool getComputeUpperBounds() {
        return computeUpperBounds;
    }

    bool hasUniqueSolution() const {
        return uniqueSolution;
    }

    bool hasNoEndComponents() const {
        return noEndComponents;
    }

    boost::optional<std::vector<uint64_t>> schedulerHint;
    boost::optional<std::vector<ValueType>> valueHint;
    boost::optional<ValueType> lowerResultBound;
    boost::optional<ValueType> upperResultBound;
    boost::optional<std::vector<ValueType>> upperResultBounds;
    bool eliminateEndComponents;
    bool computeUpperBounds;
    bool uniqueSolution;
    bool noEndComponents;
};

template<typename ValueType, typename SolutionType>
void extractValueAndSchedulerHint(SparseMdpHintType<SolutionType>& hintStorage, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                  storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& maybeStates,
                                  boost::optional<storm::storage::BitVector> const& selectedChoices, ModelCheckerHint const& hint,
                                  bool skipECWithinMaybeStatesCheck) {
    // Deal with scheduler hint.
    if (hint.isExplicitModelCheckerHint() && hint.template asExplicitModelCheckerHint<ValueType>().hasSchedulerHint()) {
        if (hintStorage.hasSchedulerHint()) {
            STORM_LOG_WARN("A scheduler hint was provided, but the solver requires a specific one. The provided scheduler hint will be ignored.");
        } else {
            auto const& schedulerHint = hint.template asExplicitModelCheckerHint<ValueType>().getSchedulerHint();
            std::vector<uint64_t> hintChoices;

            // The scheduler hint is only applicable if it induces no BSCC consisting of maybe states.
            bool hintApplicable;
            if (!skipECWithinMaybeStatesCheck) {
                hintChoices.reserve(maybeStates.size());
                for (uint_fast64_t state = 0; state < maybeStates.size(); ++state) {
                    hintChoices.push_back(schedulerHint.getChoice(state).getDeterministicChoice());
                }
                hintApplicable =
                    storm::utility::graph::performProb1(transitionMatrix.transposeSelectedRowsFromRowGroups(hintChoices), maybeStates, ~maybeStates).full();
            } else {
                hintApplicable = true;
            }

            if (hintApplicable) {
                // Compute the hint w.r.t. the given subsystem.
                hintChoices.clear();
                hintChoices.reserve(maybeStates.getNumberOfSetBits());
                for (auto state : maybeStates) {
                    uint_fast64_t hintChoice = schedulerHint.getChoice(state).getDeterministicChoice();
                    if (selectedChoices) {
                        uint_fast64_t firstChoice = transitionMatrix.getRowGroupIndices()[state];
                        uint_fast64_t lastChoice = firstChoice + hintChoice;
                        hintChoice = 0;
                        for (uint_fast64_t choice = selectedChoices->getNextSetIndex(firstChoice); choice < lastChoice;
                             choice = selectedChoices->getNextSetIndex(choice + 1)) {
                            ++hintChoice;
                        }
                    }
                    hintChoices.push_back(hintChoice);
                }
                hintStorage.schedulerHint = std::move(hintChoices);
            }
        }
    }

    // Deal with solution value hint. Only applicable if there are no End Components consisting of maybe states.
    if (hint.isExplicitModelCheckerHint() && hint.template asExplicitModelCheckerHint<ValueType>().hasResultHint() &&
        (skipECWithinMaybeStatesCheck || hintStorage.hasSchedulerHint() ||
         storm::utility::graph::performProb1A(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, maybeStates, ~maybeStates)
             .full())) {
        hintStorage.valueHint = storm::utility::vector::filterVector(hint.template asExplicitModelCheckerHint<SolutionType>().getResultHint(), maybeStates);
    }
}

template<typename ValueType, typename SolutionType>
SparseMdpHintType<SolutionType> computeHints(Environment const& env, SemanticSolutionType const& type, ModelCheckerHint const& hint,
                                             storm::OptimizationDirection const& dir, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                             storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& maybeStates,
                                             storm::storage::BitVector const& phiStates, storm::storage::BitVector const& targetStates, bool produceScheduler,
                                             boost::optional<storm::storage::BitVector> const& selectedChoices = boost::none) {
    SparseMdpHintType<SolutionType> result;

    // There are no end components if we minimize until probabilities or
    // maximize reachability rewards or if the hint tells us so.
    result.noEndComponents = (dir == storm::solver::OptimizationDirection::Minimize && type == SemanticSolutionType::UntilProbabilities) ||
                             (dir == storm::solver::OptimizationDirection::Maximize && type == SemanticSolutionType::ExpectedRewards) ||
                             (hint.isExplicitModelCheckerHint() && hint.asExplicitModelCheckerHint<ValueType>().getNoEndComponentsInMaybeStates());

    // If there are no end components, the solution is unique. (Note that the other direction does not hold,
    // e.g., end components in which infinite reward is collected.
    result.uniqueSolution = result.hasNoEndComponents();

    // Check for requirements of the solver.
    bool hasSchedulerHint = hint.isExplicitModelCheckerHint() && hint.template asExplicitModelCheckerHint<ValueType>().hasSchedulerHint();
    storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType, SolutionType> minMaxLinearEquationSolverFactory;
    storm::solver::MinMaxLinearEquationSolverRequirements requirements =
        minMaxLinearEquationSolverFactory.getRequirements(env, result.uniqueSolution, result.noEndComponents, dir, hasSchedulerHint, produceScheduler);
    if (requirements.hasEnabledRequirement()) {
        // If the solver still requires no end-components, we have to eliminate them later.
        if (requirements.uniqueSolution()) {
            STORM_LOG_ASSERT(!result.hasUniqueSolution(),
                             "The solver requires to eliminate the end components although the solution is already assumed to be unique.");
            STORM_LOG_DEBUG("Scheduling EC elimination, because the solver requires a unique solution.");
            result.eliminateEndComponents = true;
            // If end components have been eliminated we can assume a unique solution.
            result.uniqueSolution = true;
            requirements.clearUniqueSolution();
            // If we compute until probabilities, we can even assume the absence of end components.
            // Note that in the case of minimizing expected rewards there might still be end components in which reward is collected.
            result.noEndComponents = (type == SemanticSolutionType::UntilProbabilities);
        }

        // If the solver requires an initial scheduler, compute one now. Note that any scheduler is valid if there are no end components.
        if (requirements.validInitialScheduler() && !result.noEndComponents) {
            STORM_LOG_DEBUG("Computing valid scheduler, because the solver requires it.");
            result.schedulerHint = computeValidSchedulerHint<ValueType, SolutionType>(env, type, transitionMatrix, backwardTransitions, maybeStates, phiStates,
                                                                                      targetStates, selectedChoices);
            requirements.clearValidInitialScheduler();
        }

        // Finally, we have information on the bounds depending on the problem type.
        if (type == SemanticSolutionType::UntilProbabilities) {
            requirements.clearBounds();
        } else if (type == SemanticSolutionType::ExpectedRewards) {
            requirements.clearLowerBounds();
        }
        if (requirements.upperBounds()) {
            result.computeUpperBounds = true;
            requirements.clearUpperBounds();
        }
        STORM_LOG_THROW(!requirements.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                        "Solver requirements " + requirements.getEnabledRequirementsAsString() + " not checked.");
    } else {
        STORM_LOG_DEBUG("Solver has no requirements.");
    }

    // Only if there is no end component decomposition that we will need to do later, we use value and scheduler
    // hints from the provided hint.
    if (!result.eliminateEndComponents) {
        extractValueAndSchedulerHint(result, transitionMatrix, backwardTransitions, maybeStates, selectedChoices, hint, result.uniqueSolution);
    } else {
        if (hint.isEmpty()) {
            STORM_LOG_TRACE("Warn A non-empty hint was provided, but its information will be disregarded.");
        }
    }

    // Only set bounds if we did not obtain them from the hint.
    if (!result.hasLowerResultBound()) {
        result.lowerResultBound = storm::utility::zero<SolutionType>();
    }
    if (!result.hasUpperResultBound() && type == SemanticSolutionType::UntilProbabilities) {
        result.upperResultBound = storm::utility::one<SolutionType>();
    }

    // If we received an upper bound, we can drop the requirement to compute one.
    if (result.hasUpperResultBound()) {
        result.computeUpperBounds = false;
    }

    return result;
}

template<typename ValueType>
struct MaybeStateResult {
    MaybeStateResult(std::vector<ValueType>&& values) : values(std::move(values)) {
        // Intentionally left empty.
    }

    bool hasScheduler() const {
        return static_cast<bool>(scheduler);
    }

    std::vector<uint64_t> const& getScheduler() const {
        return scheduler.get();
    }

    std::vector<ValueType> const& getValues() const {
        return values;
    }

    std::vector<ValueType> values;
    boost::optional<std::vector<uint64_t>> scheduler;
};

template<typename ValueType, typename SolutionType>
MaybeStateResult<SolutionType> computeValuesForMaybeStates(Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal,
                                                           storm::storage::SparseMatrix<ValueType>&& submatrix, std::vector<ValueType> const& b,
                                                           bool produceScheduler, SparseMdpHintType<SolutionType>& hint) {
    // Initialize the solution vector.
    std::vector<SolutionType> x =
        hint.hasValueHint() ? std::move(hint.getValueHint())
                            : std::vector<SolutionType>(submatrix.getRowGroupCount(),
                                                        hint.hasLowerResultBound() ? hint.getLowerResultBound() : storm::utility::zero<SolutionType>());

    // Set up the solver.
    storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType, SolutionType> minMaxLinearEquationSolverFactory;
    std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType, SolutionType>> solver =
        storm::solver::configureMinMaxLinearEquationSolver(env, std::move(goal), minMaxLinearEquationSolverFactory, std::move(submatrix));
    solver->setRequirementsChecked();
    solver->setUncertaintyResolutionMode(goal.getUncertaintyResolutionMode());
    solver->setHasUniqueSolution(hint.hasUniqueSolution());
    solver->setHasNoEndComponents(hint.hasNoEndComponents());
    if (hint.hasLowerResultBound()) {
        solver->setLowerBound(hint.getLowerResultBound());
    }
    if (hint.hasUpperResultBound()) {
        solver->setUpperBound(hint.getUpperResultBound());
    }
    if (hint.hasUpperResultBounds()) {
        solver->setUpperBounds(std::move(hint.getUpperResultBounds()));
    }
    if (hint.hasSchedulerHint()) {
        solver->setInitialScheduler(std::move(hint.getSchedulerHint()));
    }
    solver->setTrackScheduler(produceScheduler);

    // Solve the corresponding system of equations.
    solver->solveEquations(env, x, b);

#ifndef NDEBUG
    // As a sanity check, make sure our local upper bounds were in fact correct.
    if (solver->hasUpperBound(storm::solver::AbstractEquationSolver<SolutionType>::BoundType::Local)) {
        uint64_t relevantState = solver->hasRelevantValues() ? solver->getRelevantValues().getNextSetIndex(0ull) : 0ull;
        std::function<void()> getNextRelevantStateIndex;
        if (solver->hasRelevantValues()) {
            storm::storage::BitVector const& relevantValues = solver->getRelevantValues();
            getNextRelevantStateIndex = [&relevantState, &relevantValues]() { relevantState = relevantValues.getNextSetIndex(++relevantState); };
        } else {
            getNextRelevantStateIndex = [&relevantState]() { ++relevantState; };
        }
        for (; relevantState < solver->getUpperBounds().size(); getNextRelevantStateIndex()) {
            STORM_LOG_ASSERT(x.at(relevantState) <=
                                 solver->getUpperBounds().at(relevantState) + storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision()),
                             "Expecting result value for state " << relevantState << " to be <= " << solver->getUpperBounds().at(relevantState) << ", but got "
                                                                 << x.at(relevantState) << ".");
        }
    }
#endif

    // Create result.
    MaybeStateResult<SolutionType> result(std::move(x));

    // If requested, return the requested scheduler.
    if (produceScheduler) {
        result.scheduler = std::move(solver->getSchedulerChoices());
    }
    return result;
}

struct QualitativeStateSetsUntilProbabilities {
    storm::storage::BitVector maybeStates;
    storm::storage::BitVector statesWithProbability0;
    storm::storage::BitVector statesWithProbability1;
};

template<typename ValueType>
QualitativeStateSetsUntilProbabilities getQualitativeStateSetsUntilProbabilitiesFromHint(ModelCheckerHint const& hint) {
    QualitativeStateSetsUntilProbabilities result;
    result.maybeStates = hint.template asExplicitModelCheckerHint<ValueType>().getMaybeStates();

    // Treat the states with probability zero/one.
    std::vector<ValueType> const& resultsForNonMaybeStates = hint.template asExplicitModelCheckerHint<ValueType>().getResultHint();
    result.statesWithProbability1 = storm::storage::BitVector(result.maybeStates.size());
    result.statesWithProbability0 = storm::storage::BitVector(result.maybeStates.size());
    storm::storage::BitVector nonMaybeStates = ~result.maybeStates;
    for (auto state : nonMaybeStates) {
        if (storm::utility::isOne(resultsForNonMaybeStates[state])) {
            result.statesWithProbability1.set(state, true);
        } else {
            STORM_LOG_THROW(storm::utility::isZero(resultsForNonMaybeStates[state]), storm::exceptions::IllegalArgumentException,
                            "Expected that the result hint specifies probabilities in {0,1} for non-maybe states");
            result.statesWithProbability0.set(state, true);
        }
    }

    return result;
}

template<typename ValueType, typename SolutionType>
QualitativeStateSetsUntilProbabilities computeQualitativeStateSetsUntilProbabilities(storm::solver::SolveGoal<ValueType, SolutionType> const& goal,
                                                                                     storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                     storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                     storm::storage::BitVector const& phiStates,
                                                                                     storm::storage::BitVector const& psiStates) {
    QualitativeStateSetsUntilProbabilities result;

    // Get all states that have probability 0 and 1 of satisfying the until-formula.
    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01;
    if (goal.minimize()) {
        statesWithProbability01 =
            storm::utility::graph::performProb01Min(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, phiStates, psiStates);
    } else {
        statesWithProbability01 =
            storm::utility::graph::performProb01Max(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, phiStates, psiStates);
    }
    result.statesWithProbability0 = std::move(statesWithProbability01.first);
    result.statesWithProbability1 = std::move(statesWithProbability01.second);
    result.maybeStates = ~(result.statesWithProbability0 | result.statesWithProbability1);

    return result;
}

template<typename ValueType, typename SolutionType>
QualitativeStateSetsUntilProbabilities getQualitativeStateSetsUntilProbabilities(storm::solver::SolveGoal<ValueType, SolutionType> const& goal,
                                                                                 storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                 storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                 storm::storage::BitVector const& phiStates,
                                                                                 storm::storage::BitVector const& psiStates, ModelCheckerHint const& hint) {
    if (hint.isExplicitModelCheckerHint() && hint.template asExplicitModelCheckerHint<ValueType>().getComputeOnlyMaybeStates()) {
        return getQualitativeStateSetsUntilProbabilitiesFromHint<ValueType>(hint);
    } else {
        return computeQualitativeStateSetsUntilProbabilities(goal, transitionMatrix, backwardTransitions, phiStates, psiStates);
    }
}

template<typename SolutionType, bool subChoicesCoverOnlyMaybeStates = true>
void extractSchedulerChoices(storm::storage::Scheduler<SolutionType>& scheduler, std::vector<uint64_t> const& subChoices,
                             storm::storage::BitVector const& maybeStates) {
    if constexpr (subChoicesCoverOnlyMaybeStates) {
        auto subChoiceIt = subChoices.begin();
        for (auto maybeState : maybeStates) {
            scheduler.setChoice(*subChoiceIt, maybeState);
            ++subChoiceIt;
        }
        assert(subChoiceIt == subChoices.end());
    } else {
        // See computeFixedPointSystemUntilProbabilities, where we create a different equation system.
        // Consequentially, we run a slightly different code here for interval-based models.
        STORM_LOG_ASSERT(maybeStates.size() == subChoices.size(), "Sizes do not coincide.");
        for (auto maybeState : maybeStates) {
            scheduler.setChoice(subChoices[maybeState], maybeState);
        }
    }
}

template<typename ValueType, typename SolutionType>
void extendScheduler(storm::storage::Scheduler<SolutionType>& scheduler, storm::solver::SolveGoal<ValueType, SolutionType> const& goal,
                     QualitativeStateSetsUntilProbabilities const& qualitativeStateSets, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                     storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                     storm::storage::BitVector const& psiStates) {
    // Finally, if we need to produce a scheduler, we also need to figure out the parts of the scheduler for
    // the states with probability 1 or 0 (depending on whether we maximize or minimize).
    // We also need to define some arbitrary choice for the remaining states to obtain a fully defined scheduler.
    if (goal.minimize()) {
        storm::utility::graph::computeSchedulerProb0E(qualitativeStateSets.statesWithProbability0, transitionMatrix, scheduler);
        for (auto prob1State : qualitativeStateSets.statesWithProbability1) {
            scheduler.setChoice(0, prob1State);
        }
    } else {
        storm::utility::graph::computeSchedulerProb1E(qualitativeStateSets.statesWithProbability1, transitionMatrix, backwardTransitions, phiStates, psiStates,
                                                      scheduler);
        for (auto prob0State : qualitativeStateSets.statesWithProbability0) {
            scheduler.setChoice(0, prob0State);
        }
    }
}

template<typename ValueType, typename SolutionType>
void computeFixedPointSystemUntilProbabilities(storm::solver::SolveGoal<ValueType, SolutionType>& goal,
                                               storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                               QualitativeStateSetsUntilProbabilities const& qualitativeStateSets,
                                               storm::storage::SparseMatrix<ValueType>& submatrix, std::vector<ValueType>& b) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        // For non-interval based models, we can eliminate the rows and columns from the original transition probability matrix for states
        // whose probabilities are already known... However, there is information in the transition to those states.
        // Thus, we cannot eliminate them all.
        // We can however drop all the outgoing transitions from these states.
        // TODO: we can drop more than those entries and actually remove many states (all but the ones reachable in one step from the maybe states),
        // TODO ctned: however, there is quite some bookkeeping involved in projecting the right vectors.
        // TODO ctned: Instead is likely easier to just do a pass and make a unique sink and a unique target state.
        // TODO ctned: If this is ever changed, extractSchedulerChoices must also be updated.
        submatrix = transitionMatrix.filterEntries(transitionMatrix.getRowFilter(qualitativeStateSets.maybeStates));

        // Prepare the right-hand side of the equation system. For entry i this corresponds to
        // the accumulated probability of going from state i to some state that has probability 1.
        storm::utility::vector::setAllValues(b, transitionMatrix.getRowFilter(qualitativeStateSets.statesWithProbability1));
    } else {
        // First, we can eliminate the rows and columns from the original transition probability matrix for states
        // whose probabilities are already known.
        submatrix = transitionMatrix.getSubmatrix(true, qualitativeStateSets.maybeStates, qualitativeStateSets.maybeStates, false);

        // Prepare the right-hand side of the equation system. For entry i this corresponds to
        // the accumulated probability of going from state i to some state that has probability 1.
        b = transitionMatrix.getConstrainedRowGroupSumVector(qualitativeStateSets.maybeStates, qualitativeStateSets.statesWithProbability1);
    }
    // If the solve goal has relevant values, we need to adjust them.
    goal.restrictRelevantValues(qualitativeStateSets.maybeStates);
}

template<typename ValueType, typename SolutionType>
boost::optional<SparseMdpEndComponentInformation<ValueType>> computeFixedPointSystemUntilProbabilitiesEliminateEndComponents(
    storm::solver::SolveGoal<ValueType, SolutionType>& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, QualitativeStateSetsUntilProbabilities const& qualitativeStateSets,
    storm::storage::SparseMatrix<ValueType>& submatrix, std::vector<ValueType>& b, bool produceScheduler) {
    // Get the set of states that (under some scheduler) can stay in the set of maybestates forever
    storm::storage::BitVector candidateStates = storm::utility::graph::performProb0E(
        transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, qualitativeStateSets.maybeStates, ~qualitativeStateSets.maybeStates);

    bool doDecomposition = !candidateStates.empty();

    storm::storage::MaximalEndComponentDecomposition<ValueType> endComponentDecomposition;
    if (doDecomposition) {
        // Compute the states that are in MECs.
        endComponentDecomposition = storm::storage::MaximalEndComponentDecomposition<ValueType>(transitionMatrix, backwardTransitions, candidateStates);
        STORM_LOG_INFO(endComponentDecomposition.statistics(transitionMatrix.getRowGroupCount()));
    }

    // Only do more work if there are actually end-components.
    if (doDecomposition && !endComponentDecomposition.empty()) {
        STORM_LOG_DEBUG("Eliminating " << endComponentDecomposition.size() << " EC(s).");
        SparseMdpEndComponentInformation<ValueType> result = SparseMdpEndComponentInformation<ValueType>::eliminateEndComponents(
            endComponentDecomposition, transitionMatrix, qualitativeStateSets.maybeStates, &qualitativeStateSets.statesWithProbability1, nullptr, nullptr,
            submatrix, &b, nullptr, produceScheduler);

        // If the solve goal has relevant values, we need to adjust them.
        if (goal.hasRelevantValues()) {
            storm::storage::BitVector newRelevantValues(submatrix.getRowGroupCount());
            for (auto state : goal.relevantValues()) {
                if (qualitativeStateSets.maybeStates.get(state)) {
                    newRelevantValues.set(result.getRowGroupAfterElimination(state));
                }
            }
            if (!newRelevantValues.empty()) {
                goal.setRelevantValues(std::move(newRelevantValues));
            }
        }

        return result;
    } else {
        STORM_LOG_DEBUG("Not eliminating ECs as there are none.");
        computeFixedPointSystemUntilProbabilities(goal, transitionMatrix, qualitativeStateSets, submatrix, b);

        return boost::none;
    }
}

template<typename ValueType, typename SolutionType>
MDPSparseModelCheckingHelperReturnType<SolutionType> SparseMdpPrctlHelper<ValueType, SolutionType>::computeUntilProbabilities(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
    bool qualitative, bool produceScheduler, ModelCheckerHint const& hint) {
    STORM_LOG_THROW(!qualitative || !produceScheduler, storm::exceptions::InvalidSettingsException,
                    "Cannot produce scheduler when performing qualitative model checking only.");

    // Prepare resulting vector.
    std::vector<SolutionType> result(transitionMatrix.getRowGroupCount(), storm::utility::zero<SolutionType>());

    // We need to identify the maybe states (states which have a probability for satisfying the until formula
    // that is strictly between 0 and 1) and the states that satisfy the formula with probablity 1 and 0, respectively.
    QualitativeStateSetsUntilProbabilities qualitativeStateSets =
        getQualitativeStateSetsUntilProbabilities(goal, transitionMatrix, backwardTransitions, phiStates, psiStates, hint);

    STORM_LOG_INFO("Preprocessing: " << qualitativeStateSets.statesWithProbability1.getNumberOfSetBits() << " states with probability 1, "
                                     << qualitativeStateSets.statesWithProbability0.getNumberOfSetBits() << " with probability 0 ("
                                     << qualitativeStateSets.maybeStates.getNumberOfSetBits() << " states remaining).");

    // Set values of resulting vector that are known exactly.
    storm::utility::vector::setVectorValues<SolutionType>(result, qualitativeStateSets.statesWithProbability1, storm::utility::one<SolutionType>());

    // Check if the values of the maybe states are relevant for the SolveGoal
    bool maybeStatesNotRelevant = goal.hasRelevantValues() && goal.relevantValues().isDisjointFrom(qualitativeStateSets.maybeStates);

    // If requested, we will produce a scheduler.
    std::unique_ptr<storm::storage::Scheduler<SolutionType>> scheduler;
    if (produceScheduler) {
        scheduler = std::make_unique<storm::storage::Scheduler<SolutionType>>(transitionMatrix.getRowGroupCount());
        // If maybeStatesNotRelevant is true, we have to set the scheduler for maybe states as "dontCare"
        if (maybeStatesNotRelevant) {
            for (auto state : qualitativeStateSets.maybeStates) {
                scheduler->setDontCare(state);
            }
        }
    }

    // Check whether we need to compute exact probabilities for some states.
    if (qualitative || maybeStatesNotRelevant) {
        // Set the values for all maybe-states to 0.5 to indicate that their probability values are neither 0 nor 1.
        storm::utility::vector::setVectorValues<SolutionType>(result, qualitativeStateSets.maybeStates, storm::utility::convertNumber<SolutionType>(0.5));
    } else {
        if (!qualitativeStateSets.maybeStates.empty()) {
            // In this case we have have to compute the remaining probabilities.

            // Obtain proper hint information either from the provided hint or from requirements of the solver.
            SparseMdpHintType<SolutionType> hintInformation = computeHints<ValueType, SolutionType>(
                env, SemanticSolutionType::UntilProbabilities, hint, goal.direction(), transitionMatrix, backwardTransitions, qualitativeStateSets.maybeStates,
                phiStates, qualitativeStateSets.statesWithProbability1, produceScheduler);

            // Declare the components of the equation system we will solve.
            storm::storage::SparseMatrix<ValueType> submatrix;
            std::vector<ValueType> b;

            // If the hint information tells us that we have to eliminate MECs, we do so now.
            boost::optional<SparseMdpEndComponentInformation<ValueType>> ecInformation;
            if (hintInformation.getEliminateEndComponents()) {
                ecInformation = computeFixedPointSystemUntilProbabilitiesEliminateEndComponents(goal, transitionMatrix, backwardTransitions,
                                                                                                qualitativeStateSets, submatrix, b, produceScheduler);
            } else {
                // Otherwise, we compute the standard equations.
                computeFixedPointSystemUntilProbabilities(goal, transitionMatrix, qualitativeStateSets, submatrix, b);
            }

            // Now compute the results for the maybe states.
            MaybeStateResult<SolutionType> resultForMaybeStates =
                computeValuesForMaybeStates(env, std::move(goal), std::move(submatrix), b, produceScheduler, hintInformation);

            // If we eliminated end components, we need to extract the result differently.
            if (ecInformation && ecInformation.get().getEliminatedEndComponents()) {
                if constexpr (storm::IsIntervalType<ValueType>) {
                    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support this end component with interval models.");
                } else {
                    ecInformation.get().setValues(result, qualitativeStateSets.maybeStates, resultForMaybeStates.getValues());
                    if (produceScheduler) {
                        ecInformation.get().setScheduler(*scheduler, qualitativeStateSets.maybeStates, transitionMatrix, backwardTransitions,
                                                         resultForMaybeStates.getScheduler());
                    }
                }
            } else {
                // Set values of resulting vector according to result.
                if constexpr (!storm::IsIntervalType<ValueType>) {
                    // For non-interval models, we only operated on the maybe states, and we must recover the qualitative values for the other state.
                    storm::utility::vector::setVectorValues<SolutionType>(result, qualitativeStateSets.maybeStates, resultForMaybeStates.getValues());
                } else {
                    // For interval models, the result for maybe states indeed also holds values for all qualitative states.
                    STORM_LOG_ASSERT(resultForMaybeStates.getValues().size() == transitionMatrix.getColumnCount(), "Dimensions do not match");
                    result = resultForMaybeStates.getValues();
                }
                if (produceScheduler) {
                    extractSchedulerChoices<SolutionType, !storm::IsIntervalType<ValueType>>(*scheduler, resultForMaybeStates.getScheduler(),
                                                                                             qualitativeStateSets.maybeStates);
                }
            }
        }
    }

    // Extend scheduler with choices for the states in the qualitative state sets.
    if (produceScheduler) {
        extendScheduler(*scheduler, goal, qualitativeStateSets, transitionMatrix, backwardTransitions, phiStates, psiStates);
    }

    // Sanity check for created scheduler.
    STORM_LOG_ASSERT(!produceScheduler || scheduler, "Expected that a scheduler was obtained.");
    STORM_LOG_ASSERT((!produceScheduler && !scheduler) || !scheduler->isPartialScheduler(), "Expected a fully defined scheduler");
    STORM_LOG_ASSERT((!produceScheduler && !scheduler) || scheduler->isDeterministicScheduler(), "Expected a deterministic scheduler");
    STORM_LOG_ASSERT((!produceScheduler && !scheduler) || scheduler->isMemorylessScheduler(), "Expected a memoryless scheduler");

    // Return result.
    return MDPSparseModelCheckingHelperReturnType<SolutionType>(std::move(result), std::move(scheduler));
}

template<typename ValueType, typename SolutionType>
MDPSparseModelCheckingHelperReturnType<SolutionType> SparseMdpPrctlHelper<ValueType, SolutionType>::computeGloballyProbabilities(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& psiStates, bool qualitative, bool produceScheduler,
    bool useMecBasedTechnique) {
    if (useMecBasedTechnique) {
        // TODO: does this really work for minimizing objectives?
        storm::storage::MaximalEndComponentDecomposition<ValueType> mecDecomposition(transitionMatrix, backwardTransitions, psiStates);
        storm::storage::BitVector statesInPsiMecs(transitionMatrix.getRowGroupCount());
        for (auto const& mec : mecDecomposition) {
            for (auto const& stateActionsPair : mec) {
                statesInPsiMecs.set(stateActionsPair.first, true);
            }
        }

        return computeUntilProbabilities(env, std::move(goal), transitionMatrix, backwardTransitions, psiStates, statesInPsiMecs, qualitative,
                                         produceScheduler);
    } else {
        goal.oneMinus();
        auto result =
            computeUntilProbabilities(env, std::move(goal), transitionMatrix, backwardTransitions,
                                      storm::storage::BitVector(transitionMatrix.getRowGroupCount(), true), ~psiStates, qualitative, produceScheduler);
        for (auto& element : result.values) {
            element = storm::utility::one<SolutionType>() - element;
        }
        return result;
    }
}

template<typename ValueType, typename SolutionType>
template<typename RewardModelType>
std::vector<SolutionType> SparseMdpPrctlHelper<ValueType, SolutionType>::computeInstantaneousRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    RewardModelType const& rewardModel, uint_fast64_t stepCount) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support instantenous rewards with interval models.");
    } else {
        // Only compute the result if the model has a state-based reward this->getModel().
        STORM_LOG_THROW(rewardModel.hasStateRewards(), storm::exceptions::InvalidPropertyException,
                        "Computing instantaneous rewards for a reward model that does not define any state-rewards. The result is trivially 0.");
        // Initialize result to state rewards of the this->getModel().
        std::vector<SolutionType> result(rewardModel.getStateRewardVector());

        auto multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, transitionMatrix);
        multiplier->repeatedMultiplyAndReduce(env, goal.direction(), result, nullptr, stepCount, goal.getUncertaintyResolutionMode());

        return result;
    }
}

template<typename ValueType, typename SolutionType>
template<typename RewardModelType>
std::vector<SolutionType> SparseMdpPrctlHelper<ValueType, SolutionType>::computeCumulativeRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    RewardModelType const& rewardModel, uint_fast64_t stepBound) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support cumulative rewards with interval models.");
    } else {
        // Only compute the result if the model has at least one reward this->getModel().
        STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");

        // Compute the reward vector to add in each step based on the available reward models.
        std::vector<ValueType> totalRewardVector = rewardModel.getTotalRewardVector(transitionMatrix);

        // Initialize result to the zero vector.
        std::vector<SolutionType> result(transitionMatrix.getRowGroupCount(), storm::utility::zero<SolutionType>());

        auto multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, transitionMatrix);
        multiplier->repeatedMultiplyAndReduce(env, goal.direction(), result, &totalRewardVector, stepBound, goal.getUncertaintyResolutionMode());

        return result;
    }
}

template<typename ValueType, typename SolutionType>
template<typename RewardModelType>
MDPSparseModelCheckingHelperReturnType<SolutionType> SparseMdpPrctlHelper<ValueType, SolutionType>::computeTotalRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, RewardModelType const& rewardModel, bool qualitative, bool produceScheduler,
    ModelCheckerHint const& hint) {
    STORM_LOG_THROW(!rewardModel.hasNegativeRewards(), storm::exceptions::NotImplementedException,
                    "The reward model contains negative rewards. This is not implemented by the total rewards computation.");
    // Reduce to reachability rewards
    if (goal.minimize()) {
        // Identify the states from which no reward can be collected under some scheduler
        storm::storage::BitVector choicesWithoutReward = rewardModel.getChoicesWithZeroReward(transitionMatrix);
        storm::storage::BitVector statesWithZeroRewardChoice(transitionMatrix.getRowGroupCount(), false);
        for (uint64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
            if (choicesWithoutReward.getNextSetIndex(transitionMatrix.getRowGroupIndices()[state]) < transitionMatrix.getRowGroupIndices()[state + 1]) {
                statesWithZeroRewardChoice.set(state);
            }
        }
        storm::storage::BitVector rew0EStates =
            storm::utility::graph::performProbGreater0A(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions,
                                                        statesWithZeroRewardChoice, ~statesWithZeroRewardChoice, false, 0, choicesWithoutReward);
        rew0EStates.complement();
        auto result = computeReachabilityRewards(env, std::move(goal), transitionMatrix, backwardTransitions, rewardModel, rew0EStates, qualitative,
                                                 produceScheduler, hint);
        if (result.scheduler) {
            storm::utility::graph::computeSchedulerStayingInStates(rew0EStates, transitionMatrix, *result.scheduler, choicesWithoutReward);
        }
        return result;
    } else {
        // Identify the states from which only states with zero reward are reachable.
        storm::storage::BitVector statesWithoutReward = rewardModel.getStatesWithZeroReward(transitionMatrix);
        storm::storage::BitVector rew0AStates = storm::utility::graph::performProbGreater0E(backwardTransitions, statesWithoutReward, ~statesWithoutReward);
        rew0AStates.complement();

        // There might be end components that consists only of states/choices with zero rewards. The reachability reward semantics would assign such
        // end components reward infinity. To avoid this, we potentially need to eliminate such end components
        storm::storage::BitVector trueStates(transitionMatrix.getRowGroupCount(), true);
        if (storm::utility::graph::performProb1A(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, trueStates, rew0AStates)
                .full()) {
            return computeReachabilityRewards(env, std::move(goal), transitionMatrix, backwardTransitions, rewardModel, rew0AStates, qualitative,
                                              produceScheduler, hint);
        } else {
            // The transformation of schedulers for the ec-eliminated system back to the original one is not implemented.
            STORM_LOG_ERROR_COND(!produceScheduler, "Can not produce scheduler for this property (functionality not implemented");
            storm::storage::BitVector choicesWithoutReward = rewardModel.getChoicesWithZeroReward(transitionMatrix);
            auto ecElimResult = storm::transformer::EndComponentEliminator<ValueType>::transform(
                transitionMatrix, storm::storage::BitVector(transitionMatrix.getRowGroupCount(), true), choicesWithoutReward, rew0AStates, true);
            storm::storage::BitVector newRew0AStates(ecElimResult.matrix.getRowGroupCount(), false);
            for (auto oldRew0AState : rew0AStates) {
                newRew0AStates.set(ecElimResult.oldToNewStateMapping[oldRew0AState]);
            }

            if (goal.hasRelevantValues()) {
                storm::storage::BitVector newRelevantValues(ecElimResult.matrix.getRowGroupCount(), false);
                for (auto oldRelevantState : goal.relevantValues()) {
                    newRelevantValues.set(ecElimResult.oldToNewStateMapping[oldRelevantState]);
                }
                goal.relevantValues() = std::move(newRelevantValues);
            }

            MDPSparseModelCheckingHelperReturnType<SolutionType> result = computeReachabilityRewardsHelper(
                env, std::move(goal), ecElimResult.matrix, ecElimResult.matrix.transpose(true),
                [&](uint_fast64_t rowCount, storm::storage::SparseMatrix<ValueType> const& newTransitionMatrix, storm::storage::BitVector const& maybeStates) {
                    std::vector<ValueType> result;
                    std::vector<ValueType> oldChoiceRewards = rewardModel.getTotalRewardVector(transitionMatrix);
                    result.reserve(rowCount);
                    for (uint64_t newState : maybeStates) {
                        for (auto newChoice : newTransitionMatrix.getRowGroupIndices(newState)) {
                            uint64_t oldChoice = ecElimResult.newToOldRowMapping[newChoice];
                            result.push_back(oldChoiceRewards[oldChoice]);
                        }
                    }
                    STORM_LOG_ASSERT(result.size() == rowCount, "Unexpected size of reward vector.");
                    return result;
                },
                newRew0AStates, qualitative, false,
                [&]() {
                    storm::storage::BitVector newStatesWithoutReward(ecElimResult.matrix.getRowGroupCount(), false);
                    for (auto oldStateWithoutRew : statesWithoutReward) {
                        newStatesWithoutReward.set(ecElimResult.oldToNewStateMapping[oldStateWithoutRew]);
                    }
                    return newStatesWithoutReward;
                },
                [&]() {
                    storm::storage::BitVector newChoicesWithoutReward(ecElimResult.matrix.getRowGroupCount(), false);
                    for (uint64_t newChoice = 0; newChoice < ecElimResult.matrix.getRowCount(); ++newChoice) {
                        if (choicesWithoutReward.get(ecElimResult.newToOldRowMapping[newChoice])) {
                            newChoicesWithoutReward.set(newChoice);
                        }
                    }
                    return newChoicesWithoutReward;
                });

            std::vector<SolutionType> resultInEcQuotient = std::move(result.values);
            result.values.resize(ecElimResult.oldToNewStateMapping.size());
            storm::utility::vector::selectVectorValues(result.values, ecElimResult.oldToNewStateMapping, resultInEcQuotient);
            return result;
        }
    }
}

template<typename ValueType, typename SolutionType>
template<typename RewardModelType>
std::vector<SolutionType> SparseMdpPrctlHelper<ValueType, SolutionType>::computeDiscountedCumulativeRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    RewardModelType const& rewardModel, uint_fast64_t stepBound, ValueType discountFactor) {
    // Only compute the result if the model has at least one reward this->getModel().
    STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");

    // Compute the reward vector to add in each step based on the available reward models.
    std::vector<SolutionType> totalRewardVector = rewardModel.getTotalRewardVector(transitionMatrix);

    // Initialize result to the zero vector.
    std::vector<SolutionType> result(transitionMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());

    auto multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, transitionMatrix);
    multiplier->repeatedMultiplyAndReduceWithFactor(env, goal.direction(), result, &totalRewardVector, stepBound, discountFactor,
                                                    goal.getUncertaintyResolutionMode());

    return result;
}

template<typename ValueType, typename SolutionType>
template<typename RewardModelType>
MDPSparseModelCheckingHelperReturnType<SolutionType> SparseMdpPrctlHelper<ValueType, SolutionType>::computeDiscountedTotalRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, RewardModelType const& rewardModel, bool qualitative, bool produceScheduler,
    ValueType discountFactor, ModelCheckerHint const& hint) {
    // If the solver is set to force exact results, throw an error
    STORM_LOG_THROW(!env.solver().isForceExact(), storm::exceptions::NotSupportedException,
                    "Exact solving of discounted total reward objectives is currently not supported.");

    std::vector<ValueType> b;

    std::vector<SolutionType> x = std::vector<SolutionType>(transitionMatrix.getRowGroupCount(), storm::utility::zero<SolutionType>());
    b = rewardModel.getTotalRewardVector(transitionMatrix);
    storm::modelchecker::helper::DiscountingHelper<ValueType> discountingHelper(transitionMatrix, discountFactor, produceScheduler);

    discountingHelper.solveWithDiscountedValueIteration(env, goal.direction(), x, b);

    std::unique_ptr<storm::storage::Scheduler<SolutionType>> scheduler;
    if (produceScheduler) {
        scheduler = std::make_unique<storm::storage::Scheduler<ValueType>>(discountingHelper.computeScheduler());
    }
    STORM_LOG_ASSERT(!produceScheduler || scheduler, "Expected that a scheduler was obtained.");
    STORM_LOG_ASSERT((!produceScheduler && !scheduler) || !scheduler->isPartialScheduler(), "Expected a fully defined scheduler");
    STORM_LOG_ASSERT((!produceScheduler && !scheduler) || scheduler->isDeterministicScheduler(), "Expected a deterministic scheduler");
    STORM_LOG_ASSERT((!produceScheduler && !scheduler) || scheduler->isMemorylessScheduler(), "Expected a memoryless scheduler");
    return MDPSparseModelCheckingHelperReturnType<SolutionType>(std::move(x), std::move(scheduler));
}

template<typename ValueType, typename SolutionType>
template<typename RewardModelType>
MDPSparseModelCheckingHelperReturnType<SolutionType> SparseMdpPrctlHelper<ValueType, SolutionType>::computeReachabilityRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, RewardModelType const& rewardModel, storm::storage::BitVector const& targetStates,
    bool qualitative, bool produceScheduler, ModelCheckerHint const& hint) {
    // Only compute the result if the model has at least one reward this->getModel().
    STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Reward model for formula is empty. Skipping formula.");
    return computeReachabilityRewardsHelper(
        env, std::move(goal), transitionMatrix, backwardTransitions,
        [&rewardModel](uint_fast64_t rowCount, storm::storage::SparseMatrix<ValueType> const& transMatrix, storm::storage::BitVector const& maybeStates) {
            return rewardModel.getTotalRewardVector(rowCount, transMatrix, maybeStates);
        },
        targetStates, qualitative, produceScheduler, [&]() { return rewardModel.getStatesWithZeroReward(transitionMatrix); },
        [&]() { return rewardModel.getChoicesWithZeroReward(transitionMatrix); }, hint);
}

template<typename ValueType, typename SolutionType>
MDPSparseModelCheckingHelperReturnType<SolutionType> SparseMdpPrctlHelper<ValueType, SolutionType>::computeReachabilityTimes(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& targetStates, bool qualitative, bool produceScheduler,
    ModelCheckerHint const& hint) {
    return computeReachabilityRewardsHelper(
        env, std::move(goal), transitionMatrix, backwardTransitions,
        [](uint_fast64_t rowCount, storm::storage::SparseMatrix<ValueType> const&, storm::storage::BitVector const&) {
            return std::vector<ValueType>(rowCount, storm::utility::one<ValueType>());
        },
        targetStates, qualitative, produceScheduler, [&]() { return storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false); },
        [&]() { return storm::storage::BitVector(transitionMatrix.getRowCount(), false); }, hint);
}

template<typename ValueType, typename SolutionType>
template<typename IntervalRewardType>
std::vector<SolutionType> SparseMdpPrctlHelper<ValueType, SolutionType>::computeReachabilityRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
    storm::models::sparse::StandardRewardModel<IntervalRewardType> const& intervalRewardModel, bool lowerBoundOfIntervals,
    storm::storage::BitVector const& targetStates, bool qualitative) {
    // Only compute the result if the reward model is not empty.
    STORM_LOG_THROW(!intervalRewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");
    if constexpr (!std::is_same_v<IntervalBaseType<ValueType>, IntervalBaseType<IntervalRewardType>>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support a valuetype and reward type having different base types.");
    } else {
        return computeReachabilityRewardsHelper(
                   env, std::move(goal), transitionMatrix, backwardTransitions,
                   [&](uint_fast64_t rowCount, storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& maybeStates) {
                       std::vector<ValueType> result;
                       result.reserve(rowCount);
                       std::vector<IntervalRewardType> subIntervalVector = intervalRewardModel.getTotalRewardVector(rowCount, transitionMatrix, maybeStates);
                       for (auto const& interval : subIntervalVector) {
                           result.push_back(lowerBoundOfIntervals ? interval.lower() : interval.upper());
                       }
                       return result;
                   },
                   targetStates, qualitative, false,
                   [&]() {
                       return intervalRewardModel.getStatesWithFilter(transitionMatrix, [&](IntervalRewardType const& i) {
                           return storm::utility::isZero(lowerBoundOfIntervals ? i.lower() : i.upper());
                       });
                   },
                   [&]() {
                       return intervalRewardModel.getChoicesWithFilter(transitionMatrix, [&](IntervalRewardType const& i) {
                           return storm::utility::isZero(lowerBoundOfIntervals ? i.lower() : i.upper());
                       });
                   })
            .values;
    }
}

struct QualitativeStateSetsReachabilityRewards {
    storm::storage::BitVector maybeStates;
    storm::storage::BitVector infinityStates;
    storm::storage::BitVector rewardZeroStates;
};

template<typename ValueType>
QualitativeStateSetsReachabilityRewards getQualitativeStateSetsReachabilityRewardsFromHint(ModelCheckerHint const& hint,
                                                                                           storm::storage::BitVector const& targetStates) {
    QualitativeStateSetsReachabilityRewards result;
    result.maybeStates = hint.template asExplicitModelCheckerHint<ValueType>().getMaybeStates();

    // Treat the states with reward zero/infinity.
    std::vector<ValueType> const& resultsForNonMaybeStates = hint.template asExplicitModelCheckerHint<ValueType>().getResultHint();
    result.infinityStates = storm::storage::BitVector(result.maybeStates.size());
    result.rewardZeroStates = storm::storage::BitVector(result.maybeStates.size());
    storm::storage::BitVector nonMaybeStates = ~result.maybeStates;
    for (auto state : nonMaybeStates) {
        if (storm::utility::isZero(resultsForNonMaybeStates[state])) {
            result.rewardZeroStates.set(state, true);
        } else {
            STORM_LOG_THROW(storm::utility::isInfinity(resultsForNonMaybeStates[state]), storm::exceptions::IllegalArgumentException,
                            "Expected that the result hint specifies probabilities in {0,infinity} for non-maybe states");
            result.infinityStates.set(state, true);
        }
    }
    return result;
}

template<typename ValueType, typename SolutionType>
QualitativeStateSetsReachabilityRewards computeQualitativeStateSetsReachabilityRewards(
    storm::solver::SolveGoal<ValueType, SolutionType> const& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& targetStates,
    std::function<storm::storage::BitVector()> const& zeroRewardStatesGetter, std::function<storm::storage::BitVector()> const& zeroRewardChoicesGetter) {
    QualitativeStateSetsReachabilityRewards result;
    storm::storage::BitVector trueStates(transitionMatrix.getRowGroupCount(), true);
    if (goal.minimize()) {
        result.infinityStates =
            storm::utility::graph::performProb1E(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, trueStates, targetStates);
    } else {
        result.infinityStates =
            storm::utility::graph::performProb1A(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, trueStates, targetStates);
    }
    result.infinityStates.complement();

    if (storm::settings::getModule<storm::settings::modules::ModelCheckerSettings>().isFilterRewZeroSet()) {
        if (goal.minimize()) {
            result.rewardZeroStates = storm::utility::graph::performProb1E(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions,
                                                                           trueStates, targetStates, zeroRewardChoicesGetter());
        } else {
            result.rewardZeroStates = storm::utility::graph::performProb1A(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions,
                                                                           zeroRewardStatesGetter(), targetStates);
        }
    } else {
        result.rewardZeroStates = targetStates;
    }
    result.maybeStates = ~(result.rewardZeroStates | result.infinityStates);
    return result;
}

template<typename ValueType, typename SolutionType>
QualitativeStateSetsReachabilityRewards getQualitativeStateSetsReachabilityRewards(storm::solver::SolveGoal<ValueType, SolutionType> const& goal,
                                                                                   storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                   storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                   storm::storage::BitVector const& targetStates, ModelCheckerHint const& hint,
                                                                                   std::function<storm::storage::BitVector()> const& zeroRewardStatesGetter,
                                                                                   std::function<storm::storage::BitVector()> const& zeroRewardChoicesGetter) {
    if (hint.isExplicitModelCheckerHint() && hint.template asExplicitModelCheckerHint<ValueType>().getComputeOnlyMaybeStates()) {
        return getQualitativeStateSetsReachabilityRewardsFromHint<ValueType>(hint, targetStates);
    } else {
        return computeQualitativeStateSetsReachabilityRewards(goal, transitionMatrix, backwardTransitions, targetStates, zeroRewardStatesGetter,
                                                              zeroRewardChoicesGetter);
    }
}

template<typename ValueType, typename SolutionType>
void extendScheduler(storm::storage::Scheduler<SolutionType>& scheduler, storm::solver::SolveGoal<ValueType, SolutionType> const& goal,
                     QualitativeStateSetsReachabilityRewards const& qualitativeStateSets, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                     storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& targetStates,
                     std::function<storm::storage::BitVector()> const& zeroRewardChoicesGetter) {
    // Finally, if we need to produce a scheduler, we also need to figure out the parts of the scheduler for
    // the states with reward zero/infinity.
    if (goal.minimize()) {
        storm::utility::graph::computeSchedulerProb1E(qualitativeStateSets.rewardZeroStates, transitionMatrix, backwardTransitions,
                                                      qualitativeStateSets.rewardZeroStates, targetStates, scheduler, zeroRewardChoicesGetter());
        for (auto state : qualitativeStateSets.infinityStates) {
            scheduler.setChoice(0, state);
        }
    } else {
        storm::utility::graph::computeSchedulerRewInf(qualitativeStateSets.infinityStates, transitionMatrix, backwardTransitions, scheduler);
        for (auto state : qualitativeStateSets.rewardZeroStates) {
            scheduler.setChoice(0, state);
        }
    }
}

template<typename ValueType, typename SolutionType>
void extractSchedulerChoices(storm::storage::Scheduler<SolutionType>& scheduler, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                             std::vector<uint_fast64_t> const& subChoices, storm::storage::BitVector const& maybeStates,
                             boost::optional<storm::storage::BitVector> const& selectedChoices) {
    auto subChoiceIt = subChoices.begin();
    if (selectedChoices) {
        for (auto maybeState : maybeStates) {
            // find the rowindex that corresponds to the selected row of the submodel
            uint_fast64_t firstRowIndex = transitionMatrix.getRowGroupIndices()[maybeState];
            uint_fast64_t selectedRowIndex = selectedChoices->getNextSetIndex(firstRowIndex);
            for (uint_fast64_t choice = 0; choice < *subChoiceIt; ++choice) {
                selectedRowIndex = selectedChoices->getNextSetIndex(selectedRowIndex + 1);
            }
            scheduler.setChoice(selectedRowIndex - firstRowIndex, maybeState);
            ++subChoiceIt;
        }
    } else {
        for (auto maybeState : maybeStates) {
            scheduler.setChoice(*subChoiceIt, maybeState);
            ++subChoiceIt;
        }
    }
    assert(subChoiceIt == subChoices.end());
}

template<typename ValueType, typename SolutionType>
void computeFixedPointSystemReachabilityRewards(
    storm::solver::SolveGoal<ValueType, SolutionType>& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    QualitativeStateSetsReachabilityRewards const& qualitativeStateSets, boost::optional<storm::storage::BitVector> const& selectedChoices,
    std::function<std::vector<ValueType>(uint_fast64_t, storm::storage::SparseMatrix<ValueType> const&, storm::storage::BitVector const&)> const&
        totalStateRewardVectorGetter,
    storm::storage::SparseMatrix<ValueType>& submatrix, std::vector<ValueType>& b, std::vector<ValueType>* oneStepTargetProbabilities = nullptr) {
    // Remove rows and columns from the original transition probability matrix for states whose reward values are already known.
    // If there are infinity states, we additionally have to remove choices of maybeState that lead to infinity.
    if (qualitativeStateSets.infinityStates.empty()) {
        submatrix = transitionMatrix.getSubmatrix(true, qualitativeStateSets.maybeStates, qualitativeStateSets.maybeStates, false);
        b = totalStateRewardVectorGetter(submatrix.getRowCount(), transitionMatrix, qualitativeStateSets.maybeStates);
        if (oneStepTargetProbabilities) {
            (*oneStepTargetProbabilities) =
                transitionMatrix.getConstrainedRowGroupSumVector(qualitativeStateSets.maybeStates, qualitativeStateSets.rewardZeroStates);
        }
    } else {
        submatrix = transitionMatrix.getSubmatrix(false, *selectedChoices, qualitativeStateSets.maybeStates, false);
        b = totalStateRewardVectorGetter(transitionMatrix.getRowCount(), transitionMatrix,
                                         storm::storage::BitVector(transitionMatrix.getRowGroupCount(), true));
        storm::utility::vector::filterVectorInPlace(b, *selectedChoices);
        if (oneStepTargetProbabilities) {
            (*oneStepTargetProbabilities) = transitionMatrix.getConstrainedRowSumVector(*selectedChoices, qualitativeStateSets.rewardZeroStates);
        }
    }

    // If the solve goal has relevant values, we need to adjust them.
    goal.restrictRelevantValues(qualitativeStateSets.maybeStates);
}

template<typename ValueType, typename SolutionType>
boost::optional<SparseMdpEndComponentInformation<ValueType>> computeFixedPointSystemReachabilityRewardsEliminateEndComponents(
    storm::solver::SolveGoal<ValueType, SolutionType>& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, QualitativeStateSetsReachabilityRewards const& qualitativeStateSets,
    boost::optional<storm::storage::BitVector> const& selectedChoices,
    std::function<std::vector<ValueType>(uint_fast64_t, storm::storage::SparseMatrix<ValueType> const&, storm::storage::BitVector const&)> const&
        totalStateRewardVectorGetter,
    storm::storage::SparseMatrix<ValueType>& submatrix, std::vector<ValueType>& b, boost::optional<std::vector<ValueType>>& oneStepTargetProbabilities,
    bool produceScheduler) {
    // Start by computing the choices with reward 0, as we only want ECs within this fragment.
    storm::storage::BitVector zeroRewardChoices(transitionMatrix.getRowCount());

    // Get the rewards of all choices.
    std::vector<ValueType> rewardVector =
        totalStateRewardVectorGetter(transitionMatrix.getRowCount(), transitionMatrix, storm::storage::BitVector(transitionMatrix.getRowGroupCount(), true));

    uint64_t index = 0;
    for (auto const& e : rewardVector) {
        if (storm::utility::isZero(e)) {
            zeroRewardChoices.set(index);
        }
        ++index;
    }

    // Compute the states that have some zero reward choice.
    storm::storage::BitVector candidateStates(qualitativeStateSets.maybeStates);
    for (auto state : qualitativeStateSets.maybeStates) {
        bool keepState = false;

        for (auto row = transitionMatrix.getRowGroupIndices()[state], rowEnd = transitionMatrix.getRowGroupIndices()[state + 1]; row < rowEnd; ++row) {
            if (zeroRewardChoices.get(row)) {
                keepState = true;
                break;
            }
        }

        if (!keepState) {
            candidateStates.set(state, false);
        }
    }

    // Only keep the candidate states that (under some scheduler) can stay in the set of candidates forever
    candidateStates =
        storm::utility::graph::performProb0E(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, candidateStates, ~candidateStates);

    bool doDecomposition = !candidateStates.empty();

    storm::storage::MaximalEndComponentDecomposition<ValueType> endComponentDecomposition;
    if (doDecomposition) {
        // Then compute the states that are in MECs with zero reward.
        endComponentDecomposition =
            storm::storage::MaximalEndComponentDecomposition<ValueType>(transitionMatrix, backwardTransitions, candidateStates, zeroRewardChoices);
        STORM_LOG_INFO(endComponentDecomposition.statistics(transitionMatrix.getRowGroupCount()));
    }

    // Only do more work if there are actually end-components.
    if (doDecomposition && !endComponentDecomposition.empty()) {
        STORM_LOG_DEBUG("Eliminating " << endComponentDecomposition.size() << " ECs.");
        SparseMdpEndComponentInformation<ValueType> result = SparseMdpEndComponentInformation<ValueType>::eliminateEndComponents(
            endComponentDecomposition, transitionMatrix, qualitativeStateSets.maybeStates,
            oneStepTargetProbabilities ? &qualitativeStateSets.rewardZeroStates : nullptr, selectedChoices ? &selectedChoices.get() : nullptr, &rewardVector,
            submatrix, oneStepTargetProbabilities ? &oneStepTargetProbabilities.get() : nullptr, &b, produceScheduler);

        // If the solve goal has relevant values, we need to adjust them.
        if (goal.hasRelevantValues()) {
            storm::storage::BitVector newRelevantValues(submatrix.getRowGroupCount());
            for (auto state : goal.relevantValues()) {
                if (qualitativeStateSets.maybeStates.get(state)) {
                    newRelevantValues.set(result.getRowGroupAfterElimination(state));
                }
            }
            if (!newRelevantValues.empty()) {
                goal.setRelevantValues(std::move(newRelevantValues));
            }
        }

        return result;
    } else {
        STORM_LOG_DEBUG("Not eliminating ECs as there are none.");
        computeFixedPointSystemReachabilityRewards(goal, transitionMatrix, qualitativeStateSets, selectedChoices, totalStateRewardVectorGetter, submatrix, b,
                                                   oneStepTargetProbabilities ? &oneStepTargetProbabilities.get() : nullptr);
        return boost::none;
    }
}

template<typename ValueType, typename SolutionType>
void computeUpperRewardBounds(SparseMdpHintType<SolutionType>& hintInformation, storm::OptimizationDirection const& direction,
                              storm::storage::SparseMatrix<ValueType> const& submatrix, std::vector<ValueType> const& choiceRewards,
                              std::vector<ValueType> const& oneStepTargetProbabilities) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support computing upper reward bounds with interval models.");
    } else {
        // For the min-case, we use DS-MPI, for the max-case variant 2 of the Baier et al. paper (CAV'17).
        if (direction == storm::OptimizationDirection::Minimize) {
            DsMpiMdpUpperRewardBoundsComputer<ValueType> dsmpi(submatrix, choiceRewards, oneStepTargetProbabilities);
            hintInformation.upperResultBounds = dsmpi.computeUpperBounds();
        } else {
            BaierUpperRewardBoundsComputer<ValueType> baier(submatrix, choiceRewards, oneStepTargetProbabilities);
            hintInformation.upperResultBound = baier.computeUpperBound();
        }
    }
}

template<typename ValueType, typename SolutionType>
MDPSparseModelCheckingHelperReturnType<SolutionType> SparseMdpPrctlHelper<ValueType, SolutionType>::computeReachabilityRewardsHelper(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
    std::function<std::vector<ValueType>(uint_fast64_t, storm::storage::SparseMatrix<ValueType> const&, storm::storage::BitVector const&)> const&
        totalStateRewardVectorGetter,
    storm::storage::BitVector const& targetStates, bool qualitative, bool produceScheduler,
    std::function<storm::storage::BitVector()> const& zeroRewardStatesGetter, std::function<storm::storage::BitVector()> const& zeroRewardChoicesGetter,
    ModelCheckerHint const& hint) {
    // Prepare resulting vector.
    std::vector<SolutionType> result(transitionMatrix.getRowGroupCount(), storm::utility::zero<SolutionType>());

    // Determine which states have a reward that is infinity or less than infinity.
    QualitativeStateSetsReachabilityRewards qualitativeStateSets = getQualitativeStateSetsReachabilityRewards(
        goal, transitionMatrix, backwardTransitions, targetStates, hint, zeroRewardStatesGetter, zeroRewardChoicesGetter);

    STORM_LOG_INFO("Preprocessing: " << qualitativeStateSets.infinityStates.getNumberOfSetBits() << " states with reward infinity, "
                                     << qualitativeStateSets.rewardZeroStates.getNumberOfSetBits() << " states with reward zero ("
                                     << qualitativeStateSets.maybeStates.getNumberOfSetBits() << " states remaining).");

    storm::utility::vector::setVectorValues(result, qualitativeStateSets.infinityStates, storm::utility::infinity<SolutionType>());

    // If requested, we will produce a scheduler.
    std::unique_ptr<storm::storage::Scheduler<SolutionType>> scheduler;
    if (produceScheduler) {
        if constexpr (storm::IsIntervalType<ValueType>) {
            STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support producing schedulers in this function with interval models.");
        } else {
            scheduler = std::make_unique<storm::storage::Scheduler<SolutionType>>(transitionMatrix.getRowGroupCount());
        }
    }

    // Check if the values of the maybe states are relevant for the SolveGoal
    bool maybeStatesNotRelevant = goal.hasRelevantValues() && goal.relevantValues().isDisjointFrom(qualitativeStateSets.maybeStates);

    // Check whether we need to compute exact rewards for some states.
    if (qualitative || maybeStatesNotRelevant) {
        STORM_LOG_INFO("The rewards for the initial states were determined in a preprocessing step. No exact rewards were computed.");
        // Set the values for all maybe-states to 1 to indicate that their reward values
        // are neither 0 nor infinity.
        storm::utility::vector::setVectorValues<SolutionType>(result, qualitativeStateSets.maybeStates, storm::utility::one<SolutionType>());
    } else {
        if (!qualitativeStateSets.maybeStates.empty()) {
            // In this case we have to compute the reward values for the remaining states.

            // Store the choices that lead to non-infinity values. If none, all choices im maybe states can be selected.
            boost::optional<storm::storage::BitVector> selectedChoices;
            if (!qualitativeStateSets.infinityStates.empty()) {
                selectedChoices = transitionMatrix.getRowFilter(qualitativeStateSets.maybeStates, ~qualitativeStateSets.infinityStates);
            }

            // Obtain proper hint information either from the provided hint or from requirements of the solver.
            SparseMdpHintType<SolutionType> hintInformation = computeHints<ValueType, SolutionType>(
                env, SemanticSolutionType::ExpectedRewards, hint, goal.direction(), transitionMatrix, backwardTransitions, qualitativeStateSets.maybeStates,
                ~qualitativeStateSets.rewardZeroStates, qualitativeStateSets.rewardZeroStates, produceScheduler, selectedChoices);

            // Declare the components of the equation system we will solve.
            storm::storage::SparseMatrix<ValueType> submatrix;
            std::vector<ValueType> b;

            // If we need to compute upper bounds on the reward values, we need the one step probabilities
            // to a target state.
            boost::optional<std::vector<ValueType>> oneStepTargetProbabilities;
            if (hintInformation.getComputeUpperBounds()) {
                oneStepTargetProbabilities = std::vector<ValueType>();
            }

            // If the hint information tells us that we have to eliminate MECs, we do so now.
            boost::optional<SparseMdpEndComponentInformation<ValueType>> ecInformation;
            if (hintInformation.getEliminateEndComponents()) {
                if constexpr (storm::IsIntervalType<ValueType>) {
                    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support eliminating end components with interval models.");
                } else {
                    ecInformation = computeFixedPointSystemReachabilityRewardsEliminateEndComponents(
                        goal, transitionMatrix, backwardTransitions, qualitativeStateSets, selectedChoices, totalStateRewardVectorGetter, submatrix, b,
                        oneStepTargetProbabilities, produceScheduler);
                }
            } else {
                // Otherwise, we compute the standard equations.
                computeFixedPointSystemReachabilityRewards(goal, transitionMatrix, qualitativeStateSets, selectedChoices, totalStateRewardVectorGetter,
                                                           submatrix, b, oneStepTargetProbabilities ? &oneStepTargetProbabilities.get() : nullptr);
            }

            // If we need to compute upper bounds, do so now.
            if (hintInformation.getComputeUpperBounds()) {
                STORM_LOG_ASSERT(oneStepTargetProbabilities, "Expecting one step target probability vector to be available.");
                computeUpperRewardBounds(hintInformation, goal.direction(), submatrix, b, oneStepTargetProbabilities.get());
            }

            // Now compute the results for the maybe states.
            MaybeStateResult<SolutionType> resultForMaybeStates =
                computeValuesForMaybeStates(env, std::move(goal), std::move(submatrix), b, produceScheduler, hintInformation);

            // If we eliminated end components, we need to extract the result differently.
            if (ecInformation && ecInformation.get().getEliminatedEndComponents()) {
                if constexpr (storm::IsIntervalType<ValueType>) {
                    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support eliminating end components with interval models.");
                } else {
                    ecInformation.get().setValues(result, qualitativeStateSets.maybeStates, resultForMaybeStates.getValues());
                    if (produceScheduler) {
                        ecInformation.get().setScheduler(*scheduler, qualitativeStateSets.maybeStates, transitionMatrix, backwardTransitions,
                                                         resultForMaybeStates.getScheduler());
                    }
                }
            } else {
                // Set values of resulting vector according to result.
                storm::utility::vector::setVectorValues<SolutionType>(result, qualitativeStateSets.maybeStates, resultForMaybeStates.getValues());
                if (produceScheduler) {
                    extractSchedulerChoices(*scheduler, transitionMatrix, resultForMaybeStates.getScheduler(), qualitativeStateSets.maybeStates,
                                            selectedChoices);
                }
            }
        }
    }

    // Extend scheduler with choices for the states in the qualitative state sets.
    if (produceScheduler) {
        extendScheduler(*scheduler, goal, qualitativeStateSets, transitionMatrix, backwardTransitions, targetStates, zeroRewardChoicesGetter);
    }

    // Sanity check for created scheduler.
    STORM_LOG_ASSERT(!produceScheduler || scheduler, "Expected that a scheduler was obtained.");
    STORM_LOG_ASSERT((!produceScheduler && !scheduler) || !scheduler->isPartialScheduler(), "Expected a fully defined scheduler");
    STORM_LOG_ASSERT((!produceScheduler && !scheduler) || scheduler->isDeterministicScheduler(), "Expected a deterministic scheduler");
    STORM_LOG_ASSERT((!produceScheduler && !scheduler) || scheduler->isMemorylessScheduler(), "Expected a memoryless scheduler");

    if constexpr (storm::IsIntervalType<ValueType>) {
        return MDPSparseModelCheckingHelperReturnType<SolutionType>(std::move(result));
    } else {
        return MDPSparseModelCheckingHelperReturnType<SolutionType>(std::move(result), std::move(scheduler));
    }
}

template class SparseMdpPrctlHelper<double>;
template std::vector<double> SparseMdpPrctlHelper<double>::computeInstantaneousRewards(Environment const& env, storm::solver::SolveGoal<double>&& goal,
                                                                                       storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                                       storm::models::sparse::StandardRewardModel<double> const& rewardModel,
                                                                                       uint_fast64_t stepCount);
template std::vector<double> SparseMdpPrctlHelper<double>::computeCumulativeRewards(Environment const& env, storm::solver::SolveGoal<double>&& goal,
                                                                                    storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                                    storm::models::sparse::StandardRewardModel<double> const& rewardModel,
                                                                                    uint_fast64_t stepBound);
template std::vector<double> SparseMdpPrctlHelper<double>::computeDiscountedCumulativeRewards(
    Environment const& env, storm::solver::SolveGoal<double>&& goal, storm::storage::SparseMatrix<double> const& transitionMatrix,
    storm::models::sparse::StandardRewardModel<double> const& rewardModel, uint_fast64_t stepBound, double discountFactor);
template MDPSparseModelCheckingHelperReturnType<double> SparseMdpPrctlHelper<double>::computeReachabilityRewards(
    Environment const& env, storm::solver::SolveGoal<double>&& goal, storm::storage::SparseMatrix<double> const& transitionMatrix,
    storm::storage::SparseMatrix<double> const& backwardTransitions, storm::models::sparse::StandardRewardModel<double> const& rewardModel,
    storm::storage::BitVector const& targetStates, bool qualitative, bool produceScheduler, ModelCheckerHint const& hint);
template MDPSparseModelCheckingHelperReturnType<double> SparseMdpPrctlHelper<double>::computeTotalRewards(
    Environment const& env, storm::solver::SolveGoal<double>&& goal, storm::storage::SparseMatrix<double> const& transitionMatrix,
    storm::storage::SparseMatrix<double> const& backwardTransitions, storm::models::sparse::StandardRewardModel<double> const& rewardModel, bool qualitative,
    bool produceScheduler, ModelCheckerHint const& hint);
template MDPSparseModelCheckingHelperReturnType<double> SparseMdpPrctlHelper<double>::computeDiscountedTotalRewards(
    Environment const& env, storm::solver::SolveGoal<double>&& goal, storm::storage::SparseMatrix<double> const& transitionMatrix,
    storm::storage::SparseMatrix<double> const& backwardTransitions, storm::models::sparse::StandardRewardModel<double> const& rewardModel, bool qualitative,
    bool produceScheduler, double discountFactor, ModelCheckerHint const& hint);

template class SparseMdpPrctlHelper<storm::RationalNumber>;
template std::vector<storm::RationalNumber> SparseMdpPrctlHelper<storm::RationalNumber>::computeInstantaneousRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalNumber>&& goal, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
    storm::models::sparse::StandardRewardModel<storm::RationalNumber> const& rewardModel, uint_fast64_t stepCount);
template std::vector<storm::RationalNumber> SparseMdpPrctlHelper<storm::RationalNumber>::computeCumulativeRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalNumber>&& goal, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
    storm::models::sparse::StandardRewardModel<storm::RationalNumber> const& rewardModel, uint_fast64_t stepBound);
template std::vector<storm::RationalNumber> SparseMdpPrctlHelper<storm::RationalNumber>::computeDiscountedCumulativeRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalNumber>&& goal, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
    storm::models::sparse::StandardRewardModel<storm::RationalNumber> const& rewardModel, uint_fast64_t stepBound, storm::RationalNumber discountFactor);
template MDPSparseModelCheckingHelperReturnType<storm::RationalNumber> SparseMdpPrctlHelper<storm::RationalNumber>::computeReachabilityRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalNumber>&& goal, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
    storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
    storm::models::sparse::StandardRewardModel<storm::RationalNumber> const& rewardModel, storm::storage::BitVector const& targetStates, bool qualitative,
    bool produceScheduler, ModelCheckerHint const& hint);
template MDPSparseModelCheckingHelperReturnType<storm::RationalNumber> SparseMdpPrctlHelper<storm::RationalNumber>::computeTotalRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalNumber>&& goal, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
    storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
    storm::models::sparse::StandardRewardModel<storm::RationalNumber> const& rewardModel, bool qualitative, bool produceScheduler,
    ModelCheckerHint const& hint);
template MDPSparseModelCheckingHelperReturnType<storm::RationalNumber> SparseMdpPrctlHelper<storm::RationalNumber>::computeDiscountedTotalRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalNumber>&& goal, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
    storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
    storm::models::sparse::StandardRewardModel<storm::RationalNumber> const& rewardModel, bool qualitative, bool produceScheduler,
    storm::RationalNumber discountFactor, ModelCheckerHint const& hint);

template class SparseMdpPrctlHelper<storm::Interval, double>;
template std::vector<double> SparseMdpPrctlHelper<storm::Interval, double>::computeInstantaneousRewards(
    Environment const& env, storm::solver::SolveGoal<storm::Interval, double>&& goal, storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
    storm::models::sparse::StandardRewardModel<storm::Interval> const& rewardModel, uint_fast64_t stepCount);
template std::vector<double> SparseMdpPrctlHelper<storm::Interval, double>::computeCumulativeRewards(
    Environment const& env, storm::solver::SolveGoal<storm::Interval, double>&& goal, storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
    storm::models::sparse::StandardRewardModel<storm::Interval> const& rewardModel, uint_fast64_t stepBound);
template std::vector<double> SparseMdpPrctlHelper<storm::Interval, double>::computeReachabilityRewards<storm::Interval>(
    Environment const& env, storm::solver::SolveGoal<storm::Interval, double>&& goal, storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
    storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions, storm::models::sparse::StandardRewardModel<storm::Interval> const& rewardModel,
    bool lowerBoundOfIntervals, storm::storage::BitVector const& targetStates, bool qualitative);
template MDPSparseModelCheckingHelperReturnType<double> SparseMdpPrctlHelper<storm::Interval, double>::computeTotalRewards(
    Environment const& env, storm::solver::SolveGoal<storm::Interval, double>&& goal, storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
    storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions, storm::models::sparse::StandardRewardModel<storm::Interval> const& rewardModel,
    bool qualitative, bool produceScheduler, ModelCheckerHint const& hint);

template class SparseMdpPrctlHelper<storm::RationalInterval, storm::RationalNumber>;
template std::vector<storm::RationalNumber> SparseMdpPrctlHelper<storm::RationalInterval, storm::RationalNumber>::computeInstantaneousRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalInterval, storm::RationalNumber>&& goal,
    storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
    storm::models::sparse::StandardRewardModel<storm::RationalInterval> const& rewardModel, uint_fast64_t stepCount);
template std::vector<storm::RationalNumber> SparseMdpPrctlHelper<storm::RationalInterval, storm::RationalNumber>::computeCumulativeRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalInterval, storm::RationalNumber>&& goal,
    storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
    storm::models::sparse::StandardRewardModel<storm::RationalInterval> const& rewardModel, uint_fast64_t stepBound);
template std::vector<storm::RationalNumber> SparseMdpPrctlHelper<storm::RationalInterval, storm::RationalNumber>::computeReachabilityRewards<
    storm::RationalInterval>(Environment const& env, storm::solver::SolveGoal<storm::RationalInterval, storm::RationalNumber>&& goal,
                             storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                             storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                             storm::models::sparse::StandardRewardModel<storm::RationalInterval> const& rewardModel, bool lowerBoundOfIntervals,
                             storm::storage::BitVector const& targetStates, bool qualitative);
template MDPSparseModelCheckingHelperReturnType<storm::RationalNumber>
SparseMdpPrctlHelper<storm::RationalInterval, storm::RationalNumber>::computeTotalRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalInterval, storm::RationalNumber>&& goal,
    storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
    storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
    storm::models::sparse::StandardRewardModel<storm::RationalInterval> const& rewardModel, bool qualitative, bool produceScheduler,
    ModelCheckerHint const& hint);

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
