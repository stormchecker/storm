#include "storm/modelchecker/prctl/helper/SparseDtmcPrctlHelper.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UncheckedRequirementException.h"
#include "storm/io/export.h"
#include "storm/modelchecker/csl/helper/SparseCtmcCslHelper.h"
#include "storm/modelchecker/helper/DiscountingHelper.h"
#include "storm/modelchecker/hints/ExplicitModelCheckerHint.h"
#include "storm/modelchecker/prctl/helper/DsMpiUpperRewardBoundsComputer.h"
#include "storm/modelchecker/prctl/helper/rewardbounded/MultiDimensionalRewardUnfolding.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/ModelCheckerSettings.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/multiplier/Multiplier.h"
#include "storm/storage/ConsecutiveUint64DynamicPriorityQueue.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/utility/ProgressMeasurement.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<>
std::map<storm::storage::sparse::state_type, storm::RationalFunction> SparseDtmcPrctlHelper<storm::RationalFunction>::computeRewardBoundedValues(
    Environment const& /*env*/, storm::models::sparse::Dtmc<storm::RationalFunction> const& /*model*/,
    std::shared_ptr<storm::logic::OperatorFormula const> /*rewardBoundedFormula*/) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "The specified property is not supported by this value type.");
    return std::map<storm::storage::sparse::state_type, storm::RationalFunction>();
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::map<storm::storage::sparse::state_type, SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeRewardBoundedValues(
    Environment const& env, storm::models::sparse::Dtmc<ValueType> const& model, std::shared_ptr<storm::logic::OperatorFormula const> rewardBoundedFormula) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support computing reward bounded values with interval models.");
    } else {
        storm::utility::Stopwatch swAll(true), swBuild, swCheck;

        storm::modelchecker::helper::rewardbounded::MultiDimensionalRewardUnfolding<ValueType, true> rewardUnfolding(model, rewardBoundedFormula);

        // Get lower and upper bounds for the solution.
        auto lowerBound = rewardUnfolding.getLowerObjectiveBound();
        auto upperBound = rewardUnfolding.getUpperObjectiveBound();

        // Initialize epoch models
        auto initEpoch = rewardUnfolding.getStartEpoch();
        auto epochOrder = rewardUnfolding.getEpochComputationOrder(initEpoch);

        // initialize data that will be needed for each epoch
        std::vector<ValueType> x, b;
        std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> linEqSolver;

        Environment preciseEnv = env;
        ValueType precision = rewardUnfolding.getRequiredEpochModelPrecision(
            initEpoch, storm::utility::convertNumber<ValueType>(storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision()));
        preciseEnv.solver().setLinearEquationSolverPrecision(storm::utility::convertNumber<storm::RationalNumber>(precision));

        // In case of cdf export we store the necessary data.
        std::vector<std::vector<ValueType>> cdfData;

        // Set the correct equation problem format.
        storm::solver::GeneralLinearEquationSolverFactory<ValueType> linearEquationSolverFactory;
        rewardUnfolding.setEquationSystemFormatForEpochModel(linearEquationSolverFactory.getEquationProblemFormat(preciseEnv));

        storm::utility::ProgressMeasurement progress("epochs");
        progress.setMaxCount(epochOrder.size());
        progress.startNewMeasurement(0);
        uint64_t numCheckedEpochs = 0;
        for (auto const& epoch : epochOrder) {
            swBuild.start();
            auto& epochModel = rewardUnfolding.setCurrentEpoch(epoch);
            swBuild.stop();
            swCheck.start();
            rewardUnfolding.setSolutionForCurrentEpoch(epochModel.analyzeSingleObjective(preciseEnv, x, b, linEqSolver, lowerBound, upperBound));
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
        for (auto initState : model.getInitialStates()) {
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
std::vector<SolutionType> computeRobustValuesForMaybeStates(Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal,
                                                            storm::storage::SparseMatrix<ValueType>&& submatrix, std::vector<ValueType> const& b,
                                                            bool computeReward) {
    // Initialize the solution vector.
    std::vector<SolutionType> x = std::vector<SolutionType>(submatrix.getRowGroupCount(), storm::utility::zero<SolutionType>());

    // Set up the solver.
    storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType, SolutionType> minMaxLinearEquationSolverFactory;
    std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType, SolutionType>> solver = storm::solver::configureMinMaxLinearEquationSolver(
        env, std::move(goal), minMaxLinearEquationSolverFactory, std::move(submatrix),
        convert(OptimizationDirection::Maximize));  // default to maximize for IDTMCs; does not affect the result
    solver->setUncertaintyResolutionMode(goal.getUncertaintyResolutionMode());
    solver->setHasUniqueSolution(computeReward);  // As we check for graph-preservation, in case of rewards on IDTMCs, we have a unique solution
    solver->setHasNoEndComponents(false);

    // check requirements of solver
    auto req = solver->getRequirements(env);
    if (!computeReward) {
        solver->setLowerBound(storm::utility::zero<SolutionType>());
        solver->setUpperBound(storm::utility::one<SolutionType>());
        req.clearBounds();
    }
    STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                    "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");

    solver->setRequirementsChecked();

    // Solve the corresponding system of equations.
    solver->solveEquations(env, x, b);

    return x;
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeUntilProbabilities(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
    bool qualitative, ModelCheckerHint const& hint) {
    std::vector<SolutionType> result(transitionMatrix.getRowCount(), storm::utility::zero<SolutionType>());

    // We need to identify the maybe states (states which have a probability for satisfying the until formula
    // that is strictly between 0 and 1) and the states that satisfy the formula with probability 1.
    storm::storage::BitVector maybeStates, statesWithProbability1;

    if (hint.isExplicitModelCheckerHint() && hint.template asExplicitModelCheckerHint<ValueType>().getComputeOnlyMaybeStates()) {
        maybeStates = hint.template asExplicitModelCheckerHint<ValueType>().getMaybeStates();

        // Treat the states with probability one
        std::vector<SolutionType> const& resultsForNonMaybeStates = hint.template asExplicitModelCheckerHint<SolutionType>().getResultHint();
        statesWithProbability1 = storm::storage::BitVector(maybeStates.size(), false);
        storm::storage::BitVector nonMaybeStates = ~maybeStates;
        for (auto state : nonMaybeStates) {
            if (storm::utility::isOne(resultsForNonMaybeStates[state])) {
                statesWithProbability1.set(state, true);
                result[state] = storm::utility::one<SolutionType>();
            } else {
                STORM_LOG_THROW(storm::utility::isZero(resultsForNonMaybeStates[state]), storm::exceptions::IllegalArgumentException,
                                "Expected that the result hint specifies probabilities in {0,1} for non-maybe states");
            }
        }

        STORM_LOG_INFO("Preprocessing: " << statesWithProbability1.getNumberOfSetBits() << " states with probability 1 (" << maybeStates.getNumberOfSetBits()
                                         << " states remaining).");
    } else {
        // Get all states that have probability 0 and 1 of satisfying the until-formula.
        std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 =
            storm::utility::graph::performProb01(backwardTransitions, phiStates, psiStates);
        storm::storage::BitVector statesWithProbability0 = std::move(statesWithProbability01.first);
        statesWithProbability1 = std::move(statesWithProbability01.second);
        maybeStates = ~(statesWithProbability0 | statesWithProbability1);

        STORM_LOG_INFO("Preprocessing: " << statesWithProbability1.getNumberOfSetBits() << " states with probability 1, "
                                         << statesWithProbability0.getNumberOfSetBits() << " with probability 0 (" << maybeStates.getNumberOfSetBits()
                                         << " states remaining).");

        // Set values of resulting vector that are known exactly.
        storm::utility::vector::setVectorValues<SolutionType>(result, statesWithProbability0, storm::utility::zero<SolutionType>());
        storm::utility::vector::setVectorValues<SolutionType>(result, statesWithProbability1, storm::utility::one<SolutionType>());
    }

    // Check if the values of the maybe states are relevant for the SolveGoal
    bool maybeStatesNotRelevant = goal.hasRelevantValues() && goal.relevantValues().isDisjointFrom(maybeStates);

    // Check whether we need to compute exact probabilities for some states.
    if (qualitative || maybeStatesNotRelevant) {
        // Set the values for all maybe-states to 0.5 to indicate that their probability values are neither 0 nor 1.
        storm::utility::vector::setVectorValues<SolutionType>(result, maybeStates, storm::utility::convertNumber<SolutionType>(0.5));
    } else {
        if (!maybeStates.empty()) {
            // In this case we have to compute the probabilities.
            if constexpr (storm::IsIntervalType<ValueType>) {
                // Compute probabilities in a robust fashion by using the logic as for MDPs
                storm::storage::SparseMatrix<ValueType> submatrix;
                std::vector<ValueType> b;

                submatrix = transitionMatrix.filterEntries(transitionMatrix.getRowFilter(maybeStates));

                // Prepare the right-hand side of the equation system. For entry i this corresponds to
                // the accumulated probability of going from state i to some state that has probability 1.
                storm::utility::vector::setAllValues(b, transitionMatrix.getRowFilter(statesWithProbability1));

                std::vector<SolutionType> resultForMaybeStates = computeRobustValuesForMaybeStates(env, std::move(goal), std::move(submatrix), b, false);

                // For interval models, the result for maybe states indeed also holds values for all qualitative states.
                STORM_LOG_ASSERT(resultForMaybeStates.size() == transitionMatrix.getColumnCount(), "Dimensions do not match");
                result = std::move(resultForMaybeStates);
            } else {
                // Check whether we need to convert the input to equation system format.
                storm::solver::GeneralLinearEquationSolverFactory<ValueType> linearEquationSolverFactory;
                bool convertToEquationSystem =
                    linearEquationSolverFactory.getEquationProblemFormat(env) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem;

                // We can eliminate the rows and columns from the original transition probability matrix.
                storm::storage::SparseMatrix<ValueType> submatrix = transitionMatrix.getSubmatrix(true, maybeStates, maybeStates, convertToEquationSystem);
                if (convertToEquationSystem) {
                    // Converting the matrix from the fixpoint notation to the form needed for the equation
                    // system. That is, we go from x = A*x + b to (I-A)x = b.
                    submatrix.convertToEquationSystem();
                }

                // Initialize the x vector with the hint (if available) or with 0.5 for each element.
                // This is the initial guess for the iterative solvers. It should be safe as for all
                // 'maybe' states we know that the probability is strictly larger than 0.
                std::vector<SolutionType> x;
                if (hint.isExplicitModelCheckerHint() && hint.template asExplicitModelCheckerHint<ValueType>().hasResultHint()) {
                    x = storm::utility::vector::filterVector(hint.template asExplicitModelCheckerHint<SolutionType>().getResultHint(), maybeStates);
                } else {
                    x = std::vector<SolutionType>(maybeStates.getNumberOfSetBits(), storm::utility::convertNumber<SolutionType>(0.5));
                }

                // Prepare the right-hand side of the equation system. For entry i this corresponds to
                // the accumulated probability of going from state i to some 'yes' state.
                std::vector<SolutionType> b = transitionMatrix.getConstrainedRowSumVector(maybeStates, statesWithProbability1);

                // Now solve the created system of linear equations.
                goal.restrictRelevantValues(maybeStates);
                std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> solver =
                    storm::solver::configureLinearEquationSolver(env, std::move(goal), linearEquationSolverFactory, std::move(submatrix));
                solver->setBounds(storm::utility::zero<ValueType>(), storm::utility::one<ValueType>());
                solver->solveEquations(env, x, b);

                // Set values of resulting vector according to result.
                storm::utility::vector::setVectorValues<SolutionType>(result, maybeStates, x);
            }
        }
    }
    return result;
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeAllUntilProbabilities(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::BitVector const& initialStates, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support computing all until probabilities with interval models.");
    } else {
        uint_fast64_t numberOfStates = transitionMatrix.getRowCount();
        std::vector<SolutionType> result(numberOfStates, storm::utility::zero<SolutionType>());

        // All states are relevant
        storm::storage::BitVector relevantStates(numberOfStates, true);

        // Compute exact probabilities for some states.
        if (!relevantStates.empty()) {
            // Check whether we need to convert the input to equation system format.
            storm::solver::GeneralLinearEquationSolverFactory<ValueType> linearEquationSolverFactory;
            bool convertToEquationSystem =
                linearEquationSolverFactory.getEquationProblemFormat(env) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem;

            storm::storage::SparseMatrix<ValueType> submatrix(transitionMatrix);
            submatrix.makeRowsAbsorbing(phiStates);
            submatrix.makeRowsAbsorbing(psiStates);
            // submatrix.deleteDiagonalEntries(psiStates);
            // storm::storage::BitVector failState(numberOfStates, false);
            // failState.set(0, true);
            submatrix.deleteDiagonalEntries();
            submatrix = submatrix.transpose();
            submatrix = submatrix.getSubmatrix(true, relevantStates, relevantStates, convertToEquationSystem);

            if (convertToEquationSystem) {
                // Converting the matrix from the fixpoint notation to the form needed for the equation
                // system. That is, we go from x = A*x + b to (I-A)x = b.
                submatrix.convertToEquationSystem();
            }

            // Initialize the x vector with 0.5 for each element.
            // This is the initial guess for the iterative solvers. It should be safe as for all
            // 'maybe' states we know that the probability is strictly larger than 0.
            std::vector<SolutionType> x = std::vector<SolutionType>(relevantStates.getNumberOfSetBits(), storm::utility::convertNumber<SolutionType>(0.5));

            // Prepare the right-hand side of the equation system.
            std::vector<SolutionType> b(relevantStates.getNumberOfSetBits(), storm::utility::zero<SolutionType>());
            // Set initial states
            size_t i = 0;
            ValueType initDist = storm::utility::one<ValueType>() / storm::utility::convertNumber<ValueType>(initialStates.getNumberOfSetBits());
            for (auto state : relevantStates) {
                if (initialStates.get(state)) {
                    b[i] = initDist;
                }
                ++i;
            }

            // Now solve the created system of linear equations.
            goal.restrictRelevantValues(relevantStates);
            std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> solver =
                storm::solver::configureLinearEquationSolver(env, std::move(goal), linearEquationSolverFactory, std::move(submatrix));
            solver->setBounds(storm::utility::zero<ValueType>(), storm::utility::one<ValueType>());
            solver->solveEquations(env, x, b);

            // Set values of resulting vector according to result.
            storm::utility::vector::setVectorValues<SolutionType>(result, relevantStates, x);
        }
        return result;
    }
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeGloballyProbabilities(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& psiStates, bool qualitative) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support computing globally probabilities with interval models.");
    } else {
        goal.oneMinus();
        std::vector<SolutionType> result = computeUntilProbabilities(env, std::move(goal), transitionMatrix, backwardTransitions,
                                                                     storm::storage::BitVector(transitionMatrix.getRowCount(), true), ~psiStates, qualitative);
        for (auto& entry : result) {
            entry = storm::utility::one<SolutionType>() - entry;
        }
        return result;
    }
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeNextProbabilities(
    Environment const& env, storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& nextStates) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support next probabilities with interval models.");
    } else {
        // Create the vector with which to multiply and initialize it correctly.
        std::vector<ValueType> result(transitionMatrix.getRowCount());
        storm::utility::vector::setVectorValues(result, nextStates, storm::utility::one<ValueType>());

        // Perform one single matrix-vector multiplication.
        auto multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, transitionMatrix);
        multiplier->multiply(env, result, nullptr, result);
        return result;
    }
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeCumulativeRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    RewardModelType const& rewardModel, uint_fast64_t stepBound) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support cumulative rewards with interval models.");
    } else {
        // Initialize result to the null vector.
        std::vector<ValueType> result(transitionMatrix.getRowCount());

        // Compute the reward vector to add in each step based on the available reward models.
        std::vector<ValueType> totalRewardVector = rewardModel.getTotalRewardVector(transitionMatrix);

        // Perform the matrix vector multiplication as often as required by the formula bound.
        auto multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, transitionMatrix);
        multiplier->repeatedMultiply(env, result, &totalRewardVector, stepBound);

        return result;
    }
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeInstantaneousRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    RewardModelType const& rewardModel, uint_fast64_t stepCount) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support instantaneous rewards with interval models.");
    } else {
        // Only compute the result if the model has a state-based reward this->getModel().
        STORM_LOG_THROW(rewardModel.hasStateRewards(), storm::exceptions::InvalidPropertyException,
                        "Computing instantaneous rewards for a reward model that does not define any state-rewards. The result is trivially 0.");

        // Initialize result to state rewards of the model.
        std::vector<ValueType> result = rewardModel.getStateRewardVector();

        // Perform the matrix vector multiplication as often as required by the formula bound.
        auto multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, transitionMatrix);
        multiplier->repeatedMultiply(env, result, nullptr, stepCount);

        return result;
    }
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeTotalRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, RewardModelType const& rewardModel, bool qualitative, ModelCheckerHint const& hint) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support computing total rewards with interval models.");
    } else {
        // Identify the states from which only states with zero reward are reachable.
        // We can then compute reachability rewards assuming these states as target set.
        storm::storage::BitVector statesWithoutReward = rewardModel.getStatesWithZeroReward(transitionMatrix);
        storm::storage::BitVector rew0States = storm::utility::graph::performProbGreater0(backwardTransitions, statesWithoutReward, ~statesWithoutReward);
        rew0States.complement();
        return computeReachabilityRewards(env, std::move(goal), transitionMatrix, backwardTransitions, rewardModel, rew0States, qualitative, hint);
    }
}

template<>
std::vector<storm::RationalFunction> SparseDtmcPrctlHelper<storm::RationalFunction>::computeDiscountedCumulativeRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalFunction>&& goal,
    storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix,
    storm::models::sparse::StandardRewardModel<storm::RationalFunction> const& rewardModel, uint_fast64_t stepBound, storm::RationalFunction discountFactor) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "The specified property is not supported by this value type.");
    return {};
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeDiscountedCumulativeRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    RewardModelType const& rewardModel, uint_fast64_t stepBound, ValueType discountFactor) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support discounted cumulative rewards with interval models.");
    } else {
        // Only compute the result if the model has at least one reward this->getModel().
        STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");

        // Compute the reward vector to add in each step based on the available reward models.
        std::vector<ValueType> totalRewardVector = rewardModel.getTotalRewardVector(transitionMatrix);

        // Initialize result to the zero vector.
        std::vector<SolutionType> result(transitionMatrix.getRowGroupCount(), storm::utility::zero<SolutionType>());

        auto multiplier = storm::solver::MultiplierFactory<SolutionType>().create(env, transitionMatrix);
        multiplier->repeatedMultiplyWithFactor(env, result, &totalRewardVector, stepBound, discountFactor);

        return result;
    }
}

template<>
std::vector<storm::RationalFunction> SparseDtmcPrctlHelper<storm::RationalFunction>::computeDiscountedTotalRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalFunction>&& goal,
    storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix,
    storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
    storm::models::sparse::StandardRewardModel<storm::RationalFunction> const& rewardModel, bool qualitative, storm::RationalFunction discountFactor,
    ModelCheckerHint const& hint) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "The specified property is not supported by this value type.");
    return {};
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeDiscountedTotalRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, RewardModelType const& rewardModel, bool qualitative, ValueType discountFactor,
    ModelCheckerHint const& hint) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support discounted total rewards with interval models.");
    } else {
        // If the solver is set to force exact results, throw an error if the method is not explicitly set to a value iteration type.
        STORM_LOG_THROW(!env.solver().isForceExact(), storm::exceptions::NotSupportedException,
                        "Exact solving of discounted total reward objectives is currently not supported.");

        // Reduce to reachability rewards
        std::vector<ValueType> b;

        std::vector<ValueType> x = std::vector<ValueType>(transitionMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
        b = rewardModel.getTotalRewardVector(transitionMatrix);
        storm::modelchecker::helper::DiscountingHelper<SolutionType, true> discountingHelper(transitionMatrix, discountFactor);
        discountingHelper.solveWithDiscountedValueIteration(env, std::nullopt, x, b);
        return std::vector<SolutionType>(std::move(x));
    }
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeReachabilityRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, RewardModelType const& rewardModel, storm::storage::BitVector const& targetStates,
    bool qualitative, ModelCheckerHint const& hint) {
    return computeReachabilityRewards(
        env, std::move(goal), transitionMatrix, backwardTransitions,
        [&](uint_fast64_t numberOfRows, storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& maybeStates) {
            return rewardModel.getTotalRewardVector(numberOfRows, transitionMatrix, maybeStates);
        },
        targetStates, qualitative, [&]() { return rewardModel.getStatesWithZeroReward(transitionMatrix); }, hint);
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeReachabilityRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, std::vector<ValueType> const& totalStateRewardVector,
    storm::storage::BitVector const& targetStates, bool qualitative, ModelCheckerHint const& hint) {
    return computeReachabilityRewards(
        env, std::move(goal), transitionMatrix, backwardTransitions,
        [&](uint_fast64_t numberOfRows, storm::storage::SparseMatrix<ValueType> const&, storm::storage::BitVector const& maybeStates) {
            std::vector<ValueType> result(numberOfRows, storm::utility::zero<ValueType>());
            storm::utility::vector::selectVectorValues(result, maybeStates, totalStateRewardVector);
            return result;
        },
        targetStates, qualitative, [&]() { return storm::utility::vector::filterZero(totalStateRewardVector); }, hint);
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeReachabilityTimes(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& targetStates, bool qualitative,
    ModelCheckerHint const& hint) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support computing reachability times with interval models.");
    } else {
        return computeReachabilityRewards(
            env, std::move(goal), transitionMatrix, backwardTransitions,
            [&](uint_fast64_t numberOfRows, storm::storage::SparseMatrix<ValueType> const&, storm::storage::BitVector const&) {
                return std::vector<ValueType>(numberOfRows, storm::utility::one<ValueType>());
            },
            targetStates, qualitative, [&]() { return storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false); }, hint);
    }
}

// This function computes an upper bound on the reachability rewards (see Baier et al, CAV'17).
template<typename ValueType, typename SolutionType>
std::vector<SolutionType> computeUpperRewardBounds(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType> const& rewards,
                                                   std::vector<SolutionType> const& oneStepTargetProbabilities) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support computing upper reward bounds with interval models.");
    } else {
        DsMpiDtmcUpperRewardBoundsComputer<ValueType> dsmpi(transitionMatrix, rewards, oneStepTargetProbabilities);
        std::vector<ValueType> bounds = dsmpi.computeUpperBounds();
        return bounds;
    }
}

template<>
std::vector<storm::RationalFunction> computeUpperRewardBounds(storm::storage::SparseMatrix<storm::RationalFunction> const& /*transitionMatrix*/,
                                                              std::vector<storm::RationalFunction> const& /*rewards*/,
                                                              std::vector<storm::RationalFunction> const& /*oneStepTargetProbabilities*/) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing upper reward bounds is not supported for rational functions.");
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeReachabilityRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
    std::function<std::vector<ValueType>(uint_fast64_t, storm::storage::SparseMatrix<ValueType> const&, storm::storage::BitVector const&)> const&
        totalStateRewardVectorGetter,
    storm::storage::BitVector const& targetStates, bool qualitative, std::function<storm::storage::BitVector()> const& zeroRewardStatesGetter,
    ModelCheckerHint const& hint) {
    std::vector<SolutionType> result(transitionMatrix.getRowCount(), storm::utility::zero<SolutionType>());

    // Determine which states have reward zero
    storm::storage::BitVector rew0States;
    if (storm::settings::getModule<storm::settings::modules::ModelCheckerSettings>().isFilterRewZeroSet()) {
        rew0States = storm::utility::graph::performProb1(backwardTransitions, zeroRewardStatesGetter(), targetStates);
    } else {
        rew0States = targetStates;
    }

    // Determine which states have a reward that is less than infinity.
    storm::storage::BitVector maybeStates;
    if (hint.isExplicitModelCheckerHint() && hint.template asExplicitModelCheckerHint<ValueType>().getComputeOnlyMaybeStates()) {
        maybeStates = hint.template asExplicitModelCheckerHint<ValueType>().getMaybeStates();
        storm::utility::vector::setVectorValues(result, ~(maybeStates | rew0States), storm::utility::infinity<SolutionType>());

        STORM_LOG_INFO("Preprocessing: " << rew0States.getNumberOfSetBits() << " States with reward zero (" << maybeStates.getNumberOfSetBits()
                                         << " states remaining).");
    } else {
        storm::storage::BitVector trueStates(transitionMatrix.getRowCount(), true);
        storm::storage::BitVector infinityStates = storm::utility::graph::performProb1(backwardTransitions, trueStates, rew0States);
        infinityStates.complement();
        maybeStates = ~(rew0States | infinityStates);

        STORM_LOG_INFO("Preprocessing: " << infinityStates.getNumberOfSetBits() << " states with reward infinity, " << rew0States.getNumberOfSetBits()
                                         << " states with reward zero (" << maybeStates.getNumberOfSetBits() << " states remaining).");

        storm::utility::vector::setVectorValues(result, infinityStates, storm::utility::infinity<SolutionType>());
    }

    // Check if the values of the maybe states are relevant for the SolveGoal
    bool maybeStatesNotRelevant = goal.hasRelevantValues() && goal.relevantValues().isDisjointFrom(maybeStates);

    // Check whether we need to compute exact rewards for some states.
    if (qualitative || maybeStatesNotRelevant) {
        // Set the values for all maybe-states to 1 to indicate that their reward values
        // are neither 0 nor infinity.
        storm::utility::vector::setVectorValues<SolutionType>(result, maybeStates, storm::utility::one<SolutionType>());
    } else {
        if (!maybeStates.empty()) {
            if constexpr (storm::IsIntervalType<ValueType>) {
                // In this case we have to compute the reward values for the remaining states.
                // We can eliminate the rows and columns from the original transition probability matrix.
                storm::storage::SparseMatrix<ValueType> submatrix = transitionMatrix.filterEntries(transitionMatrix.getRowFilter(maybeStates));

                // Prepare the right-hand side of the equation system.
                std::vector<ValueType> b = totalStateRewardVectorGetter(submatrix.getRowCount(), transitionMatrix, maybeStates);

                // Compute values for maybe states.
                std::vector<SolutionType> x = computeRobustValuesForMaybeStates(env, std::move(goal), std::move(submatrix), b, true);

                // Set values of resulting vector according to result.
                storm::utility::vector::setVectorValues<SolutionType>(result, maybeStates, x);
            } else {
                // Check whether we need to convert the input to equation system format.
                storm::solver::GeneralLinearEquationSolverFactory<ValueType> linearEquationSolverFactory;
                bool convertToEquationSystem =
                    linearEquationSolverFactory.getEquationProblemFormat(env) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem;

                // In this case we have to compute the reward values for the remaining states.
                // We can eliminate the rows and columns from the original transition probability matrix.
                storm::storage::SparseMatrix<ValueType> submatrix = transitionMatrix.getSubmatrix(true, maybeStates, maybeStates, convertToEquationSystem);

                // Initialize the x vector with the hint (if available) or with 1 for each element.
                // This is the initial guess for the iterative solvers.
                std::vector<ValueType> x;
                if (hint.isExplicitModelCheckerHint() && hint.template asExplicitModelCheckerHint<ValueType>().hasResultHint()) {
                    x = storm::utility::vector::filterVector(hint.template asExplicitModelCheckerHint<ValueType>().getResultHint(), maybeStates);
                } else {
                    x = std::vector<ValueType>(submatrix.getColumnCount(), storm::utility::one<ValueType>());
                }

                // Prepare the right-hand side of the equation system.
                std::vector<ValueType> b = totalStateRewardVectorGetter(submatrix.getRowCount(), transitionMatrix, maybeStates);

                storm::solver::LinearEquationSolverRequirements requirements = linearEquationSolverFactory.getRequirements(env);
                boost::optional<std::vector<ValueType>> upperRewardBounds;
                requirements.clearLowerBounds();
                if (requirements.upperBounds()) {
                    upperRewardBounds = computeUpperRewardBounds(submatrix, b, transitionMatrix.getConstrainedRowSumVector(maybeStates, rew0States));
                    requirements.clearUpperBounds();
                }
                STORM_LOG_THROW(!requirements.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                                "Solver requirements " + requirements.getEnabledRequirementsAsString() + " not checked.");

                // If necessary, convert the matrix from the fixpoint notation to the form needed for the equation system.
                if (convertToEquationSystem) {
                    // go from x = A*x + b to (I-A)x = b.
                    submatrix.convertToEquationSystem();
                }

                // Create the solver.
                goal.restrictRelevantValues(maybeStates);
                std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> solver =
                    storm::solver::configureLinearEquationSolver(env, std::move(goal), linearEquationSolverFactory, std::move(submatrix));
                solver->setLowerBound(storm::utility::zero<ValueType>());
                if (upperRewardBounds) {
                    solver->setUpperBounds(std::move(upperRewardBounds.get()));
                }

                // Now solve the resulting equation system.
                solver->solveEquations(env, x, b);

                // Set values of resulting vector according to result.
                storm::utility::vector::setVectorValues<SolutionType>(result, maybeStates, x);
            }
        }
    }
    return result;
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
typename SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::BaierTransformedModel
SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeBaierTransformation(Environment const& env,
                                                                                            storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                            storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                            storm::storage::BitVector const& targetStates,
                                                                                            storm::storage::BitVector const& conditionStates,
                                                                                            boost::optional<std::vector<ValueType>> const& stateRewards) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support baier transformation with interval models.");
    } else {
        BaierTransformedModel result;

        // Start by computing all 'before' states, i.e. the states for which the conditional probability is defined.
        std::vector<ValueType> probabilitiesToReachConditionStates =
            computeUntilProbabilities(env, storm::solver::SolveGoal<ValueType>(), transitionMatrix, backwardTransitions,
                                      storm::storage::BitVector(transitionMatrix.getRowCount(), true), conditionStates, false);

        result.beforeStates = storm::storage::BitVector(targetStates.size(), true);
        uint_fast64_t state = 0;
        uint_fast64_t beforeStateIndex = 0;
        for (auto const& value : probabilitiesToReachConditionStates) {
            if (value == storm::utility::zero<ValueType>()) {
                result.beforeStates.set(state, false);
            } else {
                probabilitiesToReachConditionStates[beforeStateIndex] = value;
                ++beforeStateIndex;
            }
            ++state;
        }
        probabilitiesToReachConditionStates.resize(beforeStateIndex);

        if (targetStates.empty()) {
            result.noTargetStates = true;
            return result;
        } else if (!result.beforeStates.empty()) {
            // If there are some states for which the conditional probability is defined and there are some
            // states that can reach the target states without visiting condition states first, we need to
            // do more work.

            // First, compute the relevant states and some offsets.
            storm::storage::BitVector allStates(targetStates.size(), true);
            std::vector<uint_fast64_t> numberOfBeforeStatesUpToState = result.beforeStates.getNumberOfSetBitsBeforeIndices();
            storm::storage::BitVector statesWithProbabilityGreater0 = storm::utility::graph::performProbGreater0(backwardTransitions, allStates, targetStates);
            statesWithProbabilityGreater0 &= storm::utility::graph::getReachableStates(transitionMatrix, conditionStates, allStates, targetStates);
            uint_fast64_t normalStatesOffset = result.beforeStates.getNumberOfSetBits();
            std::vector<uint_fast64_t> numberOfNormalStatesUpToState = statesWithProbabilityGreater0.getNumberOfSetBitsBeforeIndices();

            // All transitions going to states with probability zero, need to be redirected to a deadlock state.
            bool addDeadlockState = false;
            uint_fast64_t deadlockState = normalStatesOffset + statesWithProbabilityGreater0.getNumberOfSetBits();

            // Now, we create the matrix of 'before' and 'normal' states.
            storm::storage::SparseMatrixBuilder<ValueType> builder;

            // Start by creating the transitions of the 'before' states.
            uint_fast64_t currentRow = 0;
            for (auto beforeState : result.beforeStates) {
                if (conditionStates.get(beforeState)) {
                    // For condition states, we move to the 'normal' states.
                    ValueType zeroProbability = storm::utility::zero<ValueType>();
                    for (auto const& successorEntry : transitionMatrix.getRow(beforeState)) {
                        if (statesWithProbabilityGreater0.get(successorEntry.getColumn())) {
                            builder.addNextValue(currentRow, normalStatesOffset + numberOfNormalStatesUpToState[successorEntry.getColumn()],
                                                 successorEntry.getValue());
                        } else {
                            zeroProbability += successorEntry.getValue();
                        }
                    }
                    if (!storm::utility::isZero(zeroProbability)) {
                        builder.addNextValue(currentRow, deadlockState, zeroProbability);
                    }
                } else {
                    // For non-condition states, we scale the probabilities going to other before states.
                    for (auto const& successorEntry : transitionMatrix.getRow(beforeState)) {
                        if (result.beforeStates.get(successorEntry.getColumn())) {
                            builder.addNextValue(currentRow, numberOfBeforeStatesUpToState[successorEntry.getColumn()],
                                                 successorEntry.getValue() *
                                                     probabilitiesToReachConditionStates[numberOfBeforeStatesUpToState[successorEntry.getColumn()]] /
                                                     probabilitiesToReachConditionStates[currentRow]);
                        }
                    }
                }
                ++currentRow;
            }

            // Then, create the transitions of the 'normal' states.
            for (auto state : statesWithProbabilityGreater0) {
                ValueType zeroProbability = storm::utility::zero<ValueType>();
                for (auto const& successorEntry : transitionMatrix.getRow(state)) {
                    if (statesWithProbabilityGreater0.get(successorEntry.getColumn())) {
                        builder.addNextValue(currentRow, normalStatesOffset + numberOfNormalStatesUpToState[successorEntry.getColumn()],
                                             successorEntry.getValue());
                    } else {
                        zeroProbability += successorEntry.getValue();
                    }
                }
                if (!storm::utility::isZero(zeroProbability)) {
                    addDeadlockState = true;
                    builder.addNextValue(currentRow, deadlockState, zeroProbability);
                }
                ++currentRow;
            }
            if (addDeadlockState) {
                builder.addNextValue(deadlockState, deadlockState, storm::utility::one<ValueType>());
            }

            // Build the new transition matrix and the new targets.
            result.transitionMatrix = builder.build(addDeadlockState ? (deadlockState + 1) : deadlockState);
            storm::storage::BitVector newTargetStates = targetStates % result.beforeStates;
            newTargetStates.resize(result.transitionMatrix.get().getRowCount());
            for (auto state : targetStates % statesWithProbabilityGreater0) {
                newTargetStates.set(normalStatesOffset + state, true);
            }
            result.targetStates = std::move(newTargetStates);

            // If a reward model was given, we need to compute the rewards for the transformed model.
            if (stateRewards) {
                std::vector<ValueType> newStateRewards(result.beforeStates.getNumberOfSetBits());
                storm::utility::vector::selectVectorValues(newStateRewards, result.beforeStates, stateRewards.get());

                newStateRewards.reserve(result.transitionMatrix.get().getRowCount());
                for (auto state : statesWithProbabilityGreater0) {
                    newStateRewards.push_back(stateRewards.get()[state]);
                }
                // Add a zero reward to the deadlock state.
                if (addDeadlockState) {
                    newStateRewards.push_back(storm::utility::zero<ValueType>());
                }
                result.stateRewards = std::move(newStateRewards);
            }
        }

        return result;
    }
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
storm::storage::BitVector SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::BaierTransformedModel::getNewRelevantStates() const {
    storm::storage::BitVector newRelevantStates(transitionMatrix.get().getRowCount());
    for (uint64_t i = 0; i < this->beforeStates.getNumberOfSetBits(); ++i) {
        newRelevantStates.set(i);
    }
    return newRelevantStates;
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
storm::storage::BitVector SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::BaierTransformedModel::getNewRelevantStates(
    storm::storage::BitVector const& oldRelevantStates) const {
    storm::storage::BitVector result = oldRelevantStates % this->beforeStates;
    result.resize(transitionMatrix.get().getRowCount());
    return result;
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeConditionalProbabilities(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& targetStates,
    storm::storage::BitVector const& conditionStates, bool qualitative) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support computing conditional probabilities with interval models.");
    } else {
        // Prepare result vector.
        std::vector<ValueType> result(transitionMatrix.getRowCount(), storm::utility::infinity<ValueType>());

        if (!conditionStates.empty()) {
            BaierTransformedModel transformedModel =
                computeBaierTransformation(env, transitionMatrix, backwardTransitions, targetStates, conditionStates, boost::none);

            if (transformedModel.noTargetStates) {
                storm::utility::vector::setVectorValues(result, transformedModel.beforeStates, storm::utility::zero<ValueType>());
            } else {
                // At this point, we do not need to check whether there are 'before' states, since the condition
                // states were non-empty so there is at least one state with a positive probability of satisfying
                // the condition.

                // Now compute reachability probabilities in the transformed model.
                storm::storage::SparseMatrix<ValueType> const& newTransitionMatrix = transformedModel.transitionMatrix.get();
                storm::storage::BitVector newRelevantValues;
                if (goal.hasRelevantValues()) {
                    newRelevantValues = transformedModel.getNewRelevantStates(goal.relevantValues());
                } else {
                    newRelevantValues = transformedModel.getNewRelevantStates();
                }
                goal.setRelevantValues(std::move(newRelevantValues));
                std::vector<ValueType> conditionalProbabilities = computeUntilProbabilities(
                    env, std::move(goal), newTransitionMatrix, newTransitionMatrix.transpose(),
                    storm::storage::BitVector(newTransitionMatrix.getRowCount(), true), transformedModel.targetStates.get(), qualitative);

                storm::utility::vector::setVectorValues(result, transformedModel.beforeStates, conditionalProbabilities);
            }
        }

        return result;
    }
}

template<typename ValueType, typename RewardModelType, typename SolutionType>
std::vector<SolutionType> SparseDtmcPrctlHelper<ValueType, RewardModelType, SolutionType>::computeConditionalRewards(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, RewardModelType const& rewardModel, storm::storage::BitVector const& targetStates,
    storm::storage::BitVector const& conditionStates, bool qualitative) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We do not support computing conditional rewards with interval models.");
    } else {
        // Prepare result vector.
        std::vector<ValueType> result(transitionMatrix.getRowCount(), storm::utility::infinity<ValueType>());

        if (!conditionStates.empty()) {
            BaierTransformedModel transformedModel = computeBaierTransformation(env, transitionMatrix, backwardTransitions, targetStates, conditionStates,
                                                                                rewardModel.getTotalRewardVector(transitionMatrix));

            if (transformedModel.noTargetStates) {
                storm::utility::vector::setVectorValues(result, transformedModel.beforeStates, storm::utility::zero<ValueType>());
            } else {
                // At this point, we do not need to check whether there are 'before' states, since the condition
                // states were non-empty so there is at least one state with a positive probability of satisfying
                // the condition.

                // Now compute reachability probabilities in the transformed model.
                storm::storage::SparseMatrix<ValueType> const& newTransitionMatrix = transformedModel.transitionMatrix.get();
                storm::storage::BitVector newRelevantValues;
                if (goal.hasRelevantValues()) {
                    newRelevantValues = transformedModel.getNewRelevantStates(goal.relevantValues());
                } else {
                    newRelevantValues = transformedModel.getNewRelevantStates();
                }
                goal.setRelevantValues(std::move(newRelevantValues));
                std::vector<ValueType> conditionalRewards =
                    computeReachabilityRewards(env, std::move(goal), newTransitionMatrix, newTransitionMatrix.transpose(), transformedModel.stateRewards.get(),
                                               transformedModel.targetStates.get(), qualitative);
                storm::utility::vector::setVectorValues(result, transformedModel.beforeStates, conditionalRewards);
            }
        }

        return result;
    }
}

template class SparseDtmcPrctlHelper<double>;

template class SparseDtmcPrctlHelper<storm::RationalNumber>;
template class SparseDtmcPrctlHelper<storm::RationalFunction>;
template class SparseDtmcPrctlHelper<storm::Interval, storm::models::sparse::StandardRewardModel<storm::Interval>, double>;
template class SparseDtmcPrctlHelper<storm::RationalInterval, storm::models::sparse::StandardRewardModel<storm::RationalInterval>, storm::RationalNumber>;
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
