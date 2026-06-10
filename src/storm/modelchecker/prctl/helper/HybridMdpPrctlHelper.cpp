#include "storm/modelchecker/prctl/helper/HybridMdpPrctlHelper.h"

#include "storm/modelchecker/prctl/helper/SymbolicMdpPrctlHelper.h"

#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/dd/Odd.h"

#include "storm/utility/constants.h"
#include "storm/utility/graph.h"

#include "storm/models/symbolic/StandardRewardModel.h"

#include "storm/modelchecker/prctl/helper/BaierUpperRewardBoundsComputer.h"
#include "storm/modelchecker/prctl/helper/DsMpiUpperRewardBoundsComputer.h"
#include "storm/modelchecker/prctl/helper/SparseMdpEndComponentInformation.h"
#include "storm/modelchecker/results/HybridQuantitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQuantitativeCheckResult.h"

#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/solver/multiplier/Multiplier.h"

#include "storm/utility/Stopwatch.h"

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/UncheckedRequirementException.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType>
struct SolverRequirementsData {
    boost::optional<SparseMdpEndComponentInformation<ValueType>> ecInformation;
    boost::optional<std::vector<uint64_t>> initialScheduler;
    storm::storage::BitVector properMaybeStates;
    boost::optional<std::vector<ValueType>> oneStepTargetProbabilities;
};

template<typename ValueType>
std::vector<uint64_t> computeValidInitialSchedulerForUntilProbabilities(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                        std::vector<ValueType> const& b) {
    uint64_t numberOfMaybeStates = transitionMatrix.getRowGroupCount();
    std::vector<uint64_t> result(numberOfMaybeStates);
    storm::storage::BitVector targetStates(numberOfMaybeStates);

    for (uint64_t state = 0; state < numberOfMaybeStates; ++state) {
        // Record all states with non-zero probability of moving directly to the target states.
        for (uint64_t row = transitionMatrix.getRowGroupIndices()[state]; row < transitionMatrix.getRowGroupIndices()[state + 1]; ++row) {
            if (!storm::utility::isZero(b[row])) {
                targetStates.set(state);
                result[state] = row - transitionMatrix.getRowGroupIndices()[state];
            }
        }
    }

    if (!targetStates.full()) {
        storm::storage::Scheduler<ValueType> validScheduler(numberOfMaybeStates);
        storm::storage::SparseMatrix<ValueType> backwardTransitions = transitionMatrix.transpose(true);
        storm::utility::graph::computeSchedulerProbGreater0E(transitionMatrix, backwardTransitions, storm::storage::BitVector(numberOfMaybeStates, true),
                                                             targetStates, validScheduler, boost::none);

        for (uint64_t state = 0; state < numberOfMaybeStates; ++state) {
            if (!targetStates.get(state)) {
                result[state] = validScheduler.getChoice(state).getDeterministicChoice();
            }
        }
    }

    return result;
}

template<typename ValueType>
void eliminateExtendedStatesFromExplicitRepresentation(std::pair<storm::storage::SparseMatrix<ValueType>, std::vector<ValueType>>& explicitRepresentation,
                                                       boost::optional<std::vector<uint64_t>>& scheduler, storm::storage::BitVector const& properMaybeStates) {
    if (scheduler) {
        // Eliminate superfluous entries from the scheduler.
        uint64_t position = 0;
        for (auto state : properMaybeStates) {
            scheduler.get()[position] = scheduler.get()[state];
            position++;
        }
        scheduler.get().resize(properMaybeStates.getNumberOfSetBits());
        scheduler.get().shrink_to_fit();
    }

    // Treat the matrix.
    explicitRepresentation.first = explicitRepresentation.first.getSubmatrix(true, properMaybeStates, properMaybeStates);
}

template<typename ValueType>
void eliminateEndComponentsAndExtendedStatesUntilProbabilities(
    std::pair<storm::storage::SparseMatrix<ValueType>, std::vector<ValueType>>& explicitRepresentation,
    SolverRequirementsData<ValueType>& solverRequirementsData, storm::storage::BitVector const& targetStates) {
    // Get easier handles to the data.
    auto& transitionMatrix = explicitRepresentation.first;
    auto& oneStepProbabilities = explicitRepresentation.second;

    bool doDecomposition = !solverRequirementsData.properMaybeStates.empty();

    storm::storage::MaximalEndComponentDecomposition<ValueType> endComponentDecomposition;
    if (doDecomposition) {
        auto backwardTransitions = transitionMatrix.transpose(true);
        // Get the set of states that (under some scheduler) can stay in the set of maybestates forever
        storm::storage::BitVector candidateStates =
            storm::utility::graph::performProb0E(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions,
                                                 solverRequirementsData.properMaybeStates, ~solverRequirementsData.properMaybeStates);

        doDecomposition = !candidateStates.empty();

        if (doDecomposition) {
            // If there are candidates, compute the states that are in MECs with zero reward.
            endComponentDecomposition = storm::storage::MaximalEndComponentDecomposition<ValueType>(transitionMatrix, backwardTransitions, candidateStates);
        }
    }

    // Only do more work if there are actually end-components.
    if (doDecomposition && !endComponentDecomposition.empty()) {
        STORM_LOG_DEBUG("Eliminating " << endComponentDecomposition.size() << " EC(s).");
        std::vector<ValueType> subvector;
        SparseMdpEndComponentInformation<ValueType>::eliminateEndComponents(endComponentDecomposition, transitionMatrix,
                                                                            solverRequirementsData.properMaybeStates, &targetStates, nullptr, nullptr,
                                                                            explicitRepresentation.first, &subvector, nullptr);
        oneStepProbabilities = std::move(subvector);
    } else {
        STORM_LOG_DEBUG("Not eliminating ECs as there are none.");
        oneStepProbabilities = explicitRepresentation.first.getConstrainedRowGroupSumVector(solverRequirementsData.properMaybeStates, targetStates);
        eliminateExtendedStatesFromExplicitRepresentation(explicitRepresentation, solverRequirementsData.initialScheduler,
                                                          solverRequirementsData.properMaybeStates);
    }
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeUntilProbabilities(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
    storm::dd::Add<DdType, ValueType> const& transitionMatrix, storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates,
    bool qualitative) {
    // We need to identify the states which have to be taken out of the matrix, i.e. all states that have
    // probability 0 and 1 of satisfying the until-formula.
    storm::dd::Bdd<DdType> transitionMatrixBdd = transitionMatrix.notZero();
    std::pair<storm::dd::Bdd<DdType>, storm::dd::Bdd<DdType>> statesWithProbability01;
    if (dir == OptimizationDirection::Minimize) {
        statesWithProbability01 = storm::utility::graph::performProb01Min(model, transitionMatrixBdd, phiStates, psiStates);
    } else {
        statesWithProbability01 = storm::utility::graph::performProb01Max(model, transitionMatrixBdd, phiStates, psiStates);
    }
    storm::dd::Bdd<DdType> maybeStates = !statesWithProbability01.first && !statesWithProbability01.second && model.getReachableStates();

    STORM_LOG_INFO("Preprocessing: " << statesWithProbability01.first.getNonZeroCount() << " states with probability 1, "
                                     << statesWithProbability01.second.getNonZeroCount() << " with probability 0 (" << maybeStates.getNonZeroCount()
                                     << " states remaining).");

    // Check whether we need to compute exact probabilities for some states.
    if (qualitative) {
        // Set the values for all maybe-states to 0.5 to indicate that their probability values are neither 0 nor 1.
        return std::unique_ptr<CheckResult>(new storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>(
            model.getReachableStates(),
            statesWithProbability01.second.template toAdd<ValueType>() +
                maybeStates.template toAdd<ValueType>() * model.getManager().getConstant(storm::utility::convertNumber<ValueType>(0.5))));
    } else {
        // If there are maybe states, we need to solve an equation system.
        if (!maybeStates.isZero()) {
            // If we minimize, we know that the solution to the equation system has no end components
            bool hasNoEndComponents = dir == storm::solver::OptimizationDirection::Minimize;
            // Check for requirements of the solver early so we can adjust the maybe state computation accordingly.
            storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType> linearEquationSolverFactory;
            storm::solver::MinMaxLinearEquationSolverRequirements requirements =
                linearEquationSolverFactory.getRequirements(env, hasNoEndComponents, hasNoEndComponents, dir);
            storm::solver::MinMaxLinearEquationSolverRequirements clearedRequirements = requirements;
            SolverRequirementsData<ValueType> solverRequirementsData;
            bool extendMaybeStates = false;

            if (clearedRequirements.hasEnabledRequirement()) {
                if (clearedRequirements.uniqueSolution()) {
                    STORM_LOG_DEBUG("Scheduling EC elimination, because the solver requires a unique solution.");
                    extendMaybeStates = true;
                    clearedRequirements.clearUniqueSolution();
                    hasNoEndComponents = true;
                }
                if (clearedRequirements.validInitialScheduler() && !hasNoEndComponents) {
                    STORM_LOG_DEBUG("Scheduling valid scheduler computation, because the solver requires it.");
                    clearedRequirements.clearValidInitialScheduler();
                }
                clearedRequirements.clearBounds();
                STORM_LOG_THROW(!clearedRequirements.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                                "Solver requirements " + clearedRequirements.getEnabledRequirementsAsString() + " not checked.");
            }

            storm::dd::Bdd<DdType> extendedMaybeStates = maybeStates;
            if (extendMaybeStates) {
                // Extend the maybe states by all non-maybe states that can be reached from a maybe state within one step (they
                // either are states with probability 0 or 1).
                extendedMaybeStates |= maybeStates.relationalProduct(transitionMatrixBdd.existsAbstract(model.getNondeterminismVariables()),
                                                                     model.getRowVariables(), model.getColumnVariables());
            }

            storm::utility::Stopwatch conversionWatch;

            // Create the ODD for the translation between symbolic and explicit storage.
            conversionWatch.start();
            storm::dd::Odd odd = extendedMaybeStates.createOdd();
            conversionWatch.stop();

            // Convert the maybe states BDD to an ADD.
            storm::dd::Add<DdType, ValueType> maybeStatesAdd = maybeStates.template toAdd<ValueType>();

            // Start by cutting away all rows that do not belong to maybe states. Note that this leaves columns targeting
            // non-maybe states in the matrix.
            storm::dd::Add<DdType, ValueType> submatrix = transitionMatrix * maybeStatesAdd;

            // If the maybe states were extended, we generate the explicit representation slightly differently.
            std::pair<storm::storage::SparseMatrix<ValueType>, std::vector<ValueType>> explicitRepresentation;
            if (extendMaybeStates) {
                // Eliminate all transitions to non-extended-maybe states.
                submatrix *= extendedMaybeStates.template toAdd<ValueType>().swapVariables(model.getRowColumnMetaVariablePairs());

                // Only translate the matrix for now.
                conversionWatch.start();
                explicitRepresentation.first = submatrix.toMatrix(model.getNondeterminismVariables(), odd, odd);

                // Get all original maybe states in the extended matrix.
                solverRequirementsData.properMaybeStates = maybeStates.toVector(odd);

                // Compute the target states within the set of extended maybe states.
                storm::storage::BitVector targetStates = (extendedMaybeStates && statesWithProbability01.second).toVector(odd);
                conversionWatch.stop();

                // Eliminate the end components and remove the states that are not interesting (target or non-filter).
                eliminateEndComponentsAndExtendedStatesUntilProbabilities(explicitRepresentation, solverRequirementsData, targetStates);

            } else {
                // Then compute the vector that contains the one-step probabilities to a state with probability 1 for all
                // maybe states.
                storm::dd::Add<DdType, ValueType> prob1StatesAsColumn = statesWithProbability01.second.template toAdd<ValueType>();
                prob1StatesAsColumn = prob1StatesAsColumn.swapVariables(model.getRowColumnMetaVariablePairs());
                storm::dd::Add<DdType, ValueType> subvector = submatrix * prob1StatesAsColumn;
                subvector = subvector.sumAbstract(model.getColumnVariables());

                // Finally cut away all columns targeting non-maybe states.
                submatrix *= maybeStatesAdd.swapVariables(model.getRowColumnMetaVariablePairs());

                // Translate the symbolic matrix/vector to their explicit representations and solve the equation system.
                conversionWatch.start();
                explicitRepresentation = submatrix.toMatrixVector(subvector, model.getNondeterminismVariables(), odd, odd);
                conversionWatch.stop();

                if (requirements.validInitialScheduler()) {
                    solverRequirementsData.initialScheduler =
                        computeValidInitialSchedulerForUntilProbabilities<ValueType>(explicitRepresentation.first, explicitRepresentation.second);
                }
            }

            STORM_LOG_INFO("Converting symbolic matrix/vector to explicit representation done in " << conversionWatch.getTimeInMilliseconds() << "ms.");

            // Create the solution vector.
            std::vector<ValueType> x(explicitRepresentation.first.getRowGroupCount(), storm::utility::zero<ValueType>());

            std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> solver =
                linearEquationSolverFactory.create(env, std::move(explicitRepresentation.first));

            // Set whether the equation system will have a unique solution / no end components
            solver->setHasUniqueSolution(hasNoEndComponents);
            solver->setHasNoEndComponents(hasNoEndComponents);

            if (solverRequirementsData.initialScheduler) {
                solver->setInitialScheduler(std::move(solverRequirementsData.initialScheduler.get()));
            }
            solver->setBounds(storm::utility::zero<ValueType>(), storm::utility::one<ValueType>());
            solver->setRequirementsChecked();
            solver->solveEquations(env, dir, x, explicitRepresentation.second);

            // If we included some target and non-filter states in the ODD, we need to expand the result from the solver.
            if (requirements.uniqueSolution() && solverRequirementsData.ecInformation) {
                std::vector<ValueType> extendedVector(solverRequirementsData.properMaybeStates.getNumberOfSetBits());
                solverRequirementsData.ecInformation.get().setValues(extendedVector, solverRequirementsData.properMaybeStates, x);
                x = std::move(extendedVector);
            }

            // If we extended the maybe states, we create a new ODD containing only the propery maybe states.
            if (extendMaybeStates) {
                odd = maybeStates.createOdd();
            }

            // Return a hybrid check result that stores the numerical values explicitly.
            return std::unique_ptr<CheckResult>(new storm::modelchecker::HybridQuantitativeCheckResult<DdType, ValueType>(
                model.getReachableStates(), model.getReachableStates() && !maybeStates, statesWithProbability01.second.template toAdd<ValueType>(), maybeStates,
                odd, x));
        } else {
            return std::unique_ptr<CheckResult>(new storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>(
                model.getReachableStates(), statesWithProbability01.second.template toAdd<ValueType>()));
        }
    }
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeGloballyProbabilities(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
    storm::dd::Add<DdType, ValueType> const& transitionMatrix, storm::dd::Bdd<DdType> const& psiStates, bool qualitative) {
    std::unique_ptr<CheckResult> result =
        computeUntilProbabilities(env, dir == OptimizationDirection::Minimize ? OptimizationDirection::Maximize : OptimizationDirection::Maximize, model,
                                  transitionMatrix, model.getReachableStates(), !psiStates && model.getReachableStates(), qualitative);
    result->asQuantitativeCheckResult<ValueType>().oneMinus();
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeNextProbabilities(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
    storm::dd::Add<DdType, ValueType> const& transitionMatrix, storm::dd::Bdd<DdType> const& nextStates) {
    return SymbolicMdpPrctlHelper<DdType, ValueType>::computeNextProbabilities(env, dir, model, transitionMatrix, nextStates);
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeBoundedUntilProbabilities(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
    storm::dd::Add<DdType, ValueType> const& transitionMatrix, storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates,
    uint_fast64_t stepBound) {
    // We need to identify the states which have to be taken out of the matrix, i.e. all states that have
    // probability 0 or 1 of satisfying the until-formula.
    storm::dd::Bdd<DdType> statesWithProbabilityGreater0;
    if (dir == OptimizationDirection::Minimize) {
        statesWithProbabilityGreater0 = storm::utility::graph::performProbGreater0A(model, transitionMatrix.notZero(), phiStates, psiStates);
    } else {
        statesWithProbabilityGreater0 = storm::utility::graph::performProbGreater0E(model, transitionMatrix.notZero(), phiStates, psiStates);
    }
    storm::dd::Bdd<DdType> maybeStates = statesWithProbabilityGreater0 && !psiStates && model.getReachableStates();

    STORM_LOG_INFO("Preprocessing: " << statesWithProbabilityGreater0.getNonZeroCount() << " states with probability greater 0.");

    // If there are maybe states, we need to perform matrix-vector multiplications.
    if (!maybeStates.isZero()) {
        storm::utility::Stopwatch conversionWatch;

        // Create the ODD for the translation between symbolic and explicit storage.
        conversionWatch.start();
        storm::dd::Odd odd = maybeStates.createOdd();
        conversionWatch.stop();

        // Create the matrix and the vector for the equation system.
        storm::dd::Add<DdType, ValueType> maybeStatesAdd = maybeStates.template toAdd<ValueType>();

        // Start by cutting away all rows that do not belong to maybe states. Note that this leaves columns targeting
        // non-maybe states in the matrix.
        storm::dd::Add<DdType, ValueType> submatrix = transitionMatrix * maybeStatesAdd;

        // Then compute the vector that contains the one-step probabilities to a state with probability 1 for all
        // maybe states.
        storm::dd::Add<DdType, ValueType> prob1StatesAsColumn = psiStates.template toAdd<ValueType>().swapVariables(model.getRowColumnMetaVariablePairs());
        storm::dd::Add<DdType, ValueType> subvector = (submatrix * prob1StatesAsColumn).sumAbstract(model.getColumnVariables());

        // Finally cut away all columns targeting non-maybe states.
        submatrix *= maybeStatesAdd.swapVariables(model.getRowColumnMetaVariablePairs());

        // Create the solution vector.
        std::vector<ValueType> x(maybeStates.getNonZeroCount(), storm::utility::zero<ValueType>());

        // Translate the symbolic matrix/vector to their explicit representations.
        conversionWatch.start();
        std::pair<storm::storage::SparseMatrix<ValueType>, std::vector<ValueType>> explicitRepresentation =
            submatrix.toMatrixVector(subvector, model.getNondeterminismVariables(), odd, odd);
        conversionWatch.stop();
        STORM_LOG_INFO("Converting symbolic matrix/vector to explicit representation done in " << conversionWatch.getTimeInMilliseconds() << "ms.");

        auto multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, explicitRepresentation.first);
        multiplier->repeatedMultiplyAndReduce(env, dir, x, &explicitRepresentation.second, stepBound);

        // Return a hybrid check result that stores the numerical values explicitly.
        return std::unique_ptr<CheckResult>(new storm::modelchecker::HybridQuantitativeCheckResult<DdType, ValueType>(
            model.getReachableStates(), model.getReachableStates() && !maybeStates, psiStates.template toAdd<ValueType>(), maybeStates, odd, x));
    } else {
        return std::unique_ptr<CheckResult>(
            new storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>(model.getReachableStates(), psiStates.template toAdd<ValueType>()));
    }
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeInstantaneousRewards(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
    storm::dd::Add<DdType, ValueType> const& transitionMatrix, RewardModelType const& rewardModel, uint_fast64_t stepBound) {
    // Only compute the result if the model has at least one reward this->getModel().
    STORM_LOG_THROW(rewardModel.hasStateRewards(), storm::exceptions::InvalidPropertyException,
                    "Computing instantaneous rewards for a reward model that does not define any state-rewards. The result is trivially 0.");

    storm::utility::Stopwatch conversionWatch;

    // Create the ODD for the translation between symbolic and explicit storage.
    storm::dd::Odd odd = model.getReachableStates().createOdd();

    // Translate the symbolic matrix to its explicit representations.
    storm::storage::SparseMatrix<ValueType> explicitMatrix = transitionMatrix.toMatrix(model.getNondeterminismVariables(), odd, odd);

    // Create the solution vector (and initialize it to the state rewards of the model).
    std::vector<ValueType> x = rewardModel.getStateRewardVector().toVector(odd);
    conversionWatch.stop();
    STORM_LOG_INFO("Converting symbolic matrix/vector to explicit representation done in " << conversionWatch.getTimeInMilliseconds() << "ms.");

    // Perform the matrix-vector multiplication.
    auto multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, explicitMatrix);
    multiplier->repeatedMultiplyAndReduce(env, dir, x, nullptr, stepBound);

    // Return a hybrid check result that stores the numerical values explicitly.
    return std::unique_ptr<CheckResult>(new HybridQuantitativeCheckResult<DdType, ValueType>(
        model.getReachableStates(), model.getManager().getBddZero(), model.getManager().template getAddZero<ValueType>(), model.getReachableStates(), odd, x));
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeCumulativeRewards(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
    storm::dd::Add<DdType, ValueType> const& transitionMatrix, RewardModelType const& rewardModel, uint_fast64_t stepBound) {
    // Only compute the result if the model has at least one reward this->getModel().
    STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");

    // Compute the reward vector to add in each step based on the available reward models.
    storm::dd::Add<DdType, ValueType> totalRewardVector = rewardModel.getTotalRewardVector(transitionMatrix, model.getColumnVariables());

    // Create the solution vector.
    std::vector<ValueType> x(model.getNumberOfStates(), storm::utility::zero<ValueType>());

    storm::utility::Stopwatch conversionWatch(true);

    // Create the ODD for the translation between symbolic and explicit storage.
    storm::dd::Odd odd = model.getReachableStates().createOdd();

    // Translate the symbolic matrix/vector to their explicit representations.
    std::pair<storm::storage::SparseMatrix<ValueType>, std::vector<ValueType>> explicitRepresentation =
        transitionMatrix.toMatrixVector(totalRewardVector, model.getNondeterminismVariables(), odd, odd);
    conversionWatch.stop();
    STORM_LOG_INFO("Converting symbolic matrix/vector to explicit representation done in " << conversionWatch.getTimeInMilliseconds() << "ms.");

    // Perform the matrix-vector multiplication.
    auto multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, explicitRepresentation.first);
    multiplier->repeatedMultiplyAndReduce(env, dir, x, &explicitRepresentation.second, stepBound);

    // Return a hybrid check result that stores the numerical values explicitly.
    return std::unique_ptr<CheckResult>(new HybridQuantitativeCheckResult<DdType, ValueType>(
        model.getReachableStates(), model.getManager().getBddZero(), model.getManager().template getAddZero<ValueType>(), model.getReachableStates(), odd, x));
}

template<typename ValueType>
storm::storage::BitVector computeTargetStatesForReachabilityRewardsFromExplicitRepresentation(storm::storage::SparseMatrix<ValueType> const& transitionMatrix) {
    storm::storage::BitVector targetStates(transitionMatrix.getRowGroupCount());

    // A state is a target state iff its row group is empty.
    for (uint64_t rowGroup = 0; rowGroup < transitionMatrix.getRowGroupCount(); ++rowGroup) {
        if (transitionMatrix.getRowGroupIndices()[rowGroup] == transitionMatrix.getRowGroupIndices()[rowGroup + 1]) {
            targetStates.set(rowGroup);
        }
    }

    return targetStates;
}

template<typename ValueType>
std::vector<uint64_t> computeValidInitialSchedulerForReachabilityRewards(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                         storm::storage::BitVector const& properMaybeStates,
                                                                         storm::storage::BitVector const& targetStates) {
    uint64_t numberOfMaybeStates = transitionMatrix.getRowGroupCount();
    std::vector<uint64_t> result(numberOfMaybeStates);

    storm::storage::Scheduler<ValueType> validScheduler(numberOfMaybeStates);
    storm::storage::SparseMatrix<ValueType> backwardTransitions = transitionMatrix.transpose(true);
    storm::utility::graph::computeSchedulerProb1E(storm::storage::BitVector(numberOfMaybeStates, true), transitionMatrix, backwardTransitions,
                                                  properMaybeStates, targetStates, validScheduler);

    for (uint64_t state = 0; state < numberOfMaybeStates; ++state) {
        if (!targetStates.get(state)) {
            result[state] = validScheduler.getChoice(state).getDeterministicChoice();
        }
    }

    return result;
}

template<typename ValueType>
std::vector<ValueType> computeOneStepTargetProbabilitiesFromExtendedExplicitRepresentation(storm::storage::SparseMatrix<ValueType> const& extendedMatrix,
                                                                                           storm::storage::BitVector const& properMaybeStates,
                                                                                           storm::storage::BitVector const& targetStates) {
    return extendedMatrix.getConstrainedRowGroupSumVector(properMaybeStates, targetStates);
}

template<typename ValueType>
void eliminateEndComponentsAndTargetStatesReachabilityRewards(
    std::pair<storm::storage::SparseMatrix<ValueType>, std::vector<ValueType>>& explicitRepresentation,
    SolverRequirementsData<ValueType>& solverRequirementsData, storm::storage::BitVector const& targetStates, bool computeOneStepTargetProbabilities) {
    // Get easier handles to the data.
    auto& transitionMatrix = explicitRepresentation.first;
    auto& rewardVector = explicitRepresentation.second;

    // Start by computing the choices with reward 0, as we only want ECs within this fragment.
    storm::storage::BitVector zeroRewardChoices(transitionMatrix.getRowCount());

    uint64_t index = 0;
    for (auto const& e : rewardVector) {
        if (storm::utility::isZero(e)) {
            zeroRewardChoices.set(index);
        }
        ++index;
    }

    // Compute the states that have some zero reward choice.
    storm::storage::BitVector candidateStates(solverRequirementsData.properMaybeStates);
    for (auto state : solverRequirementsData.properMaybeStates) {
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

    bool doDecomposition = !candidateStates.empty();

    storm::storage::MaximalEndComponentDecomposition<ValueType> endComponentDecomposition;
    if (doDecomposition) {
        auto backwardTransitions = transitionMatrix.transpose(true);

        // Only keep the candidate states that (under some scheduler) can stay in the set of candidates forever
        candidateStates = storm::utility::graph::performProb0E(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, candidateStates,
                                                               ~candidateStates);

        doDecomposition = !candidateStates.empty();

        if (doDecomposition) {
            // If there are candidates, compute the states that are in MECs with zero reward.
            endComponentDecomposition =
                storm::storage::MaximalEndComponentDecomposition<ValueType>(transitionMatrix, backwardTransitions, candidateStates, zeroRewardChoices);
        }
    }

    // Only do more work if there are actually end-components.
    if (doDecomposition && !endComponentDecomposition.empty()) {
        STORM_LOG_DEBUG("Eliminating " << endComponentDecomposition.size() << " EC(s).");
        if (computeOneStepTargetProbabilities) {
            solverRequirementsData.oneStepTargetProbabilities =
                std::vector<ValueType>(solverRequirementsData.properMaybeStates.getNumberOfSetBits(), storm::utility::zero<ValueType>());
        }

        std::vector<ValueType> subvector(solverRequirementsData.properMaybeStates.getNumberOfSetBits(), storm::utility::zero<ValueType>());
        SparseMdpEndComponentInformation<ValueType>::eliminateEndComponents(
            endComponentDecomposition, transitionMatrix, solverRequirementsData.properMaybeStates, computeOneStepTargetProbabilities ? &targetStates : nullptr,
            nullptr, &rewardVector, transitionMatrix, computeOneStepTargetProbabilities ? &solverRequirementsData.oneStepTargetProbabilities.get() : nullptr,
            &subvector);
        rewardVector = std::move(subvector);
    } else {
        STORM_LOG_DEBUG("Not eliminating ECs as there are none.");
        if (computeOneStepTargetProbabilities) {
            solverRequirementsData.oneStepTargetProbabilities = computeOneStepTargetProbabilitiesFromExtendedExplicitRepresentation(
                explicitRepresentation.first, solverRequirementsData.properMaybeStates, targetStates);
        }
        eliminateExtendedStatesFromExplicitRepresentation(explicitRepresentation, solverRequirementsData.initialScheduler,
                                                          solverRequirementsData.properMaybeStates);
    }
}

template<typename ValueType>
void setUpperRewardBounds(storm::solver::MinMaxLinearEquationSolver<ValueType>& solver, storm::OptimizationDirection const& direction,
                          storm::storage::SparseMatrix<ValueType> const& submatrix, std::vector<ValueType> const& choiceRewards,
                          std::vector<ValueType> const& oneStepTargetProbabilities) {
    // For the min-case, we use DS-MPI, for the max-case variant 2 of the Baier et al. paper (CAV'17).
    if (direction == storm::OptimizationDirection::Minimize) {
        DsMpiMdpUpperRewardBoundsComputer<ValueType> dsmpi(submatrix, choiceRewards, oneStepTargetProbabilities);
        solver.setUpperBounds(dsmpi.computeUpperBounds());
    } else {
        BaierUpperRewardBoundsComputer<ValueType> baier(submatrix, oneStepTargetProbabilities);
        solver.setUpperBound(baier.computeTotalRewardBounds(choiceRewards).upper);
    }
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeReachabilityRewards(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
    storm::dd::Add<DdType, ValueType> const& transitionMatrix, RewardModelType const& rewardModel, storm::dd::Bdd<DdType> const& targetStates,
    bool qualitative) {
    // Only compute the result if there is at least one reward model.
    STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");

    // Determine which states have a reward of infinity by definition.
    storm::dd::Bdd<DdType> infinityStates;
    storm::dd::Bdd<DdType> transitionMatrixBdd = transitionMatrix.notZero();
    if (dir == OptimizationDirection::Minimize) {
        infinityStates = storm::utility::graph::performProb1E(
            model, transitionMatrixBdd, model.getReachableStates(), targetStates,
            storm::utility::graph::performProbGreater0E(model, transitionMatrixBdd, model.getReachableStates(), targetStates));
    } else {
        infinityStates = storm::utility::graph::performProb1A(
            model, transitionMatrixBdd, targetStates,
            storm::utility::graph::performProbGreater0A(model, transitionMatrixBdd, model.getReachableStates(), targetStates));
    }
    infinityStates = !infinityStates && model.getReachableStates();
    storm::dd::Bdd<DdType> maybeStatesWithTargetStates = !infinityStates && model.getReachableStates();
    storm::dd::Bdd<DdType> maybeStates = !targetStates && maybeStatesWithTargetStates;

    STORM_LOG_INFO("Preprocessing: " << infinityStates.getNonZeroCount() << " states with reward infinity, " << targetStates.getNonZeroCount()
                                     << " target states (" << maybeStates.getNonZeroCount() << " states remaining).");

    // Check whether we need to compute exact rewards for some states.
    if (qualitative) {
        // Set the values for all maybe-states to 1 to indicate that their reward values
        // are neither 0 nor infinity.
        return std::unique_ptr<CheckResult>(new SymbolicQuantitativeCheckResult<DdType, ValueType>(
            model.getReachableStates(),
            infinityStates.ite(model.getManager().getConstant(storm::utility::infinity<ValueType>()), model.getManager().template getAddZero<ValueType>()) +
                maybeStates.template toAdd<ValueType>() * model.getManager().getConstant(storm::utility::one<ValueType>())));
    } else {
        // If there are maybe states, we need to solve an equation system.
        if (!maybeStates.isZero()) {
            // If we maximize, we know that the solution to the equation system is unique.
            bool hasNoEndComponents = dir == storm::solver::OptimizationDirection::Maximize;
            bool hasUniqueSolution = hasNoEndComponents;
            // Check for requirements of the solver this early so we can adapt the maybe states accordingly.
            storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType> linearEquationSolverFactory;
            storm::solver::MinMaxLinearEquationSolverRequirements requirements =
                linearEquationSolverFactory.getRequirements(env, hasUniqueSolution, hasNoEndComponents, dir);
            storm::solver::MinMaxLinearEquationSolverRequirements clearedRequirements = requirements;
            bool extendMaybeStates = false;
            if (clearedRequirements.hasEnabledRequirement()) {
                if (clearedRequirements.uniqueSolution()) {
                    STORM_LOG_DEBUG("Scheduling EC elimination, because the solver requires it.");
                    extendMaybeStates = true;
                    clearedRequirements.clearUniqueSolution();
                    hasUniqueSolution = true;
                    // There might still be end components in which reward is collected.
                }
                if (clearedRequirements.validInitialScheduler()) {
                    STORM_LOG_DEBUG("Computing valid scheduler, because the solver requires it.");
                    extendMaybeStates = true;
                    clearedRequirements.clearValidInitialScheduler();
                }
                clearedRequirements.clearLowerBounds();
                if (clearedRequirements.upperBounds()) {
                    STORM_LOG_DEBUG("Computing upper bounds, because the solver requires it.");
                    extendMaybeStates = true;
                    clearedRequirements.clearUpperBounds();
                }
                STORM_LOG_THROW(!clearedRequirements.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                                "Solver requirements " + clearedRequirements.getEnabledRequirementsAsString() + " not checked.");
            }

            // Compute the set of maybe states that we are required to keep in the translation to explicit.
            storm::dd::Bdd<DdType> requiredMaybeStates = extendMaybeStates ? maybeStatesWithTargetStates : maybeStates;

            storm::utility::Stopwatch conversionWatch;

            // Create the ODD for the translation between symbolic and explicit storage.
            conversionWatch.start();
            storm::dd::Odd odd = requiredMaybeStates.createOdd();
            conversionWatch.stop();

            // Create the matrix and the vector for the equation system.
            storm::dd::Add<DdType, ValueType> maybeStatesAdd = maybeStates.template toAdd<ValueType>();

            // Start by getting rid of
            // (a) transitions from non-maybe states, and
            // (b) the choices in the transition matrix that lead to a state that is neither a maybe state
            // nor a target state ('infinity choices').
            storm::dd::Add<DdType, ValueType> choiceFilterAdd =
                maybeStatesAdd * (transitionMatrixBdd && maybeStatesWithTargetStates.renameVariables(model.getRowVariables(), model.getColumnVariables()))
                                     .existsAbstract(model.getColumnVariables())
                                     .template toAdd<ValueType>();
            storm::dd::Add<DdType, ValueType> submatrix = transitionMatrix * choiceFilterAdd;

            // Then compute the reward vector to use in the computation.
            storm::dd::Add<DdType, ValueType> subvector =
                rewardModel.getTotalRewardVector(maybeStatesAdd, choiceFilterAdd, submatrix, model.getColumnVariables());

            conversionWatch.start();
            std::vector<uint_fast64_t> rowGroupSizes = (submatrix.notZero().existsAbstract(model.getColumnVariables()) || subvector.notZero())
                                                           .template toAdd<uint_fast64_t>()
                                                           .sumAbstract(model.getNondeterminismVariables())
                                                           .toVector(odd);
            conversionWatch.stop();

            // Finally cut away all columns targeting non-maybe states (or non-(maybe or target) states, respectively).
            submatrix *= extendMaybeStates ? maybeStatesWithTargetStates.swapVariables(model.getRowColumnMetaVariablePairs()).template toAdd<ValueType>()
                                           : maybeStatesAdd.swapVariables(model.getRowColumnMetaVariablePairs());

            // Translate the symbolic matrix/vector to their explicit representations.
            conversionWatch.start();
            std::pair<storm::storage::SparseMatrix<ValueType>, std::vector<ValueType>> explicitRepresentation = submatrix.toMatrixVector(
                std::move(rowGroupSizes), subvector, model.getRowVariables(), model.getColumnVariables(), model.getNondeterminismVariables(), odd, odd);
            conversionWatch.stop();
            STORM_LOG_INFO("Converting symbolic matrix/vector to explicit representation done in " << conversionWatch.getTimeInMilliseconds() << "ms.");

            // Fulfill the solver's requirements.
            SolverRequirementsData<ValueType> solverRequirementsData;
            if (extendMaybeStates) {
                storm::storage::BitVector targetStates = computeTargetStatesForReachabilityRewardsFromExplicitRepresentation(explicitRepresentation.first);
                solverRequirementsData.properMaybeStates = ~targetStates;

                if (requirements.uniqueSolution()) {
                    STORM_LOG_THROW(!requirements.validInitialScheduler(), storm::exceptions::UncheckedRequirementException,
                                    "The underlying solver requires a unique solution and an initial valid scheduler. This is currently not supported for "
                                    "expected reward properties.");
                    // eliminate the end components with reward 0.
                    // Note that this may also compute the oneStepTargetProbabilities if upper bounds are required.
                    eliminateEndComponentsAndTargetStatesReachabilityRewards(explicitRepresentation, solverRequirementsData, targetStates,
                                                                             requirements.upperBounds());
                    // The solution becomes unique after end components have been eliminated.
                    hasUniqueSolution = true;
                } else {
                    if (requirements.validInitialScheduler()) {
                        // Compute a valid initial scheduler.
                        solverRequirementsData.initialScheduler = computeValidInitialSchedulerForReachabilityRewards<ValueType>(
                            explicitRepresentation.first, solverRequirementsData.properMaybeStates, targetStates);
                    }

                    if (requirements.upperBounds()) {
                        solverRequirementsData.oneStepTargetProbabilities = computeOneStepTargetProbabilitiesFromExtendedExplicitRepresentation(
                            explicitRepresentation.first, solverRequirementsData.properMaybeStates, targetStates);
                    }

                    // Since we needed the transitions to target states to be translated as well for the computation
                    // of the scheduler, we have to get rid of them now.
                    eliminateExtendedStatesFromExplicitRepresentation(explicitRepresentation, solverRequirementsData.initialScheduler,
                                                                      solverRequirementsData.properMaybeStates);
                }
            }

            // Create the solution vector.
            std::vector<ValueType> x(explicitRepresentation.first.getRowGroupCount(), storm::utility::zero<ValueType>());

            // Now solve the resulting equation system.
            std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> solver = linearEquationSolverFactory.create(env);

            // Set whether the equation system will have a unique solution / no end components
            solver->setHasUniqueSolution(hasUniqueSolution);
            solver->setHasNoEndComponents(hasNoEndComponents);

            // If the solver requires upper bounds, compute them now.
            if (requirements.upperBounds()) {
                setUpperRewardBounds(*solver, dir, explicitRepresentation.first, explicitRepresentation.second,
                                     solverRequirementsData.oneStepTargetProbabilities.get());
            }

            solver->setMatrix(std::move(explicitRepresentation.first));

            // Move the scheduler to the solver.
            if (solverRequirementsData.initialScheduler) {
                solver->setInitialScheduler(std::move(solverRequirementsData.initialScheduler.get()));
            }

            solver->setLowerBound(storm::utility::zero<ValueType>());
            solver->setRequirementsChecked();
            solver->solveEquations(env, dir, x, explicitRepresentation.second);

            // If we eliminated end components, we need to extend the solution vector.
            if (requirements.uniqueSolution() && solverRequirementsData.ecInformation) {
                std::vector<ValueType> extendedVector(solverRequirementsData.properMaybeStates.getNumberOfSetBits());
                solverRequirementsData.ecInformation.get().setValues(extendedVector, solverRequirementsData.properMaybeStates, x);
                x = std::move(extendedVector);
            }

            // If we extended the maybe states, we create a new ODD that only contains proper maybe states.
            if (extendMaybeStates) {
                odd = maybeStates.createOdd();
            }

            // Return a hybrid check result that stores the numerical values explicitly.
            return std::unique_ptr<CheckResult>(new storm::modelchecker::HybridQuantitativeCheckResult<DdType, ValueType>(
                model.getReachableStates(), model.getReachableStates() && !maybeStates,
                infinityStates.ite(model.getManager().getConstant(storm::utility::infinity<ValueType>()), model.getManager().template getAddZero<ValueType>()),
                maybeStates, odd, x));
        } else {
            return std::unique_ptr<CheckResult>(new storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>(
                model.getReachableStates(), infinityStates.ite(model.getManager().getConstant(storm::utility::infinity<ValueType>()),
                                                               model.getManager().template getAddZero<ValueType>())));
        }
    }
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<CheckResult> HybridMdpPrctlHelper<DdType, ValueType>::computeReachabilityTimes(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
    storm::dd::Add<DdType, ValueType> const& transitionMatrix, storm::dd::Bdd<DdType> const& targetStates, bool qualitative) {
    RewardModelType rewardModel(model.getManager().getConstant(storm::utility::one<ValueType>()), boost::none, boost::none);
    return computeReachabilityRewards(env, dir, model, transitionMatrix, rewardModel, targetStates, qualitative);
}

template class HybridMdpPrctlHelper<storm::dd::DdType::CUDD, double>;
template class HybridMdpPrctlHelper<storm::dd::DdType::Sylvan, double>;

template class HybridMdpPrctlHelper<storm::dd::DdType::Sylvan, storm::RationalNumber>;

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
