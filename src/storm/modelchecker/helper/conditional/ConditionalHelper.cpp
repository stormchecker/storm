#include "storm/modelchecker/helper/conditional/ConditionalHelper.h"

#include <algorithm>
#include <stack>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/adapters/RationalNumberForward.h"
#include "storm/environment/modelchecker/ConditionalModelCheckerEnvironment.h"
#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/modelchecker/prctl/helper/SparseMdpPrctlHelper.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/solver/SolveGoal.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/transformer/EndComponentEliminator.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/OptionalRef.h"
#include "storm/utility/RationalApproximation.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/logging.h"
#include "storm/utility/macros.h"

namespace storm::modelchecker {

namespace internal {

template<typename ValueType>
std::optional<typename storm::transformer::EndComponentEliminator<ValueType>::EndComponentEliminatorReturnType> eliminateEndComponents(
    storm::storage::BitVector const& possibleEcStates, bool addRowAtRepresentativeState, std::optional<uint64_t> const representativeRowEntry,
    storm::storage::SparseMatrix<ValueType>& matrix, storm::storage::BitVector& rowsWithSum1, std::vector<ValueType>& rowValues1,
    storm::OptionalRef<std::vector<ValueType>> rowValues2 = {}) {
    storm::storage::MaximalEndComponentDecomposition<ValueType> ecs(matrix, matrix.transpose(true), possibleEcStates, rowsWithSum1);
    if (ecs.empty()) {
        return {};  // nothing to do
    }

    storm::storage::BitVector allRowGroups(matrix.getRowGroupCount(), true);
    auto ecElimResult = storm::transformer::EndComponentEliminator<ValueType>::transform(
        matrix, ecs, allRowGroups, addRowAtRepresentativeState ? allRowGroups : ~allRowGroups, representativeRowEntry.has_value());

    // Update matrix
    matrix = std::move(ecElimResult.matrix);
    if (addRowAtRepresentativeState && representativeRowEntry) {
        auto const columnIndex = ecElimResult.oldToNewStateMapping[*representativeRowEntry];
        for (auto representativeRowIndex : ecElimResult.sinkRows) {
            auto row = matrix.getRow(representativeRowIndex);
            STORM_LOG_ASSERT(row.getNumberOfEntries() == 1, "unexpected number of entries in representative row.");
            auto& entry = *row.begin();
            entry.setColumn(columnIndex);
        }
    }

    // update vectors
    auto updateRowValue = [&ecElimResult](std::vector<ValueType>& rowValues) {
        std::vector<ValueType> newRowValues;
        newRowValues.reserve(ecElimResult.newToOldRowMapping.size());
        for (auto oldRowIndex : ecElimResult.newToOldRowMapping) {
            newRowValues.push_back(rowValues[oldRowIndex]);
        }
        rowValues = std::move(newRowValues);
        STORM_LOG_ASSERT(
            std::all_of(ecElimResult.sinkRows.begin(), ecElimResult.sinkRows.end(), [&rowValues](auto i) { return storm::utility::isZero(rowValues[i]); }),
            "Sink rows are expected to have zero value");
    };
    updateRowValue(rowValues1);
    if (rowValues2) {
        updateRowValue(*rowValues2);
    }

    // update bitvector
    storm::storage::BitVector newRowsWithSum1(ecElimResult.newToOldRowMapping.size(), true);
    uint64_t newRowIndex = 0;
    for (auto oldRowIndex : ecElimResult.newToOldRowMapping) {
        if ((addRowAtRepresentativeState && !representativeRowEntry.has_value() && ecElimResult.sinkRows.get(newRowIndex)) || !rowsWithSum1.get(oldRowIndex)) {
            newRowsWithSum1.set(newRowIndex, false);
        }
        ++newRowIndex;
    }
    rowsWithSum1 = std::move(newRowsWithSum1);

    return ecElimResult;
}

template<typename ValueType, typename SolutionType = ValueType>
SolutionType solveMinMaxEquationSystem(storm::Environment const& env, storm::storage::SparseMatrix<ValueType> const& matrix,
                                       std::vector<ValueType> const& rowValues, storm::storage::BitVector const& rowsWithSum1,
                                       storm::solver::SolveGoal<ValueType, SolutionType> const& goal, uint64_t const initialState,
                                       std::optional<std::vector<uint64_t>>& schedulerOutput) {
    // Initialize the solution vector.
    std::vector<SolutionType> x(matrix.getRowGroupCount(), storm::utility::zero<ValueType>());

    // Set up the solver.
    storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType, SolutionType> factory;
    storm::storage::BitVector relevantValues(matrix.getRowGroupCount(), false);
    relevantValues.set(initialState, true);
    auto getGoal = [&goal, &relevantValues]() -> storm::solver::SolveGoal<ValueType, SolutionType> {
        if (goal.isBounded()) {
            return {goal.direction(), goal.boundComparisonType(), goal.thresholdValue(), relevantValues};
        } else {
            return {goal.direction(), relevantValues};
        }
    };
    auto solver = storm::solver::configureMinMaxLinearEquationSolver(env, getGoal(), factory, matrix);

    storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType, SolutionType>().create(env, matrix);
    solver->setOptimizationDirection(goal.direction());
    solver->setRequirementsChecked();
    solver->setHasUniqueSolution(true);
    solver->setHasNoEndComponents(true);
    solver->setLowerBound(storm::utility::zero<ValueType>());
    solver->setUpperBound(storm::utility::one<ValueType>());
    solver->setTrackScheduler(schedulerOutput.has_value());

    // Solve the corresponding system of equations.
    solver->solveEquations(env, x, rowValues);

    if (schedulerOutput) {
        *schedulerOutput = std::move(solver->getSchedulerChoices());
    }

    return x[initialState];
}

/*!
 * Computes the reachability probabilities for the given target states and inserts all non-zero values into the given map.
 * @note This code is optimized for cases where not all states are reachable from the initial states.
 */
template<typename ValueType>
std::unique_ptr<storm::storage::Scheduler<ValueType>> computeReachabilityProbabilities(
    Environment const& env, std::map<uint64_t, ValueType>& nonZeroResults, storm::solver::OptimizationDirection const dir,
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& initialStates,
    storm::storage::BitVector const& allowedStates, storm::storage::BitVector const& targetStates, bool computeScheduler = true) {
    std::unique_ptr<storm::storage::Scheduler<ValueType>> scheduler;
    if (computeScheduler) {
        scheduler = std::make_unique<storm::storage::Scheduler<ValueType>>(transitionMatrix.getRowGroupCount());
    }

    auto reachabilityEnv = env;
    reachabilityEnv.solver().minMax().setPrecision(env.modelchecker().conditional().getPrecision());
    reachabilityEnv.solver().minMax().setRelativeTerminationCriterion(env.modelchecker().conditional().isRelativePrecision());

    if (initialStates.empty()) {  // nothing to do
        return scheduler;
    }
    auto const reachableStates = storm::utility::graph::getReachableStates(transitionMatrix, initialStates, allowedStates, targetStates);
    auto const subTargets = targetStates % reachableStates;
    // Catch the case where no target is reachable from an initial state. In this case, there is nothing to do since all probabilities are zero.
    if (subTargets.empty()) {
        return scheduler;
    }
    auto const subInits = initialStates % reachableStates;
    auto const submatrix = transitionMatrix.getSubmatrix(true, reachableStates, reachableStates);
    auto const subResult = helper::SparseMdpPrctlHelper<ValueType, ValueType>::computeUntilProbabilities(
        reachabilityEnv, storm::solver::SolveGoal<ValueType>(dir, subInits), submatrix, submatrix.transpose(true),
        storm::storage::BitVector(subTargets.size(), true), subTargets, false, computeScheduler);

    auto origInitIt = initialStates.begin();
    for (auto subInit : subInits) {
        auto const& val = subResult.values[subInit];
        if (!storm::utility::isZero(val)) {
            nonZeroResults.emplace(*origInitIt, val);
        }
        ++origInitIt;
    }

    if (computeScheduler) {
        auto submatrixIdx = 0;
        for (auto state : reachableStates) {
            scheduler->setChoice(subResult.scheduler->getChoice(submatrixIdx), state);
            ++submatrixIdx;
        }
    }

    return scheduler;
}

template<typename ValueType>
struct NormalFormData {
    storm::storage::BitVector const maybeStates;      // Those states that can be reached from initial without reaching a terminal state
    storm::storage::BitVector const terminalStates;   // Those states where we already know the probability to reach the condition and the target value
    storm::storage::BitVector const conditionStates;  // Those states where the condition holds almost surely (under all schedulers)
    storm::storage::BitVector const targetStates;     // Those states where the target holds almost surely (under all schedulers)
    storm::storage::BitVector const universalObservationFailureStates;    // Those states where the condition is not reachable (under all schedulers)
    storm::storage::BitVector const existentialObservationFailureStates;  // Those states s where a scheduler exists that (i) does not reach the condition from
                                                                          // s and (ii) acts optimal in all terminal states
    std::map<uint64_t, ValueType> const nonZeroTargetStateValues;         // The known non-zero target values. (default is zero)
    // There are three cases of terminal states:
    // 1. conditionStates: The condition holds, so the target value is the optimal probability to reach target from there
    // 2. targetStates: The target is reached, so the target value is the optimal probability to reach a condition from there.
    //                  The remaining probability mass is the probability of an observation failure
    // 3. states that can not reach the condition under any scheduler. The target value is zero.

    // TerminalStates is a superset of conditionStates and dom(nonZeroTargetStateValues).
    // For a terminalState that is not a conditionState, it is impossible to (reach the condition and not reach the target).

    std::unique_ptr<storm::storage::Scheduler<ValueType>>
        schedulerChoicesForReachingTargetStates;  // Scheduler choices for reaching target states, used for constructing the resulting scheduler
    std::unique_ptr<storm::storage::Scheduler<ValueType>>
        schedulerChoicesForReachingConditionStates;  // Scheduler choices for reaching condition states, used for constructing the resulting scheduler

    ValueType getTargetValue(uint64_t state) const {
        STORM_LOG_ASSERT(terminalStates.get(state), "Tried to get target value for non-terminal state");
        auto const it = nonZeroTargetStateValues.find(state);
        return it == nonZeroTargetStateValues.end() ? storm::utility::zero<ValueType>() : it->second;
    }

    ValueType failProbability(uint64_t state) const {
        STORM_LOG_ASSERT(terminalStates.get(state), "Tried to get fail probability for non-terminal state");
        STORM_LOG_ASSERT(!conditionStates.get(state), "Tried to get fail probability for a condition state");
        // condition states have fail probability zero
        return storm::utility::one<ValueType>() - getTargetValue(state);
    }
};

template<typename ValueType>
NormalFormData<ValueType> obtainNormalForm(Environment const& env, storm::solver::OptimizationDirection const dir, bool computeScheduler,
                                           storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                           storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& relevantStates,
                                           storm::storage::BitVector const& targetStates, storm::storage::BitVector const& conditionStates) {
    storm::storage::BitVector const allStates(transitionMatrix.getRowGroupCount(), true);
    auto extendedConditionStates =
        storm::utility::graph::performProb1A(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, allStates, conditionStates);
    auto universalObservationFailureStates = storm::utility::graph::performProb0A(backwardTransitions, allStates, extendedConditionStates);
    std::map<uint64_t, ValueType> nonZeroTargetStateValues;
    auto const extendedTargetStates =
        storm::utility::graph::performProb1A(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, allStates, targetStates);
    auto const targetAndNotCondFailStates = extendedTargetStates & ~(extendedConditionStates | universalObservationFailureStates);

    // compute schedulers for reaching target and condition states from target and condition states
    std::unique_ptr<storm::storage::Scheduler<ValueType>> schedulerChoicesForReachingTargetStates = computeReachabilityProbabilities(
        env, nonZeroTargetStateValues, dir, transitionMatrix, extendedConditionStates, allStates, extendedTargetStates, computeScheduler);
    std::unique_ptr<storm::storage::Scheduler<ValueType>> schedulerChoicesForReachingConditionStates = computeReachabilityProbabilities(
        env, nonZeroTargetStateValues, dir, transitionMatrix, targetAndNotCondFailStates, allStates, extendedConditionStates, computeScheduler);

    // get states where the optimal policy reaches the condition with positive probability
    auto terminalStatesThatReachCondition = extendedConditionStates;
    for (auto state : targetAndNotCondFailStates) {
        if (nonZeroTargetStateValues.contains(state)) {
            terminalStatesThatReachCondition.set(state, true);
        }
    }

    // get the terminal states following the three cases described above
    auto terminalStates = extendedConditionStates | extendedTargetStates | universalObservationFailureStates;
    if (storm::solver::minimize(dir)) {
        // There can be target states from which (only) the *minimal* probability to reach a condition is zero.
        // For those states, the optimal policy is to enforce observation failure.
        // States that can only reach (target states with almost sure observation failure) or observation failure will be treated as terminal states with
        // targetValue zero and failProbability one.
        terminalStates |= storm::utility::graph::performProb0A(backwardTransitions, ~terminalStates, terminalStatesThatReachCondition);
    }

    auto nonTerminalStates = ~terminalStates;

    auto existentialObservationFailureStates = storm::utility::graph::performProb0E(transitionMatrix, transitionMatrix.getRowGroupIndices(),
                                                                                    backwardTransitions, nonTerminalStates, terminalStatesThatReachCondition);

    // Restrict non-terminal states to those that are still relevant
    nonTerminalStates &= storm::utility::graph::getReachableStates(transitionMatrix, relevantStates, nonTerminalStates, terminalStates);

    return NormalFormData<ValueType>{.maybeStates = std::move(nonTerminalStates),
                                     .terminalStates = std::move(terminalStates),
                                     .conditionStates = std::move(extendedConditionStates),
                                     .targetStates = std::move(extendedTargetStates),
                                     .universalObservationFailureStates = std::move(universalObservationFailureStates),
                                     .existentialObservationFailureStates = std::move(existentialObservationFailureStates),
                                     .nonZeroTargetStateValues = std::move(nonZeroTargetStateValues),
                                     .schedulerChoicesForReachingTargetStates = std::move(schedulerChoicesForReachingTargetStates),
                                     .schedulerChoicesForReachingConditionStates = std::move(schedulerChoicesForReachingConditionStates)};
}

// computes the scheduler that reaches the EC exits from the maybe states that were removed by EC elimination
template<typename ValueType, typename SolutionType = ValueType>
void finalizeSchedulerForMaybeStates(storm::storage::Scheduler<SolutionType>& scheduler, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                     storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& maybeStates,
                                     storm::storage::BitVector const& maybeStatesWithoutChoice, storm::storage::BitVector const& maybeStatesWithChoice,
                                     std::vector<uint64_t> const& stateToFinalEc, NormalFormData<ValueType> const& normalForm, uint64_t initialComponentIndex,
                                     storm::storage::BitVector const& initialComponentExitStates, storm::storage::BitVector const& initialComponentExitRows,
                                     uint64_t chosenInitialComponentExitState, uint64_t chosenInitialComponentExit) {
    // Compute the EC stay choices for the states in maybeStatesWithChoice
    storm::storage::BitVector ecStayChoices(transitionMatrix.getRowCount(), false);
    storm::storage::BitVector initialComponentStates(transitionMatrix.getRowGroupCount(), false);

    // compute initial component states and all choices that stay within a given EC
    for (auto state : maybeStates) {
        auto ecIndex = stateToFinalEc[state];
        if (ecIndex == initialComponentIndex) {
            initialComponentStates.set(state, true);
            continue;  // state part of the initial component
        } else if (ecIndex == std::numeric_limits<uint64_t>::max()) {
            continue;
        }
        for (auto choiceIndex : transitionMatrix.getRowGroupIndices(state)) {
            bool isEcStayChoice = true;
            for (auto const& entry : transitionMatrix.getRow(choiceIndex)) {
                auto targetState = entry.getColumn();
                if (stateToFinalEc[targetState] != ecIndex) {
                    isEcStayChoice = false;
                    break;
                }
            }
            if (isEcStayChoice) {
                ecStayChoices.set(choiceIndex, true);
            }
        }
    }

    // fill choices for ECs that reach the chosen EC exit
    auto const maybeNonICStatesWithoutChoice = maybeStatesWithoutChoice & ~initialComponentStates;
    storm::utility::graph::computeSchedulerProb1E(maybeNonICStatesWithoutChoice, transitionMatrix, backwardTransitions, maybeStates, maybeStatesWithChoice,
                                                  scheduler, ecStayChoices);

    // collect all choices from the initial component states and the choices that were selected by the scheduler so far
    auto const condOrTargetStates = normalForm.conditionStates | normalForm.targetStates;
    auto const& rowGroups = transitionMatrix.getRowGroupIndices();
    storm::storage::BitVector allowedChoices(transitionMatrix.getRowCount(), false);
    auto const rowGroupCount = transitionMatrix.getRowGroupCount();
    for (uint64_t state = 0; state < rowGroupCount; ++state) {
        if (scheduler.isChoiceSelected(state)) {
            auto choiceIndex = scheduler.getChoice(state).getDeterministicChoice();
            allowedChoices.set(rowGroups[state] + choiceIndex, true);
        } else if (initialComponentStates.get(state) || condOrTargetStates.get(state)) {
            for (auto choiceIndex : transitionMatrix.getRowGroupIndices(state)) {
                allowedChoices.set(choiceIndex, true);
            }
        }
    }

    // dfs to find which choices in initial component states lead to condOrTargetStates
    storm::storage::BitVector choicesThatCanVisitCondOrTargetStates(transitionMatrix.getRowCount(), false);
    std::stack<uint64_t> toProcess;
    for (auto state : condOrTargetStates) {
        toProcess.push(state);
    }
    auto visitedStates = condOrTargetStates;
    while (!toProcess.empty()) {
        auto currentState = toProcess.top();
        toProcess.pop();
        for (auto const& entry : backwardTransitions.getRow(currentState)) {
            uint64_t const predecessorState = entry.getColumn();
            for (uint64_t const predecessorChoice : transitionMatrix.getRowGroupIndices(predecessorState)) {
                if (!allowedChoices.get(predecessorChoice) || choicesThatCanVisitCondOrTargetStates.get(predecessorChoice)) {
                    continue;  // The choice is either not allowed or has been considered already
                }
                if (auto const r = transitionMatrix.getRow(predecessorChoice);
                    std::none_of(r.begin(), r.end(), [&currentState](auto const& e) { return e.getColumn() == currentState; })) {
                    continue;  // not an actual predecessor choice
                }
                choicesThatCanVisitCondOrTargetStates.set(predecessorChoice, true);
                if (!visitedStates.get(predecessorState)) {
                    visitedStates.set(predecessorState, true);
                    toProcess.push(predecessorState);
                }
            }
        }
    }

    // we want to disallow taking initial component exits that can lead to a condition or target state, beside the one exit that was chosen
    storm::storage::BitVector disallowedInitialComponentExits = initialComponentExitRows & choicesThatCanVisitCondOrTargetStates;
    disallowedInitialComponentExits.set(chosenInitialComponentExit, false);
    storm::storage::BitVector choicesAllowedForInitialComponent = allowedChoices & ~disallowedInitialComponentExits;

    storm::storage::BitVector goodInitialComponentStates = initialComponentStates;
    bool progress = false;
    for (auto state : initialComponentExitStates) {
        auto const groupStart = transitionMatrix.getRowGroupIndices()[state];
        auto const groupEnd = transitionMatrix.getRowGroupIndices()[state + 1];
        bool const allChoicesAreDisallowed = disallowedInitialComponentExits.getNextUnsetIndex(groupStart) >= groupEnd;
        if (allChoicesAreDisallowed) {
            goodInitialComponentStates.set(state, false);
            progress = true;
        }
    }
    while (progress) {
        progress = false;
        for (auto state : goodInitialComponentStates) {
            bool allChoicesAreDisallowed = true;
            for (auto choiceIndex : transitionMatrix.getRowGroupIndices(state)) {
                auto row = transitionMatrix.getRow(choiceIndex);
                bool const hasBadSuccessor = std::any_of(
                    row.begin(), row.end(), [&goodInitialComponentStates](auto const& entry) { return !goodInitialComponentStates.get(entry.getColumn()); });
                if (hasBadSuccessor) {
                    choicesAllowedForInitialComponent.set(choiceIndex, false);
                } else {
                    allChoicesAreDisallowed = false;
                }
            }
            if (allChoicesAreDisallowed) {
                goodInitialComponentStates.set(state, false);
                progress = true;
            }
        }
    }

    storm::storage::BitVector exitStateBitvector(transitionMatrix.getRowGroupCount(), false);
    exitStateBitvector.set(chosenInitialComponentExitState, true);

    storm::utility::graph::computeSchedulerProbGreater0E(transitionMatrix, backwardTransitions, initialComponentStates, exitStateBitvector, scheduler,
                                                         choicesAllowedForInitialComponent);

    // fill the choices of initial component states that do not have a choice yet
    // these states should not reach the condition or target states under the constructed scheduler
    for (auto state : initialComponentStates) {
        if (!scheduler.isChoiceSelected(state)) {
            for (auto choiceIndex : transitionMatrix.getRowGroupIndices(state)) {
                if (choicesAllowedForInitialComponent.get(choiceIndex)) {
                    scheduler.setChoice(choiceIndex - rowGroups[state], state);
                    break;
                }
            }
        }
    }
}

template<typename ValueType, typename SolutionType = ValueType>
struct ResultReturnType {
    ResultReturnType(ValueType initialStateValue, std::unique_ptr<storm::storage::Scheduler<ValueType>>&& scheduler = nullptr)
        : initialStateValue(initialStateValue), scheduler(std::move(scheduler)) {
        // Intentionally left empty.
    }

    bool hasScheduler() const {
        return static_cast<bool>(scheduler);
    }

    ValueType initialStateValue;
    std::unique_ptr<storm::storage::Scheduler<SolutionType>> scheduler;
};

/*!
 * Uses the restart method by Baier et al.
// @see doi.org/10.1007/978-3-642-54862-8_43
 */
template<typename ValueType, typename SolutionType = ValueType>
typename internal::ResultReturnType<ValueType> computeViaRestartMethod(Environment const& env, uint64_t const initialState,
                                                                       storm::solver::SolveGoal<ValueType, SolutionType> const& goal, bool computeScheduler,
                                                                       storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                       storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                       NormalFormData<ValueType> const& normalForm) {
    auto const& maybeStates = normalForm.maybeStates;
    auto originalToReducedStateIndexMap = maybeStates.getNumberOfSetBitsBeforeIndices();
    auto const numMaybeStates = maybeStates.getNumberOfSetBits();
    auto const numMaybeChoices = transitionMatrix.getNumRowsInRowGroups(maybeStates);

    // Build the transitions that include a backwards loop to the initial state
    storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder(numMaybeChoices, numMaybeStates, 0, true, true, numMaybeStates);
    std::vector<ValueType> rowValues;
    storm::storage::BitVector rowsWithSum1(numMaybeChoices, true);
    rowValues.reserve(numMaybeChoices);
    uint64_t currentRow = 0;
    for (auto state : maybeStates) {
        matrixBuilder.newRowGroup(currentRow);
        for (auto origRowIndex : transitionMatrix.getRowGroupIndices(state)) {
            // We make two passes over the successors. First, we find out the reset probabilities and target probabilities
            // Then, we insert the matrix entries in the correct order
            // This two-phase approach is to avoid a costly out-of-order insertion into the matrix
            ValueType targetProbability = storm::utility::zero<ValueType>();
            ValueType restartProbability = storm::utility::zero<ValueType>();
            bool rowSumIsLess1 = false;
            for (auto const& entry : transitionMatrix.getRow(origRowIndex)) {
                if (normalForm.terminalStates.get(entry.getColumn())) {
                    ValueType const targetValue = normalForm.getTargetValue(entry.getColumn());
                    targetProbability += targetValue * entry.getValue();
                    if (normalForm.conditionStates.get(entry.getColumn())) {
                        rowSumIsLess1 = true;
                    } else {
                        if (!storm::utility::isZero(targetValue)) {
                            rowSumIsLess1 = true;
                        }
                        restartProbability += entry.getValue() * normalForm.failProbability(entry.getColumn());
                    }
                }
            }
            if (rowSumIsLess1) {
                rowsWithSum1.set(currentRow, false);
            }
            rowValues.push_back(targetProbability);
            bool addRestartTransition = !storm::utility::isZero(restartProbability);
            for (auto const& entry : transitionMatrix.getRow(origRowIndex)) {
                // Insert backloop probability if we haven't done so yet and are past the initial state index
                // This is to avoid a costly out-of-order insertion into the matrix
                if (addRestartTransition && entry.getColumn() > initialState) {
                    matrixBuilder.addNextValue(currentRow, originalToReducedStateIndexMap[initialState], restartProbability);
                    addRestartTransition = false;
                }
                if (maybeStates.get(entry.getColumn())) {
                    matrixBuilder.addNextValue(currentRow, originalToReducedStateIndexMap[entry.getColumn()], entry.getValue());
                }
            }
            // Add the backloop if we haven't done this already
            if (addRestartTransition) {
                matrixBuilder.addNextValue(currentRow, originalToReducedStateIndexMap[initialState], restartProbability);
            }
            ++currentRow;
        }
    }
    STORM_LOG_ASSERT(currentRow == numMaybeChoices, "Unexpected number of constructed rows.");

    auto matrix = matrixBuilder.build();
    auto initStateInReduced = originalToReducedStateIndexMap[initialState];

    // Eliminate end components in two phases
    // First, we catch all end components that do not contain the initial state. It is possible to stay in those ECs forever
    // without reaching the condition. This is reflected by a backloop to the initial state.
    storm::storage::BitVector selectedStatesInReduced(numMaybeStates, true);
    selectedStatesInReduced.set(initStateInReduced, false);
    auto ecElimResult1 = eliminateEndComponents(selectedStatesInReduced, true, initStateInReduced, matrix, rowsWithSum1, rowValues);
    selectedStatesInReduced.set(initStateInReduced, true);
    if (ecElimResult1) {
        selectedStatesInReduced.resize(matrix.getRowGroupCount(), true);
        initStateInReduced = ecElimResult1->oldToNewStateMapping[initStateInReduced];
    }
    // Second, eliminate the remaining ECs. These must involve the initial state and might have been introduced in the previous step.
    // A policy selecting such an EC must reach the condition with probability zero and is thus invalid.
    auto ecElimResult2 = eliminateEndComponents(selectedStatesInReduced, false, std::nullopt, matrix, rowsWithSum1, rowValues);
    if (ecElimResult2) {
        initStateInReduced = ecElimResult2->oldToNewStateMapping[initStateInReduced];
    }

    STORM_LOG_INFO("Processed model has " << matrix.getRowGroupCount() << " states and " << matrix.getRowGroupCount() << " choices and "
                                          << matrix.getEntryCount() << " transitions.");

    // Finally, solve the equation system, potentially computing a scheduler
    std::optional<std::vector<uint64_t>> reducedSchedulerChoices;
    if (computeScheduler) {
        reducedSchedulerChoices.emplace();
    }
    auto resultValue = solveMinMaxEquationSystem(env, matrix, rowValues, rowsWithSum1, goal, initStateInReduced, reducedSchedulerChoices);

    // Create result (scheduler potentially added below)
    auto finalResult = ResultReturnType<ValueType, SolutionType>(resultValue);

    if (!computeScheduler) {
        return finalResult;
    }
    // At this point we have to reconstruct the scheduler for the original model
    STORM_LOG_ASSERT(reducedSchedulerChoices.has_value() && reducedSchedulerChoices->size() == matrix.getRowGroupCount(),
                     "Requested scheduler, but it was not computed or has invalid size.");
    // For easier access, we create and update some index mappings
    std::vector<uint64_t> originalRowToStateIndexMap;  // maps original row indices to original state indices. transitionMatrix.getRowGroupIndices() are the
                                                       // inverse of that mapping
    originalRowToStateIndexMap.reserve(transitionMatrix.getRowCount());
    for (uint64_t originalStateIndex = 0; originalStateIndex < transitionMatrix.getRowGroupCount(); ++originalStateIndex) {
        originalRowToStateIndexMap.insert(originalRowToStateIndexMap.end(), transitionMatrix.getRowGroupSize(originalStateIndex), originalStateIndex);
    }
    std::vector<uint64_t> reducedToOriginalRowIndexMap;  // maps row indices of the reduced model to the original ones
    reducedToOriginalRowIndexMap.reserve(numMaybeChoices);
    for (uint64_t const originalMaybeState : maybeStates) {
        for (auto const originalRowIndex : transitionMatrix.getRowGroupIndices(originalMaybeState)) {
            reducedToOriginalRowIndexMap.push_back(originalRowIndex);
        }
    }
    if (ecElimResult1.has_value() || ecElimResult2.has_value()) {
        // reducedToOriginalRowIndexMap needs to be updated so it maps from rows of the ec-eliminated system
        std::vector<uint64_t> tmpReducedToOriginalRowIndexMap;
        tmpReducedToOriginalRowIndexMap.reserve(matrix.getRowCount());
        for (uint64_t reducedRow = 0; reducedRow < matrix.getRowCount(); ++reducedRow) {
            uint64_t intermediateRow = reducedRow;
            if (ecElimResult2.has_value()) {
                intermediateRow = ecElimResult2->newToOldRowMapping.at(intermediateRow);
            }
            if (ecElimResult1.has_value()) {
                intermediateRow = ecElimResult1->newToOldRowMapping.at(intermediateRow);
            }
            tmpReducedToOriginalRowIndexMap.push_back(reducedToOriginalRowIndexMap[intermediateRow]);
        }
        reducedToOriginalRowIndexMap = std::move(tmpReducedToOriginalRowIndexMap);
        // originalToReducedStateIndexMap needs to be updated so it maps into the ec-eliminated system
        for (uint64_t originalStateIndex = 0; originalStateIndex < transitionMatrix.getRowGroupCount(); ++originalStateIndex) {
            auto& reducedIndex = originalToReducedStateIndexMap[originalStateIndex];
            if (maybeStates.get(originalStateIndex)) {
                if (ecElimResult1.has_value()) {
                    reducedIndex = ecElimResult1->oldToNewStateMapping.at(reducedIndex);
                }
                if (ecElimResult2.has_value()) {
                    reducedIndex = ecElimResult2->oldToNewStateMapping.at(reducedIndex);
                }
            } else {
                reducedIndex = std::numeric_limits<uint64_t>::max();  // The original state does not exist in the reduced model.
            }
        }
    }

    storm::storage::BitVector initialComponentExitRows(transitionMatrix.getRowCount(), false);
    storm::storage::BitVector initialComponentExitStates(transitionMatrix.getRowGroupCount(), false);
    for (auto const reducedRowIndex : matrix.getRowGroupIndices(initStateInReduced)) {
        uint64_t const originalRowIndex = reducedToOriginalRowIndexMap[reducedRowIndex];
        uint64_t const originalState = originalRowToStateIndexMap[originalRowIndex];

        initialComponentExitRows.set(originalRowIndex, true);
        initialComponentExitStates.set(originalState, true);
    }

    // If requested, construct the scheduler for the original model
    storm::storage::BitVector maybeStatesWithChoice(maybeStates.size(), false);
    uint64_t chosenInitialComponentExitState = std::numeric_limits<uint64_t>::max();
    uint64_t chosenInitialComponentExit = std::numeric_limits<uint64_t>::max();
    auto scheduler = std::make_unique<storm::storage::Scheduler<SolutionType>>(transitionMatrix.getRowGroupCount());

    uint64_t reducedState = 0;
    for (auto const& choice : reducedSchedulerChoices.value()) {
        uint64_t const reducedRowIndex = matrix.getRowGroupIndices()[reducedState] + choice;
        uint64_t const originalRowIndex = reducedToOriginalRowIndexMap[reducedRowIndex];
        uint64_t const originalState = originalRowToStateIndexMap[originalRowIndex];
        uint64_t const originalChoice = originalRowIndex - transitionMatrix.getRowGroupIndices()[originalState];
        scheduler->setChoice(originalChoice, originalState);
        maybeStatesWithChoice.set(originalState, true);
        if (reducedState == initStateInReduced) {
            chosenInitialComponentExitState = originalState;
            chosenInitialComponentExit = originalRowIndex;
        }
        ++reducedState;
    }

    auto const maybeStatesWithoutChoice = maybeStates & ~maybeStatesWithChoice;
    finalizeSchedulerForMaybeStates(*scheduler, transitionMatrix, backwardTransitions, maybeStates, maybeStatesWithoutChoice, maybeStatesWithChoice,
                                    originalToReducedStateIndexMap, normalForm, initStateInReduced, initialComponentExitStates, initialComponentExitRows,
                                    chosenInitialComponentExitState, chosenInitialComponentExit);

    finalResult.scheduler = std::move(scheduler);

    return finalResult;
}

/*!
 * A helper class that computes (weighted) reachability probabilities for a given MDP in normal form.
 * @tparam ValueType
 * @tparam SolutionType
 */
template<typename ValueType, typename SolutionType = ValueType>
class WeightedReachabilityHelper {
   public:
    WeightedReachabilityHelper(uint64_t const initialState, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                               NormalFormData<ValueType> const& normalForm, bool computeScheduler) {
        // Determine rowgroups (states) and rows (choices) of the submatrix
        auto subMatrixRowGroups = normalForm.maybeStates;
        // Identify and eliminate the initial component to enforce that it is eventually exited
        // The initial component is the largest subset of maybestates C such that
        // (i) the initial state is contained in C
        // (ii) each state in C can be reached from the initial state while only playing actions that stay inside C or observation failure and
        // (iii) for each state in C except the initial state there is a policy that almost surely reaches an observation failure
        // An optimal scheduler can intuitively pick the best exiting action of C and enforce that all paths that satisfy the condition exit C through that
        // action. By eliminating the initial component, we ensure that only policies that actually exit C are considered. The remaining policies have
        // probability zero of satisfying the condition.
        initialComponentExitRows = storm::storage::BitVector(transitionMatrix.getRowCount(), false);
        initialComponentExitStates = storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false);
        subMatrixRowGroups.set(initialState, false);  // temporarily unset initial state
        std::vector<uint64_t> dfsStack = {initialState};
        while (!dfsStack.empty()) {
            auto const state = dfsStack.back();
            dfsStack.pop_back();
            for (auto rowIndex : transitionMatrix.getRowGroupIndices(state)) {
                auto const row = transitionMatrix.getRow(rowIndex);
                if (std::all_of(row.begin(), row.end(),
                                [&normalForm](auto const& entry) { return normalForm.existentialObservationFailureStates.get(entry.getColumn()); })) {
                    for (auto const& entry : row) {
                        auto const& successor = entry.getColumn();
                        if (subMatrixRowGroups.get(successor)) {
                            subMatrixRowGroups.set(successor, false);
                            dfsStack.push_back(successor);
                        }
                    }
                } else {
                    initialComponentExitRows.set(rowIndex, true);
                    initialComponentExitStates.set(state, true);
                }
            }
        }
        auto const numSubmatrixRows = transitionMatrix.getNumRowsInRowGroups(subMatrixRowGroups) + initialComponentExitRows.getNumberOfSetBits();
        subMatrixRowGroups.set(initialState, true);  // set initial state again, as single representative state for the initial component
        auto const numSubmatrixRowGroups = subMatrixRowGroups.getNumberOfSetBits();

        if (computeScheduler) {
            reducedToOriginalRowIndexMap.reserve(numSubmatrixRows);
        }

        // state index mapping and initial state
        originalToReducedStateIndexMap = subMatrixRowGroups.getNumberOfSetBitsBeforeIndices();
        initialStateInSubmatrix = originalToReducedStateIndexMap[initialState];
        auto const eliminatedInitialComponentStates = normalForm.maybeStates & ~subMatrixRowGroups;

        // Inital component states do not have the correct mapping yet.
        for (auto state : eliminatedInitialComponentStates) {
            originalToReducedStateIndexMap[state] = initialStateInSubmatrix;
        }

        // build matrix, rows that sum up to 1, target values, condition values
        storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder(numSubmatrixRows, numSubmatrixRowGroups, 0, true, true, numSubmatrixRowGroups);
        rowsWithSum1 = storm::storage::BitVector(numSubmatrixRows, true);
        targetRowValues.reserve(numSubmatrixRows);
        conditionRowValues.reserve(numSubmatrixRows);
        uint64_t currentRow = 0;
        for (auto state : subMatrixRowGroups) {
            matrixBuilder.newRowGroup(currentRow);

            // Put the row processing into a lambda for avoiding code duplications
            auto processRow = [&](uint64_t origRowIndex) {
                // insert the submatrix entries and find out the target and condition probabilities for this row
                ValueType targetProbability = storm::utility::zero<ValueType>();
                ValueType conditionProbability = storm::utility::zero<ValueType>();
                bool rowSumIsLess1 = false;
                for (auto const& entry : transitionMatrix.getRow(origRowIndex)) {
                    if (normalForm.terminalStates.get(entry.getColumn())) {
                        STORM_LOG_ASSERT(!storm::utility::isZero(entry.getValue()), "Transition probability must be non-zero");
                        rowSumIsLess1 = true;
                        ValueType const scaledTargetValue = normalForm.getTargetValue(entry.getColumn()) * entry.getValue();
                        targetProbability += scaledTargetValue;
                        if (normalForm.conditionStates.get(entry.getColumn())) {
                            conditionProbability += entry.getValue();  // conditionValue of successor is 1
                        } else {
                            conditionProbability += scaledTargetValue;  // for terminal, non-condition states, the condition value equals the target value
                        }
                    } else if (eliminatedInitialComponentStates.get(entry.getColumn())) {
                        rowSumIsLess1 = true;
                    } else {
                        auto const columnIndex = originalToReducedStateIndexMap[entry.getColumn()];
                        matrixBuilder.addNextValue(currentRow, columnIndex, entry.getValue());
                    }
                }
                if (rowSumIsLess1) {
                    rowsWithSum1.set(currentRow, false);
                }
                targetRowValues.push_back(targetProbability);
                conditionRowValues.push_back(conditionProbability);
                ++currentRow;
            };
            // invoke the lambda
            if (state == initialState) {
                for (auto origRowIndex : initialComponentExitRows) {
                    processRow(origRowIndex);
                    reducedToOriginalRowIndexMap.push_back(origRowIndex);
                }
            } else {
                for (auto origRowIndex : transitionMatrix.getRowGroupIndices(state)) {
                    processRow(origRowIndex);
                    reducedToOriginalRowIndexMap.push_back(origRowIndex);
                }
            }
        }
        submatrix = matrixBuilder.build();

        //  eliminate ECs if present. We already checked that the initial state can not yield observation failure, so it cannot be part of an EC.
        //  For all remaining ECs, staying in an EC forever is reflected by collecting a value of zero for both, target and condition
        storm::storage::BitVector allExceptInit(numSubmatrixRowGroups, true);
        allExceptInit.set(initialStateInSubmatrix, false);
        ecResult = eliminateEndComponents<ValueType>(allExceptInit, true, std::nullopt, submatrix, rowsWithSum1, targetRowValues, conditionRowValues);
        if (ecResult) {
            initialStateInSubmatrix = ecResult->oldToNewStateMapping[initialStateInSubmatrix];
        }
        isAcyclic = !storm::utility::graph::hasCycle(submatrix);
        STORM_LOG_INFO("Processed model has " << submatrix.getRowGroupCount() << " states and " << submatrix.getRowGroupCount() << " choices and "
                                              << submatrix.getEntryCount() << " transitions. Matrix is " << (isAcyclic ? "acyclic." : "cyclic."));

        if (computeScheduler) {
            // For easier conversion of schedulers to the original model, we create and update some index mappings
            STORM_LOG_ASSERT(reducedToOriginalRowIndexMap.size() == numSubmatrixRows, "Unexpected size of reducedToOriginalRowIndexMap.");
            if (ecResult.has_value()) {
                // reducedToOriginalRowIndexMap needs to be updated so it maps from rows of the ec-eliminated system
                std::vector<uint64_t> tmpReducedToOriginalRowIndexMap;
                tmpReducedToOriginalRowIndexMap.reserve(submatrix.getRowCount());
                for (uint64_t reducedRow = 0; reducedRow < submatrix.getRowCount(); ++reducedRow) {
                    uint64_t intermediateRow = ecResult->newToOldRowMapping.at(reducedRow);
                    tmpReducedToOriginalRowIndexMap.push_back(reducedToOriginalRowIndexMap[intermediateRow]);
                }
                reducedToOriginalRowIndexMap = std::move(tmpReducedToOriginalRowIndexMap);
                // originalToReducedStateIndexMap needs to be updated so it maps into the ec-eliminated system
                for (uint64_t originalStateIndex = 0; originalStateIndex < transitionMatrix.getRowGroupCount(); ++originalStateIndex) {
                    auto& reducedIndex = originalToReducedStateIndexMap[originalStateIndex];
                    if (subMatrixRowGroups.get(originalStateIndex)) {
                        reducedIndex = ecResult->oldToNewStateMapping.at(reducedIndex);
                    } else {
                        reducedIndex = std::numeric_limits<uint64_t>::max();  // The original state does not exist in the reduced model.
                    }
                }
            }
        } else {
            // Clear data that is only needed if we compute schedulers
            originalToReducedStateIndexMap.clear();
            reducedToOriginalRowIndexMap.clear();
            ecResult.emplace();
            initialComponentExitRows.clear();
            initialComponentExitStates.clear();
        }
    }

    SolutionType computeWeightedDiff(storm::Environment const& env, storm::OptimizationDirection const dir, ValueType const& targetWeight,
                                     ValueType const& conditionWeight, storm::OptionalRef<std::vector<uint64_t>> schedulerOutput = {}) {
        // Set up the solver.
        if (!cachedSolver) {
            auto solverEnv = env;
            if (isAcyclic) {
                STORM_LOG_INFO("Using acyclic min-max solver for weighted reachability computation.");
                solverEnv.solver().minMax().setMethod(storm::solver::MinMaxMethod::Acyclic);
            }
            cachedSolver = storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType, SolutionType>().create(solverEnv, submatrix);
            cachedSolver->setCachingEnabled(true);
            cachedSolver->setRequirementsChecked();
            cachedSolver->setHasUniqueSolution(true);
            cachedSolver->setHasNoEndComponents(true);
            cachedSolver->setLowerBound(-storm::utility::one<ValueType>());
            cachedSolver->setUpperBound(storm::utility::one<ValueType>());
        }
        cachedSolver->setTrackScheduler(schedulerOutput.has_value());
        cachedSolver->setOptimizationDirection(dir);

        // Initialize the right-hand side vector.
        createScaledVector(cachedB, targetWeight, targetRowValues, conditionWeight, conditionRowValues);

        // Initialize the solution vector.
        cachedX.assign(submatrix.getRowGroupCount(), storm::utility::zero<ValueType>());

        cachedSolver->solveEquations(env, cachedX, cachedB);
        if (schedulerOutput) {
            *schedulerOutput = cachedSolver->getSchedulerChoices();
        }
        return cachedX[initialStateInSubmatrix];
    }

    auto getInternalInitialState() const {
        return initialStateInSubmatrix;
    }

    void evaluateScheduler(storm::Environment const& env, std::vector<uint64_t>& scheduler, std::vector<SolutionType>& targetResults,
                           std::vector<SolutionType>& conditionResults) {
        if (scheduler.empty()) {
            scheduler.resize(submatrix.getRowGroupCount(), 0);
        }
        if (targetResults.empty()) {
            targetResults.resize(submatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
        }
        if (conditionResults.empty()) {
            conditionResults.resize(submatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
        }
        auto solver = getScheduledSolver(env, scheduler);

        cachedB.resize(submatrix.getRowGroupCount());
        storm::utility::vector::selectVectorValues<ValueType>(cachedB, scheduler, submatrix.getRowGroupIndices(), targetRowValues);
        solver->solveEquations(env, targetResults, cachedB);

        storm::utility::vector::selectVectorValues<ValueType>(cachedB, scheduler, submatrix.getRowGroupIndices(), conditionRowValues);
        solver->solveEquations(env, conditionResults, cachedB);
    }

    SolutionType evaluateScheduler(storm::Environment const& env, std::vector<uint64_t> const& scheduler) {
        STORM_LOG_ASSERT(scheduler.size() == submatrix.getRowGroupCount(), "Scheduler size does not match number of row groups");
        auto solver = getScheduledSolver(env, scheduler);
        cachedB.resize(submatrix.getRowGroupCount());
        cachedX.resize(submatrix.getRowGroupCount());

        storm::utility::vector::selectVectorValues<ValueType>(cachedB, scheduler, submatrix.getRowGroupIndices(), targetRowValues);
        solver->solveEquations(env, cachedX, cachedB);
        SolutionType targetValue = cachedX[initialStateInSubmatrix];

        storm::utility::vector::selectVectorValues<ValueType>(cachedB, scheduler, submatrix.getRowGroupIndices(), conditionRowValues);
        solver->solveEquations(env, cachedX, cachedB);
        SolutionType conditionValue = cachedX[initialStateInSubmatrix];

        return targetValue / conditionValue;
    }

    template<OptimizationDirection Dir>
    bool improveScheduler(std::vector<uint64_t>& scheduler, ValueType const& lambda, std::vector<SolutionType> const& targetResults,
                          std::vector<SolutionType> const& conditionResults) {
        bool improved = false;
        for (uint64_t rowGroupIndex = 0; rowGroupIndex < scheduler.size(); ++rowGroupIndex) {
            storm::utility::Extremum<Dir, ValueType> groupValue;
            uint64_t optimalRowIndex{0};
            ValueType scheduledValue;
            for (auto rowIndex : submatrix.getRowGroupIndices(rowGroupIndex)) {
                ValueType rowValue = targetRowValues[rowIndex] - lambda * conditionRowValues[rowIndex];
                for (auto const& entry : submatrix.getRow(rowIndex)) {
                    rowValue += entry.getValue() * (targetResults[entry.getColumn()] - lambda * conditionResults[entry.getColumn()]);
                }
                if (rowIndex == scheduler[rowGroupIndex] + submatrix.getRowGroupIndices()[rowGroupIndex]) {
                    scheduledValue = rowValue;
                }
                if (groupValue &= rowValue) {
                    optimalRowIndex = rowIndex;
                }
            }
            if (scheduledValue != *groupValue) {
                scheduler[rowGroupIndex] = optimalRowIndex - submatrix.getRowGroupIndices()[rowGroupIndex];
                improved = true;
            }
        }
        return improved;
    }

    std::unique_ptr<storm::storage::Scheduler<ValueType>> constructSchedulerForInputModel(
        std::vector<uint64_t> const& schedulerForReducedModel, storm::storage::SparseMatrix<ValueType> const& originalTransitionMatrix,
        storm::storage::SparseMatrix<ValueType> const& originalBackwardTransitions, NormalFormData<ValueType> const& normalForm) const {
        std::vector<uint64_t> originalRowToStateIndexMap;  // maps original row indices to original state indices. transitionMatrix.getRowGroupIndices() are
                                                           // the inverse of that mapping
        originalRowToStateIndexMap.reserve(originalTransitionMatrix.getRowCount());
        for (uint64_t originalStateIndex = 0; originalStateIndex < originalTransitionMatrix.getRowGroupCount(); ++originalStateIndex) {
            originalRowToStateIndexMap.insert(originalRowToStateIndexMap.end(), originalTransitionMatrix.getRowGroupSize(originalStateIndex),
                                              originalStateIndex);
        }

        storm::storage::BitVector maybeStatesWithChoice(normalForm.maybeStates.size(), false);
        uint64_t chosenInitialComponentExitState = std::numeric_limits<uint64_t>::max();
        uint64_t chosenInitialComponentExit = std::numeric_limits<uint64_t>::max();
        auto scheduler = std::make_unique<storm::storage::Scheduler<SolutionType>>(originalTransitionMatrix.getRowGroupCount());

        uint64_t reducedState = 0;
        for (auto const& choice : schedulerForReducedModel) {
            uint64_t const reducedRowIndex = submatrix.getRowGroupIndices()[reducedState] + choice;
            uint64_t const originalRowIndex = reducedToOriginalRowIndexMap[reducedRowIndex];
            uint64_t const originalState = originalRowToStateIndexMap[originalRowIndex];
            uint64_t const originalChoice = originalRowIndex - originalTransitionMatrix.getRowGroupIndices()[originalState];
            scheduler->setChoice(originalChoice, originalState);
            maybeStatesWithChoice.set(originalState, true);
            if (reducedState == initialStateInSubmatrix) {
                chosenInitialComponentExitState = originalState;
                chosenInitialComponentExit = originalRowIndex;
            }
            ++reducedState;
        }

        auto const maybeStatesWithoutChoice = normalForm.maybeStates & ~maybeStatesWithChoice;
        finalizeSchedulerForMaybeStates(*scheduler, originalTransitionMatrix, originalBackwardTransitions, normalForm.maybeStates, maybeStatesWithoutChoice,
                                        maybeStatesWithChoice, originalToReducedStateIndexMap, normalForm, initialStateInSubmatrix, initialComponentExitStates,
                                        initialComponentExitRows, chosenInitialComponentExitState, chosenInitialComponentExit);
        return scheduler;
    }

   private:
    void createScaledVector(std::vector<ValueType>& out, ValueType const& w1, std::vector<ValueType> const& v1, ValueType const& w2,
                            std::vector<ValueType> const& v2) const {
        STORM_LOG_ASSERT(v1.size() == v2.size(), "Vector sizes must match");
        out.resize(v1.size());
        storm::utility::vector::applyPointwise(v1, v2, out, [&w1, &w2](ValueType const& a, ValueType const& b) -> ValueType { return w1 * a + w2 * b; });
    }

    auto getScheduledSolver(storm::Environment const& env, std::vector<uint64_t> const& scheduler) const {
        // apply the scheduler
        storm::solver::GeneralLinearEquationSolverFactory<ValueType> factory;
        bool const convertToEquationSystem = factory.getEquationProblemFormat(env) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem;
        auto scheduledMatrix = submatrix.selectRowsFromRowGroups(scheduler, convertToEquationSystem);
        if (convertToEquationSystem) {
            scheduledMatrix.convertToEquationSystem();
        }
        auto solver = factory.create(env, std::move(scheduledMatrix));
        solver->setBounds(storm::utility::zero<ValueType>(), storm::utility::one<ValueType>());
        solver->setCachingEnabled(true);
        return solver;
    }

    storm::storage::SparseMatrix<ValueType> submatrix;
    storm::storage::BitVector rowsWithSum1;
    std::vector<ValueType> targetRowValues;
    std::vector<ValueType> conditionRowValues;
    uint64_t initialStateInSubmatrix;
    bool isAcyclic;
    std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType, SolutionType>> cachedSolver;
    std::vector<ValueType> cachedX;
    std::vector<ValueType> cachedB;

    // Data used to translate schedulers:
    std::vector<uint64_t> originalToReducedStateIndexMap;
    std::vector<uint64_t> reducedToOriginalRowIndexMap;
    std::optional<typename storm::transformer::EndComponentEliminator<ValueType>::EndComponentEliminatorReturnType> ecResult;
    storm::storage::BitVector initialComponentExitRows;
    storm::storage::BitVector initialComponentExitStates;
};

template<typename ValueType, typename SolutionType = ValueType>
typename internal::ResultReturnType<ValueType> computeViaBisection(Environment const& env, bool const useAdvancedBounds, bool const usePolicyTracking,
                                                                   uint64_t const initialState, storm::solver::SolveGoal<ValueType, SolutionType> const& goal,
                                                                   bool computeScheduler, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                   storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                   NormalFormData<ValueType> const& normalForm) {
    // We currently handle sound model checking incorrectly: we would need the actual lower/upper bounds of the weightedReachabilityHelper
    STORM_LOG_WARN_COND(!env.solver().isForceSoundness(),
                        "Bisection method does not adequately handle propagation of errors. Result is not necessarily sound.");

    bool const relative = env.modelchecker().conditional().isRelativePrecision();
    auto const precision = storm::utility::convertNumber<SolutionType>(env.modelchecker().conditional().getPrecision());

    WeightedReachabilityHelper wrh(initialState, transitionMatrix, normalForm, computeScheduler);
    SolutionType pMin{storm::utility::zero<SolutionType>()};
    SolutionType pMax{storm::utility::one<SolutionType>()};

    if (useAdvancedBounds) {
        pMin = wrh.computeWeightedDiff(env, storm::OptimizationDirection::Minimize, storm::utility::zero<ValueType>(), storm::utility::one<ValueType>());
        pMax = wrh.computeWeightedDiff(env, storm::OptimizationDirection::Maximize, storm::utility::zero<ValueType>(), storm::utility::one<ValueType>());
        STORM_LOG_TRACE("Conditioning event bounds:\n\t Lower bound: " << storm::utility::convertNumber<double>(pMin)
                                                                       << ",\n\t Upper bound: " << storm::utility::convertNumber<double>(pMax));
    }
    storm::utility::Maximum<SolutionType> lowerBound = storm::utility::zero<ValueType>();
    storm::utility::Minimum<SolutionType> upperBound = storm::utility::one<ValueType>();

    std::optional<std::vector<uint64_t>> lowerScheduler, upperScheduler, middleScheduler;
    storm::OptionalRef<std::vector<uint64_t>> middleSchedulerRef;
    if (usePolicyTracking) {
        lowerScheduler.emplace();
        upperScheduler.emplace();
        middleScheduler.emplace();
        middleSchedulerRef.reset(*middleScheduler);
    }

    SolutionType middle = goal.isBounded() ? goal.thresholdValue() : (*lowerBound + *upperBound) / 2;
    [[maybe_unused]] SolutionType rationalCandiate = middle;  // relevant for exact computations
    [[maybe_unused]] uint64_t rationalCandidateCount = 0;
    std::set<SolutionType> checkedMiddleValues;  // Middle values that have been checked already
    bool terminatedThroughPolicyTracking = false;
    for (uint64_t iterationCount = 1; true; ++iterationCount) {
        // evaluate the current middle
        SolutionType const middleValue = wrh.computeWeightedDiff(env, goal.direction(), storm::utility::one<ValueType>(), -middle, middleSchedulerRef);
        checkedMiddleValues.insert(middle);
        // update the bounds and new middle value according to the bisection method
        if (!useAdvancedBounds) {
            if (middleValue >= storm::utility::zero<ValueType>()) {
                if (lowerBound &= middle) {
                    lowerScheduler.swap(middleScheduler);
                }
            }
            if (middleValue <= storm::utility::zero<ValueType>()) {
                if (upperBound &= middle) {
                    upperScheduler.swap(middleScheduler);
                }
            }
            middle = (*lowerBound + *upperBound) / 2;  // update middle to the average of the bounds
        } else {
            if (middleValue >= storm::utility::zero<ValueType>()) {
                if (lowerBound &= middle + (middleValue / pMax)) {
                    lowerScheduler.swap(middleScheduler);
                }
                upperBound &= middle + (middleValue / pMin);
            }
            if (middleValue <= storm::utility::zero<ValueType>()) {
                lowerBound &= middle + (middleValue / pMin);
                if (upperBound &= middle + (middleValue / pMax)) {
                    upperScheduler.swap(middleScheduler);
                }
            }
            // update middle to the average of the bounds, but use the middleValue as a hint:
            // If middleValue is close to -1, we use a value close to lowerBound
            // If middleValue is close to 0, we use a value close to the avg(lowerBound, upperBound)
            // If middleValue is close to +1, we use a value close to upperBound
            middle = *lowerBound + (storm::utility::one<SolutionType>() + middleValue) * (*upperBound - *lowerBound) / 2;

            if (!storm::NumberTraits<SolutionType>::IsExact && storm::utility::isAlmostZero(*upperBound - *lowerBound)) {
                if (*lowerBound > *upperBound) {
                    std::swap(*lowerBound, *upperBound);
                }
            } else {
                STORM_LOG_ASSERT(middle >= *lowerBound && middle <= *upperBound, "Bisection method bounds are inconsistent.");
            }
        }
        // check for convergence
        SolutionType const boundDiff = *upperBound - *lowerBound;
        STORM_LOG_TRACE("Iteration #" << iterationCount << ":\n\t Lower bound:      " << *lowerBound << ",\n\t Upper bound:      " << *upperBound
                                      << ",\n\t Difference:       " << boundDiff << ",\n\t Middle val:       " << middleValue
                                      << ",\n\t Difference bound: " << (relative ? (precision * *lowerBound) : precision) << ".");
        if (goal.isBounded()) {
            STORM_LOG_TRACE("Using threshold " << storm::utility::convertNumber<double>(goal.thresholdValue()) << " with comparison "
                                               << (goal.boundIsALowerBound() ? (goal.boundIsStrict() ? ">" : ">=") : (goal.boundIsStrict() ? "<" : "<="))
                                               << ".");
        }
        if (boundDiff <= (relative ? (precision * *lowerBound) : precision)) {
            STORM_LOG_INFO("Bisection method converged after " << iterationCount << " iterations. Difference is "
                                                               << std::setprecision(std::numeric_limits<double>::digits10)
                                                               << storm::utility::convertNumber<double>(boundDiff) << ".");
            break;
        } else if (usePolicyTracking && lowerScheduler && upperScheduler && (*lowerScheduler == *upperScheduler)) {
            STORM_LOG_INFO("Bisection method converged after " << iterationCount << " iterations due to identical schedulers for lower and upper bound.");
            auto result = wrh.evaluateScheduler(env, *lowerScheduler);
            lowerBound &= result;
            upperBound &= result;
            terminatedThroughPolicyTracking = true;
            break;
        }
        // Check if bounds are fully below or above threshold
        if (goal.isBounded() && (*upperBound <= goal.thresholdValue() || (*lowerBound >= goal.thresholdValue()))) {
            STORM_LOG_INFO("Bisection method determined result after " << iterationCount << " iterations. Found bounds are ["
                                                                       << storm::utility::convertNumber<double>(*lowerBound) << ", "
                                                                       << storm::utility::convertNumber<double>(*upperBound) << "], threshold is "
                                                                       << storm::utility::convertNumber<double>(goal.thresholdValue()) << ".");
            break;
        }
        if (!storm::NumberTraits<SolutionType>::IsExact && storm::utility::isAlmostZero(boundDiff)) {
            STORM_LOG_WARN("Precision of non-exact type exceeded: Bisection method has not terminated, but the difference between upper and lower bound is "
                           << boundDiff << ".");
        }
        // check for early termination
        if (storm::utility::resources::isTerminate()) {
            STORM_LOG_WARN("Bisection solver aborted after " << iterationCount << "iterations. Bound difference is "
                                                             << storm::utility::convertNumber<double>(boundDiff) << ".");
            break;
        }
        // process the middle value for the next iteration
        // This sets the middle value to a rational number with the smallest enumerator/denominator that is still within the bounds
        // With close bounds this can lead to the middle being set to exactly the lower or upper bound, thus allowing for an exact answer.
        if constexpr (storm::NumberTraits<SolutionType>::IsExact) {
            // Check if the rationalCandidate has been within the bounds for four iterations.
            // If yes, we take that as our next "middle".
            // Otherwise, we set a new rationalCandidate.
            // This heuristic ensures that we eventually check every rational number without affecting the binary search too much
            if (rationalCandidateCount >= 4 && rationalCandiate >= *lowerBound && rationalCandiate <= *upperBound &&
                !checkedMiddleValues.contains(rationalCandiate)) {
                middle = rationalCandiate;
                rationalCandidateCount = 0;
            } else {
                // find a rational number with a concise representation within our current bounds
                bool const includeLower = !checkedMiddleValues.contains(*lowerBound);
                bool const includeUpper = !checkedMiddleValues.contains(*upperBound);
                auto newRationalCandiate = storm::utility::findRational(*lowerBound, includeLower, *upperBound, includeUpper);
                if (rationalCandiate == newRationalCandiate) {
                    ++rationalCandidateCount;
                } else {
                    rationalCandiate = newRationalCandiate;
                    rationalCandidateCount = 0;
                }
                // Also simplify the middle value
                SolutionType delta =
                    std::min<SolutionType>(*upperBound - middle, middle - *lowerBound) / storm::utility::convertNumber<SolutionType, uint64_t>(16);
                middle = storm::utility::findRational(middle - delta, true, middle + delta, true);
            }
        }
        // Since above code might never set 'middle' to exactly zero or one, we check if that could be necessary after a couple of iterations
        if (iterationCount == 8) {  // 8 is just a heuristic value, it could be any number
            if (storm::utility::isZero(*lowerBound) && !checkedMiddleValues.contains(storm::utility::zero<SolutionType>())) {
                middle = storm::utility::zero<SolutionType>();
            } else if (storm::utility::isOne(*upperBound) && !checkedMiddleValues.contains(storm::utility::one<SolutionType>())) {
                middle = storm::utility::one<SolutionType>();
            }
        }
    }

    // Create result without scheduler
    auto finalResult = ResultReturnType<ValueType>((*lowerBound + *upperBound) / 2);

    if (!computeScheduler) {
        return finalResult;  // nothing else to do
    }
    // If requested, construct the scheduler for the original model
    std::vector<uint64_t> reducedSchedulerChoices;
    if (terminatedThroughPolicyTracking) {
        // We already have computed a scheduler
        reducedSchedulerChoices = std::move(*lowerScheduler);
    } else {
        // Compute a scheduler on the middle result by performing one more iteration
        wrh.computeWeightedDiff(env, goal.direction(), storm::utility::one<ValueType>(), -finalResult.initialStateValue, reducedSchedulerChoices);
    }
    finalResult.scheduler = wrh.constructSchedulerForInputModel(reducedSchedulerChoices, transitionMatrix, backwardTransitions, normalForm);
    return finalResult;
}

template<typename ValueType, typename SolutionType = ValueType>
typename internal::ResultReturnType<ValueType> computeViaBisection(Environment const& env, ConditionalAlgorithmSetting const alg, uint64_t const initialState,
                                                                   storm::solver::SolveGoal<ValueType, SolutionType> const& goal, bool computeScheduler,
                                                                   storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                   storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                   NormalFormData<ValueType> const& normalForm) {
    using enum ConditionalAlgorithmSetting;
    STORM_LOG_ASSERT(alg == Bisection || alg == BisectionAdvanced || alg == BisectionPolicyTracking || alg == BisectionAdvancedPolicyTracking,
                     "Unhandled Bisection algorithm " << alg << ".");
    bool const useAdvancedBounds = (alg == BisectionAdvanced || alg == BisectionAdvancedPolicyTracking);
    bool const usePolicyTracking = (alg == BisectionPolicyTracking || alg == BisectionAdvancedPolicyTracking);
    return computeViaBisection(env, useAdvancedBounds, usePolicyTracking, initialState, goal, computeScheduler, transitionMatrix, backwardTransitions,
                               normalForm);
}

template<typename ValueType, typename SolutionType = ValueType>
typename internal::ResultReturnType<ValueType> decideThreshold(Environment const& env, uint64_t const initialState,
                                                               storm::OptimizationDirection const& direction, SolutionType const& threshold,
                                                               bool computeScheduler, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                               storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                               NormalFormData<ValueType> const& normalForm) {
    // We currently handle sound model checking incorrectly: we would need the actual lower/upper bounds of the weightedReachabilityHelper

    WeightedReachabilityHelper wrh(initialState, transitionMatrix, normalForm, computeScheduler);

    std::optional<std::vector<uint64_t>> scheduler;
    storm::OptionalRef<std::vector<uint64_t>> schedulerRef;
    if (computeScheduler) {
        scheduler.emplace();
        schedulerRef.reset(*scheduler);
    }

    SolutionType val = wrh.computeWeightedDiff(env, direction, storm::utility::one<ValueType>(), -threshold, schedulerRef);
    SolutionType outputProbability;
    if (val > storm::utility::zero<SolutionType>()) {
        // if val is positive, the conditional probability is (strictly) greater than threshold
        outputProbability = storm::utility::one<SolutionType>();
    } else if (val < storm::utility::zero<SolutionType>()) {
        // if val is negative, the conditional probability is (strictly) smaller than threshold
        outputProbability = storm::utility::zero<SolutionType>();
    } else {
        // if val is zero, the conditional probability equals the threshold
        outputProbability = threshold;
    }
    auto finalResult = ResultReturnType<SolutionType>(outputProbability);

    if (computeScheduler) {
        // If requested, construct the scheduler for the original model
        finalResult.scheduler = wrh.constructSchedulerForInputModel(scheduler.value(), transitionMatrix, backwardTransitions, normalForm);
    }
    return finalResult;
}

template<typename ValueType, typename SolutionType = ValueType>
internal::ResultReturnType<SolutionType> computeViaPolicyIteration(Environment const& env, uint64_t const initialState,
                                                                   storm::solver::OptimizationDirection const dir,
                                                                   storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                   NormalFormData<ValueType> const& normalForm) {
    WeightedReachabilityHelper wrh(initialState, transitionMatrix, normalForm, false);  // scheduler computation not yet implemented.

    std::vector<uint64_t> scheduler;
    std::vector<SolutionType> targetResults, conditionResults;
    for (uint64_t iterationCount = 1; true; ++iterationCount) {
        wrh.evaluateScheduler(env, scheduler, targetResults, conditionResults);
        STORM_LOG_WARN_COND(
            targetResults[wrh.getInternalInitialState()] <= conditionResults[wrh.getInternalInitialState()],
            "Potential numerical issues: the probability to reach the target is greater than the probability to reach the condition. Difference is "
                << (storm::utility::convertNumber<double, ValueType>(targetResults[wrh.getInternalInitialState()] -
                                                                     conditionResults[wrh.getInternalInitialState()]))
                << ".");
        ValueType const lambda = storm::utility::isZero(conditionResults[wrh.getInternalInitialState()])
                                     ? storm::utility::zero<ValueType>()
                                     : ValueType(targetResults[wrh.getInternalInitialState()] / conditionResults[wrh.getInternalInitialState()]);
        bool schedulerChanged{false};
        if (storm::solver::minimize(dir)) {
            schedulerChanged = wrh.template improveScheduler<storm::OptimizationDirection::Minimize>(scheduler, lambda, targetResults, conditionResults);
        } else {
            schedulerChanged = wrh.template improveScheduler<storm::OptimizationDirection::Maximize>(scheduler, lambda, targetResults, conditionResults);
        }
        if (!schedulerChanged) {
            STORM_LOG_INFO("Policy iteration for conditional probabilities converged after " << iterationCount << " iterations.");
            return lambda;
        }
        if (storm::utility::resources::isTerminate()) {
            STORM_LOG_WARN("Policy iteration for conditional probabilities converged aborted after " << iterationCount << "iterations.");
            return lambda;
        }
    }
}

template<typename ValueType, typename SolutionType = ValueType>
std::optional<SolutionType> handleTrivialCases(uint64_t const initialState, NormalFormData<ValueType> const& normalForm) {
    if (normalForm.terminalStates.get(initialState)) {
        STORM_LOG_DEBUG("Initial state is terminal.");
        if (normalForm.conditionStates.get(initialState)) {
            return normalForm.getTargetValue(initialState);  // The value is already known, nothing to do.
        } else {
            STORM_LOG_THROW(!normalForm.universalObservationFailureStates.get(initialState), storm::exceptions::NotSupportedException,
                            "Trying to compute undefined conditional probability: the condition has probability 0 under all policies.");
            // The last case for a terminal initial state is that it is already target and the condition is reachable with non-zero probability.
            // In this case, all schedulers induce a conditional probability of 1 (or do not reach the condition, i.e., have undefined value)
            return storm::utility::one<SolutionType>();
        }
    } else {
        // Catch the case where all terminal states have value zero
        if (normalForm.nonZeroTargetStateValues.empty()) {
            return storm::utility::zero<SolutionType>();
        }
    }
    return std::nullopt;  // No trivial case applies, we need to compute the value.
}

}  // namespace internal

template<typename ValueType, typename SolutionType>
std::unique_ptr<CheckResult> computeConditionalProbabilities(Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal,
                                                             bool produceSchedulers, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                             storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                             storm::storage::BitVector const& targetStates, storm::storage::BitVector const& conditionStates) {
    auto precision = env.modelchecker().conditional().getPrecision();
    if (storm::NumberTraits<SolutionType>::IsExact && env.modelchecker().conditional().isPrecisionSetFromDefault()) {
        STORM_LOG_INFO("Setting the conditional precision to 0 since the value type is exact and the precision was not explicitly set by the user.");
        precision = storm::utility::zero<storm::RationalNumber>();
    }

    // We might require adapting the precision of the solver to counter error propagation (e.g. when computing the normal form).
    auto normalFormConstructionEnv = env;
    auto analysisEnv = env;
    if (env.solver().isForceSoundness()) {
        // We intuitively have to divide the precision into two parts, one for computations when constructing the normal form and one for the actual analysis.
        // As the former is usually less numerically challenging, we use a factor of 1/10 for the normal form construction and 9/10 for the analysis.
        auto const normalFormPrecisionFactor = storm::utility::convertNumber<storm::RationalNumber, std::string>("1/10");
        normalFormConstructionEnv.modelchecker().conditional().setPrecision(precision * normalFormPrecisionFactor, false);
        analysisEnv.modelchecker().conditional().setPrecision(precision * (storm::utility::one<storm::RationalNumber>() - normalFormPrecisionFactor), false);
    } else {
        normalFormConstructionEnv.modelchecker().conditional().setPrecision(precision, false);
        analysisEnv.modelchecker().conditional().setPrecision(precision, false);
    }

    // We first translate the problem into a normal form.
    // @see doi.org/10.1007/978-3-642-54862-8_43
    STORM_LOG_THROW(goal.hasRelevantValues(), storm::exceptions::NotSupportedException,
                    "No initial state given. Conditional probabilities can only be computed for models with a single initial state.");
    STORM_LOG_THROW(goal.relevantValues().hasUniqueSetBit(), storm::exceptions::NotSupportedException,
                    "Only one initial state is supported for conditional probabilities");
    STORM_LOG_TRACE("Computing conditional probabilities for a model with " << transitionMatrix.getRowGroupCount() << " states and "
                                                                            << transitionMatrix.getEntryCount() << " transitions.");
    auto normalFormData = internal::obtainNormalForm(normalFormConstructionEnv, goal.direction(), produceSchedulers, transitionMatrix, backwardTransitions,
                                                     goal.relevantValues(), targetStates, conditionStates);
    // Then, we solve the induced problem using the selected algorithm
    auto const initialState = *goal.relevantValues().begin();
    ValueType initialStateValue = -storm::utility::one<ValueType>();
    std::unique_ptr<storm::storage::Scheduler<SolutionType>> scheduler = nullptr;
    if (auto trivialValue = internal::handleTrivialCases<ValueType, SolutionType>(initialState, normalFormData); trivialValue.has_value()) {
        initialStateValue = *trivialValue;
        if (initialStateValue == storm::utility::zero<ValueType>() && !normalFormData.terminalStates.get(initialState) && produceSchedulers) {
            // we need to compute a scheduler that at least reaches the condition with non-zero probability
            auto initialStateBitVector = storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false);
            initialStateBitVector.set(initialState, true);
            auto const conditionReachResult = helper::SparseMdpPrctlHelper<ValueType, ValueType>::computeUntilProbabilities(
                env, storm::solver::SolveGoal<ValueType>(storm::solver::OptimizationDirection::Maximize, initialStateBitVector), transitionMatrix,
                transitionMatrix.transpose(true), storm::storage::BitVector(conditionStates.size(), true), conditionStates, false, true);
            scheduler =
                std::unique_ptr<storm::storage::Scheduler<SolutionType>>(new storm::storage::Scheduler<SolutionType>(transitionMatrix.getRowGroupCount()));
            auto stateId = 0;
            for (uint64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
                scheduler->setChoice(conditionReachResult.scheduler->getChoice(stateId), state);
                ++stateId;
            }
        } else {
            scheduler =
                std::unique_ptr<storm::storage::Scheduler<SolutionType>>(new storm::storage::Scheduler<SolutionType>(transitionMatrix.getRowGroupCount()));
        }
        STORM_LOG_DEBUG("Initial state has trivial value " << initialStateValue);
    } else {
        STORM_LOG_ASSERT(normalFormData.maybeStates.get(initialState), "Initial state must be a maybe state if it is not a terminal state");
        auto alg = analysisEnv.modelchecker().conditional().getAlgorithm();
        if (alg == ConditionalAlgorithmSetting::Default) {
            alg = ConditionalAlgorithmSetting::BisectionPolicyTracking;
        }

        STORM_LOG_INFO("Analyzing normal form with " << normalFormData.maybeStates.getNumberOfSetBits() << " maybe states using algorithm '" << alg << ".");
        internal::ResultReturnType<SolutionType> result{storm::utility::zero<SolutionType>()};
        switch (alg) {
            case ConditionalAlgorithmSetting::Restart: {
                auto restartEnv = analysisEnv;
                restartEnv.solver().minMax().setPrecision(analysisEnv.modelchecker().conditional().getPrecision());
                restartEnv.solver().minMax().setRelativeTerminationCriterion(analysisEnv.modelchecker().conditional().isRelativePrecision());

                result =
                    internal::computeViaRestartMethod(restartEnv, initialState, goal, produceSchedulers, transitionMatrix, backwardTransitions, normalFormData);
                break;
            }
            case ConditionalAlgorithmSetting::Bisection:
            case ConditionalAlgorithmSetting::BisectionAdvanced:
            case ConditionalAlgorithmSetting::BisectionPolicyTracking:
            case ConditionalAlgorithmSetting::BisectionAdvancedPolicyTracking: {
                if (goal.isBounded()) {
                    result = internal::decideThreshold(analysisEnv, initialState, goal.direction(), goal.thresholdValue(), produceSchedulers, transitionMatrix,
                                                       backwardTransitions, normalFormData);
                } else {
                    result = internal::computeViaBisection(analysisEnv, alg, initialState, goal, produceSchedulers, transitionMatrix, backwardTransitions,
                                                           normalFormData);
                }
                break;
            }
            case ConditionalAlgorithmSetting::PolicyIteration: {
                result = internal::computeViaPolicyIteration(analysisEnv, initialState, goal.direction(), transitionMatrix, normalFormData);
                break;
            }
            default: {
                STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Unknown conditional probability algorithm: " << alg);
            }
        }
        initialStateValue = result.initialStateValue;
        scheduler = std::move(result.scheduler);
    }
    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<SolutionType>(initialState, initialStateValue));

    // if produce schedulers was set, we have to construct a scheduler with memory
    if (produceSchedulers && scheduler) {
        storm::utility::graph::computeSchedulerProb1E(normalFormData.targetStates, transitionMatrix, backwardTransitions, normalFormData.targetStates,
                                                      targetStates, *scheduler);
        storm::utility::graph::computeSchedulerProb1E(normalFormData.conditionStates, transitionMatrix, backwardTransitions, normalFormData.conditionStates,
                                                      conditionStates, *scheduler);
        // fill in the scheduler with default choices for states that are missing a choice, these states should be just the ones from which the condition is
        // unreachable this is also used to fill choices for the trivial cases
        for (uint64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
            if (!scheduler->isChoiceSelected(state)) {
                // select an arbitrary choice
                scheduler->setChoice(0, state);
            }
        }

        // create scheduler with memory structure
        storm::storage::MemoryStructure::TransitionMatrix memoryTransitions(3, std::vector<boost::optional<storm::storage::BitVector>>(3, boost::none));
        storm::models::sparse::StateLabeling memoryStateLabeling(3);
        memoryStateLabeling.addLabel("init_memory");
        memoryStateLabeling.addLabel("condition_reached");
        memoryStateLabeling.addLabel("target_reached");
        memoryStateLabeling.addLabelToState("init_memory", 0);
        memoryStateLabeling.addLabelToState("condition_reached", 1);
        memoryStateLabeling.addLabelToState("target_reached", 2);

        storm::storage::BitVector allTransitions(transitionMatrix.getEntryCount(), true);
        storm::storage::BitVector conditionExitTransitions(transitionMatrix.getEntryCount(), false);
        storm::storage::BitVector targetExitTransitions(transitionMatrix.getEntryCount(), false);

        for (auto state : conditionStates) {
            for (auto choice : transitionMatrix.getRowGroupIndices(state)) {
                for (auto entryIt = transitionMatrix.getRow(choice).begin(); entryIt < transitionMatrix.getRow(choice).end(); ++entryIt) {
                    conditionExitTransitions.set(entryIt - transitionMatrix.begin(), true);
                }
            }
        }
        for (auto state : targetStates) {
            for (auto choice : transitionMatrix.getRowGroupIndices(state)) {
                for (auto entryIt = transitionMatrix.getRow(choice).begin(); entryIt < transitionMatrix.getRow(choice).end(); ++entryIt) {
                    targetExitTransitions.set(entryIt - transitionMatrix.begin(), true);
                }
            }
        }

        memoryTransitions[0][0] =
            allTransitions & ~conditionExitTransitions & ~targetExitTransitions;  // if neither condition nor target reached, stay in init_memory
        memoryTransitions[0][1] = conditionExitTransitions;
        memoryTransitions[0][2] = targetExitTransitions & ~conditionExitTransitions;
        memoryTransitions[1][1] = allTransitions;  // once condition reached, stay in that memory state
        memoryTransitions[2][2] = allTransitions;  // once target reached, stay in that memory state

        // this assumes there is a single initial state
        auto memoryStructure = storm::storage::MemoryStructure(memoryTransitions, memoryStateLabeling, std::vector<uint64_t>(1, 0), true);

        auto finalScheduler = std::unique_ptr<storm::storage::Scheduler<SolutionType>>(
            new storm::storage::Scheduler<SolutionType>(transitionMatrix.getRowGroupCount(), std::move(memoryStructure)));

        for (uint64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
            // set choices for memory 0
            if (conditionStates.get(state)) {
                if (normalFormData.schedulerChoicesForReachingTargetStates->isChoiceSelected(state)) {
                    finalScheduler->setChoice(normalFormData.schedulerChoicesForReachingTargetStates->getChoice(state), state, 0);
                } else {
                    finalScheduler->setChoice(0, state, 0);  // arbitrary choice if no choice was recorded.
                }
            } else if (targetStates.get(state)) {
                if (normalFormData.schedulerChoicesForReachingConditionStates->isChoiceSelected(state)) {
                    finalScheduler->setChoice(normalFormData.schedulerChoicesForReachingConditionStates->getChoice(state), state, 0);
                } else {
                    finalScheduler->setChoice(0, state, 0);  // arbitrary choice if no choice was recorded.
                }
            } else {
                finalScheduler->setChoice(scheduler->getChoice(state), state, 0);
            }

            // set choices for memory 1, these are the choices after condition was reached
            if (normalFormData.schedulerChoicesForReachingTargetStates->isChoiceSelected(state)) {
                finalScheduler->setChoice(normalFormData.schedulerChoicesForReachingTargetStates->getChoice(state), state, 1);
            } else {
                finalScheduler->setChoice(0, state, 1);  // arbitrary choice if no choice was recorded.
            }
            // set choices for memory 2, these are the choices after target was reached
            if (normalFormData.schedulerChoicesForReachingConditionStates->isChoiceSelected(state)) {
                finalScheduler->setChoice(normalFormData.schedulerChoicesForReachingConditionStates->getChoice(state), state, 2);
            } else {
                finalScheduler->setChoice(0, state, 2);  // arbitrary choice if no choice was recorded.
            }
        }

        result->asExplicitQuantitativeCheckResult<SolutionType>().setScheduler(std::move(finalScheduler));
    }
    return result;
}

template std::unique_ptr<CheckResult> computeConditionalProbabilities(Environment const& env, storm::solver::SolveGoal<double>&& goal, bool produceSchedulers,
                                                                      storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                      storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                                      storm::storage::BitVector const& targetStates,
                                                                      storm::storage::BitVector const& conditionStates);

template std::unique_ptr<CheckResult> computeConditionalProbabilities(Environment const& env, storm::solver::SolveGoal<storm::RationalNumber>&& goal,
                                                                      bool produceSchedulers,
                                                                      storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                                                      storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                                                      storm::storage::BitVector const& targetStates,
                                                                      storm::storage::BitVector const& conditionStates);

}  // namespace storm::modelchecker
