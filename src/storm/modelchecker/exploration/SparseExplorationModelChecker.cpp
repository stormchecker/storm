#include "storm/modelchecker/exploration/SparseExplorationModelChecker.h"

#include <sstream>

#include "storm/modelchecker/exploration/Bounds.h"
#include "storm/modelchecker/exploration/ExplorationInformation.h"
#include "storm/modelchecker/exploration/StateGeneration.h"
#include "storm/modelchecker/exploration/Statistics.h"

#include "storm/storage/expressions/ExpressionEvaluator.h"

#include "storm/generator/CompressedState.h"

#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/SparseMatrix.h"

#include "storm/storage/prism/Program.h"

#include "storm/logic/FragmentSpecification.h"

#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"

#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/ExplorationSettings.h"

#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotSupportedException.h"

namespace storm {
namespace modelchecker {

template<typename ModelType, typename StateType>
SparseExplorationModelChecker<ModelType, StateType>::SparseExplorationModelChecker(storm::prism::Program const& program)
    : program(program.substituteConstantsFormulas()),
      randomGenerator(std::chrono::system_clock::now().time_since_epoch().count()),
      comparator(storm::settings::getModule<storm::settings::modules::ExplorationSettings>().getPrecision()) {
    // Intentionally left empty.
}

template<typename ModelType, typename StateType>
bool SparseExplorationModelChecker<ModelType, StateType>::canHandleStatic(CheckTask<storm::logic::Formula, ValueType> const& checkTask) {
    storm::logic::Formula const& formula = checkTask.getFormula();
    storm::logic::FragmentSpecification fragment = storm::logic::reachability();
    return formula.isInFragment(fragment) && checkTask.isOnlyInitialStatesRelevantSet();
}

template<typename ModelType, typename StateType>
bool SparseExplorationModelChecker<ModelType, StateType>::canHandle(CheckTask<storm::logic::Formula, ValueType> const& checkTask) const {
    return canHandleStatic(checkTask);
}

template<typename ModelType, typename StateType>
std::unique_ptr<CheckResult> SparseExplorationModelChecker<ModelType, StateType>::computeUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) {
    storm::logic::UntilFormula const& untilFormula = checkTask.getFormula();
    storm::logic::Formula const& conditionFormula = untilFormula.getLeftSubformula();
    storm::logic::Formula const& targetFormula = untilFormula.getRightSubformula();
    STORM_LOG_THROW(program.isDeterministicModel() || checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "For nondeterministic systems, an optimization direction (min/max) must be given in the property.");

    ExplorationInformation<StateType, ValueType> explorationInformation(checkTask.isOptimizationDirectionSet() ? checkTask.getOptimizationDirection()
                                                                                                               : storm::OptimizationDirection::Maximize);

    // The first row group starts at action 0.
    explorationInformation.newRowGroup(0);

    std::map<std::string, storm::expressions::Expression> labelToExpressionMapping = program.getLabelToExpressionMapping();
    StateGeneration<StateType, ValueType> stateGeneration(program, explorationInformation,
                                                          conditionFormula.toExpression(program.getManager(), labelToExpressionMapping),
                                                          targetFormula.toExpression(program.getManager(), labelToExpressionMapping));

    // Compute and return result.
    std::tuple<StateType, ValueType, ValueType> boundsForInitialState = performExploration(stateGeneration, explorationInformation);
    return std::make_unique<ExplicitQuantitativeCheckResult<ValueType>>(std::get<0>(boundsForInitialState), std::get<1>(boundsForInitialState));
}

template<typename ModelType, typename StateType>
std::tuple<StateType, typename ModelType::ValueType, typename ModelType::ValueType> SparseExplorationModelChecker<ModelType, StateType>::performExploration(
    StateGeneration<StateType, ValueType>& stateGeneration, ExplorationInformation<StateType, typename ModelType::ValueType>& explorationInformation) const {
    // Generate the initial state so we know where to start the simulation.
    stateGeneration.computeInitialStates();
    STORM_LOG_THROW(stateGeneration.getNumberOfInitialStates() == 1, storm::exceptions::NotSupportedException,
                    "Currently only models with one initial state are supported by the exploration engine.");
    StateType initialStateIndex = stateGeneration.getFirstInitialState();

    // Create a structure that holds the bounds for the states and actions.
    Bounds<StateType, ValueType> bounds;

    // Create a stack that is used to track the path we sampled.
    StateActionStack stack;

    // Now perform the actual sampling.
    Statistics<StateType, ValueType> stats;
    bool convergenceCriterionMet = false;
    while (!convergenceCriterionMet) {
        bool result = samplePathFromInitialState(stateGeneration, explorationInformation, stack, bounds, stats);

        stats.sampledPath();
        stats.updateMaxPathLength(stack.size());

        // If a terminal state was found, we update the probabilities along the path contained in the stack.
        if (result) {
            // Update the bounds along the path to the terminal state.
            STORM_LOG_TRACE("Found terminal state, updating probabilities along path.");
            updateProbabilityBoundsAlongSampledPath(stack, explorationInformation, bounds);
        } else {
            // If not terminal state was found, the search aborted, possibly because of an EC-detection. In this
            // case, we cannot update the probabilities.
            STORM_LOG_TRACE("Did not find terminal state.");
        }

        STORM_LOG_DEBUG("Discovered states: " << explorationInformation.getNumberOfDiscoveredStates() << " (" << stats.numberOfExploredStates << " explored, "
                                              << explorationInformation.getNumberOfUnexploredStates() << " unexplored).");
        STORM_LOG_DEBUG("Value of initial state is in [" << bounds.getLowerBoundForState(initialStateIndex, explorationInformation) << ", "
                                                         << bounds.getUpperBoundForState(initialStateIndex, explorationInformation) << "].");
        ValueType difference = bounds.getDifferenceOfStateBounds(initialStateIndex, explorationInformation);
        STORM_LOG_DEBUG("Difference after iteration " << stats.pathsSampled << " is " << difference << ".");
        convergenceCriterionMet = comparator.isZero(difference);

        // If the number of sampled paths exceeds a certain threshold, do a precomputation.
        if (!convergenceCriterionMet && explorationInformation.performPrecomputationExcessiveSampledPaths(stats.pathsSampledSinceLastPrecomputation)) {
            performPrecomputation(stack, explorationInformation, bounds, stats);
        }
    }

    std::stringstream statsStream;
    stats.printToStream(statsStream, explorationInformation);
    STORM_LOG_STATISTICS(statsStream.str());

    return std::make_tuple(initialStateIndex, bounds.getLowerBoundForState(initialStateIndex, explorationInformation),
                           bounds.getUpperBoundForState(initialStateIndex, explorationInformation));
}

template<typename ModelType, typename StateType>
bool SparseExplorationModelChecker<ModelType, StateType>::samplePathFromInitialState(StateGeneration<StateType, ValueType>& stateGeneration,
                                                                                     ExplorationInformation<StateType, ValueType>& explorationInformation,
                                                                                     StateActionStack& stack, Bounds<StateType, ValueType>& bounds,
                                                                                     Statistics<StateType, ValueType>& stats) const {
    // Start the search from the initial state.
    stack.push_back(std::make_pair(stateGeneration.getFirstInitialState(), 0));

    // As long as we didn't find a terminal (accepting or rejecting) state in the search, sample a new successor.
    bool foundTerminalState = false;
    while (!foundTerminalState) {
        StateType const& currentStateId = stack.back().first;
        STORM_LOG_TRACE("State on top of stack is: " << currentStateId << ".");

        // If the state is not yet explored, we need to retrieve its behaviors.
        auto unexploredIt = explorationInformation.findUnexploredState(currentStateId);
        if (unexploredIt != explorationInformation.unexploredStatesEnd()) {
            STORM_LOG_TRACE("State was not yet explored.");

            // Explore the previously unexplored state.
            storm::generator::CompressedState const& compressedState = unexploredIt->second;
            foundTerminalState = exploreState(stateGeneration, currentStateId, compressedState, explorationInformation, bounds, stats);
            if (foundTerminalState) {
                STORM_LOG_TRACE("Aborting sampling of path, because a terminal state was reached.");
            }
            explorationInformation.removeUnexploredState(unexploredIt);
        } else {
            // If the state was already explored, we check whether it is a terminal state or not.
            if (explorationInformation.isTerminal(currentStateId)) {
                STORM_LOG_TRACE("Found already explored terminal state: " << currentStateId << ".");
                foundTerminalState = true;
            }
        }

        // Notify the stats about the performed exploration step.
        stats.explorationStep();

        // If the state was not a terminal state, we continue the path search and sample the next state.
        if (!foundTerminalState) {
            // At this point, we can be sure that the state was expanded and that we can sample according to the
            // probabilities in the matrix.
            uint32_t chosenAction = sampleActionOfState(currentStateId, explorationInformation, bounds);
            stack.back().second = chosenAction;
            STORM_LOG_TRACE("Sampled action " << chosenAction << " in state " << currentStateId << ".");

            StateType successor = sampleSuccessorFromAction(chosenAction, explorationInformation, bounds);
            STORM_LOG_TRACE("Sampled successor " << successor << " according to action " << chosenAction << " of state " << currentStateId << ".");

            // Put the successor state and a dummy action on top of the stack.
            stack.emplace_back(successor, 0);

            // If the number of exploration steps exceeds a certain threshold, do a precomputation.
            if (explorationInformation.performPrecomputationExcessiveExplorationSteps(stats.explorationStepsSinceLastPrecomputation)) {
                performPrecomputation(stack, explorationInformation, bounds, stats);

                STORM_LOG_TRACE("Aborting the search after precomputation.");
                stack.clear();
                break;
            }
        }
    }

    return foundTerminalState;
}

template<typename ModelType, typename StateType>
bool SparseExplorationModelChecker<ModelType, StateType>::exploreState(StateGeneration<StateType, ValueType>& stateGeneration, StateType const& currentStateId,
                                                                       storm::generator::CompressedState const& currentState,
                                                                       ExplorationInformation<StateType, ValueType>& explorationInformation,
                                                                       Bounds<StateType, ValueType>& bounds, Statistics<StateType, ValueType>& stats) const {
    bool isTerminalState = false;
    bool isTargetState = false;

    ++stats.numberOfExploredStates;

    // Finally, map the unexplored state to the row group.
    explorationInformation.assignStateToNextRowGroup(currentStateId);
    STORM_LOG_TRACE("Assigning row group " << explorationInformation.getRowGroup(currentStateId) << " to state " << currentStateId << ".");

    // Initialize the bounds, because some of the following computations depend on the values to be available for
    // all states that have been assigned to a row-group.
    bounds.initializeBoundsForNextState();

    // Before generating the behavior of the state, we need to determine whether it's a target state that
    // does not need to be expanded.
    stateGeneration.load(currentState);
    if (stateGeneration.isTargetState()) {
        ++stats.numberOfTargetStates;
        isTargetState = true;
        isTerminalState = true;
    } else if (stateGeneration.isConditionState()) {
        STORM_LOG_TRACE("Exploring state.");

        // If it needs to be expanded, we use the generator to retrieve the behavior of the new state.
        storm::generator::StateBehavior<ValueType, StateType> behavior = stateGeneration.expand();
        STORM_LOG_TRACE("State has " << behavior.getNumberOfChoices() << " choices.");

        // Clumsily check whether we have found a state that forms a trivial BMEC.
        bool otherSuccessor = false;
        for (auto const& choice : behavior) {
            for (auto const& entry : choice) {
                if (entry.first != currentStateId) {
                    otherSuccessor = true;
                    break;
                }
            }
        }
        isTerminalState = !otherSuccessor;

        // If the state was neither a trivial (non-accepting) terminal state nor a target state, we
        // need to store its behavior.
        if (!isTerminalState) {
            // Next, we insert the behavior into our matrix structure.
            StateType startAction = explorationInformation.getActionCount();
            explorationInformation.addActionsToMatrix(behavior.getNumberOfChoices());

            ActionType localAction = 0;

            // Retrieve the lowest state bounds (wrt. to the current optimization direction).
            std::pair<ValueType, ValueType> stateBounds = getLowestBounds(explorationInformation.getOptimizationDirection());

            for (auto const& choice : behavior) {
                for (auto const& entry : choice) {
                    explorationInformation.getRowOfMatrix(startAction + localAction).emplace_back(entry.first, entry.second);
                    STORM_LOG_TRACE("Found transition " << currentStateId << "-[" << (startAction + localAction) << ", " << entry.second << "]-> "
                                                        << entry.first << ".");
                }

                std::pair<ValueType, ValueType> actionBounds = computeBoundsOfAction(startAction + localAction, explorationInformation, bounds);
                bounds.initializeBoundsForNextAction(actionBounds);
                stateBounds = combineBounds(explorationInformation.getOptimizationDirection(), stateBounds, actionBounds);

                STORM_LOG_TRACE("Initializing bounds of action " << (startAction + localAction) << " to "
                                                                 << bounds.getLowerBoundForAction(startAction + localAction) << " and "
                                                                 << bounds.getUpperBoundForAction(startAction + localAction) << ".");

                ++localAction;
            }

            // Terminate the row group.
            explorationInformation.terminateCurrentRowGroup();

            bounds.setBoundsForState(currentStateId, explorationInformation, stateBounds);
            STORM_LOG_TRACE("Initializing bounds of state " << currentStateId << " to " << bounds.getLowerBoundForState(currentStateId, explorationInformation)
                                                            << " and " << bounds.getUpperBoundForState(currentStateId, explorationInformation) << ".");
        }
    } else {
        // In this case, the state is neither a target state nor a condition state and therefore a rejecting
        // terminal state.
        isTerminalState = true;
    }

    if (isTerminalState) {
        STORM_LOG_TRACE("State does not need to be explored, because it is " << (isTargetState ? "a target state" : "a rejecting terminal state") << ".");
        explorationInformation.addTerminalState(currentStateId);

        if (isTargetState) {
            bounds.setBoundsForState(currentStateId, explorationInformation,
                                     std::make_pair(storm::utility::one<ValueType>(), storm::utility::one<ValueType>()));
            bounds.initializeBoundsForNextAction(std::make_pair(storm::utility::one<ValueType>(), storm::utility::one<ValueType>()));
        } else {
            bounds.setBoundsForState(currentStateId, explorationInformation,
                                     std::make_pair(storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>()));
            bounds.initializeBoundsForNextAction(std::make_pair(storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>()));
        }

        // Increase the size of the matrix, but leave the row empty.
        explorationInformation.addActionsToMatrix(1);

        // Terminate the row group.
        explorationInformation.newRowGroup();
    }

    return isTerminalState;
}

template<typename ModelType, typename StateType>
typename SparseExplorationModelChecker<ModelType, StateType>::ActionType SparseExplorationModelChecker<ModelType, StateType>::sampleActionOfState(
    StateType const& currentStateId, ExplorationInformation<StateType, ValueType> const& explorationInformation, Bounds<StateType, ValueType>& bounds) const {
    // Determine the values of all available actions.
    std::vector<std::pair<ActionType, ValueType>> actionValues;
    StateType rowGroup = explorationInformation.getRowGroup(currentStateId);

    // Check for cases in which we do not need to perform more work.
    if (explorationInformation.onlyOneActionAvailable(rowGroup)) {
        return explorationInformation.getStartRowOfGroup(rowGroup);
    }

    // If there are more choices to consider, start by gathering the values of relevant actions.
    STORM_LOG_TRACE("Sampling from actions leaving the state.");

    for (uint32_t row = explorationInformation.getStartRowOfGroup(rowGroup); row < explorationInformation.getStartRowOfGroup(rowGroup + 1); ++row) {
        actionValues.push_back(std::make_pair(row, bounds.getBoundForAction(explorationInformation.getOptimizationDirection(), row)));
    }

    STORM_LOG_ASSERT(!actionValues.empty(), "Values for actions must not be empty.");

    // Sort the actions wrt. to the optimization direction.
    if (explorationInformation.maximize()) {
        std::sort(actionValues.begin(), actionValues.end(),
                  [](std::pair<ActionType, ValueType> const& a, std::pair<ActionType, ValueType> const& b) { return a.second > b.second; });
    } else {
        std::sort(actionValues.begin(), actionValues.end(),
                  [](std::pair<ActionType, ValueType> const& a, std::pair<ActionType, ValueType> const& b) { return a.second < b.second; });
    }

    // Determine the first elements of the sorted range that agree on their value.
    auto end = ++actionValues.begin();
    while (end != actionValues.end() && comparator.isEqual(actionValues.begin()->second, end->second)) {
        ++end;
    }

    // Now sample from all maximizing actions.
    std::uniform_int_distribution<ActionType> distribution(0, std::distance(actionValues.begin(), end) - 1);
    return actionValues[distribution(randomGenerator)].first;
}

template<typename ModelType, typename StateType>
StateType SparseExplorationModelChecker<ModelType, StateType>::sampleSuccessorFromAction(
    ActionType const& chosenAction, ExplorationInformation<StateType, ValueType> const& explorationInformation,
    Bounds<StateType, ValueType> const& bounds) const {
    std::vector<storm::storage::MatrixEntry<StateType, ValueType>> const& row = explorationInformation.getRowOfMatrix(chosenAction);
    if (row.size() == 1) {
        return row.front().getColumn();
    }

    // Depending on the selected next-state heuristic, we give the states other likelihoods of getting chosen.
    if (explorationInformation.useDifferenceProbabilitySumHeuristic() || explorationInformation.useProbabilityHeuristic()) {
        std::vector<ValueType> probabilities(row.size());
        if (explorationInformation.useDifferenceProbabilitySumHeuristic()) {
            std::transform(row.begin(), row.end(), probabilities.begin(),
                           [&bounds, &explorationInformation](storm::storage::MatrixEntry<StateType, ValueType> const& entry) {
                               return entry.getValue() + bounds.getDifferenceOfStateBounds(entry.getColumn(), explorationInformation);
                           });
        } else if (explorationInformation.useProbabilityHeuristic()) {
            std::transform(row.begin(), row.end(), probabilities.begin(),
                           [](storm::storage::MatrixEntry<StateType, ValueType> const& entry) { return entry.getValue(); });
        }

        // Now sample according to the probabilities.
        std::discrete_distribution<StateType> distribution(probabilities.begin(), probabilities.end());
        return row[distribution(randomGenerator)].getColumn();
    } else {
        STORM_LOG_ASSERT(explorationInformation.useUniformHeuristic(), "Illegal next-state heuristic.");
        std::uniform_int_distribution<ActionType> distribution(0, row.size() - 1);
        return row[distribution(randomGenerator)].getColumn();
    }
}

template<typename ModelType, typename StateType>
bool SparseExplorationModelChecker<ModelType, StateType>::performPrecomputation(StateActionStack const& stack,
                                                                                ExplorationInformation<StateType, ValueType>& explorationInformation,
                                                                                Bounds<StateType, ValueType>& bounds,
                                                                                Statistics<StateType, ValueType>& stats) const {
    ++stats.numberOfPrecomputations;

    // Outline:
    // 1. construct a sparse transition matrix of the relevant part of the state space.
    // 2. use this matrix to compute states with probability 0/1 and an MEC decomposition (in the max case).
    // 3. use MEC decomposition to collapse MECs.
    STORM_LOG_TRACE("Starting " << (explorationInformation.useLocalPrecomputation() ? "local" : "global") << " precomputation.");

    // Construct the matrix that represents the fragment of the system contained in the currently sampled path.
    storm::storage::SparseMatrixBuilder<ValueType> builder(0, 0, 0, false, true, 0);

    // Determine the set of states that was expanded.
    std::vector<StateType> relevantStates;
    if (explorationInformation.useLocalPrecomputation()) {
        for (auto const& stateActionPair : stack) {
            if (explorationInformation.maximize() || !storm::utility::isOne(bounds.getLowerBoundForState(stateActionPair.first, explorationInformation))) {
                relevantStates.push_back(stateActionPair.first);
            }
        }
        std::sort(relevantStates.begin(), relevantStates.end());
        auto newEnd = std::unique(relevantStates.begin(), relevantStates.end());
        relevantStates.resize(std::distance(relevantStates.begin(), newEnd));
    } else {
        for (StateType state = 0; state < explorationInformation.getNumberOfDiscoveredStates(); ++state) {
            // Add the state to the relevant states if they are not unexplored.
            if (!explorationInformation.isUnexplored(state)) {
                relevantStates.push_back(state);
            }
        }
    }
    StateType sink = relevantStates.size();

    // Create a mapping for faster look-up during the translation of flexible matrix to the real sparse matrix.
    // While doing so, record all target states.
    std::unordered_map<StateType, StateType> relevantStateToNewRowGroupMapping;
    storm::storage::BitVector targetStates(sink + 1);
    for (StateType index = 0; index < relevantStates.size(); ++index) {
        relevantStateToNewRowGroupMapping.emplace(relevantStates[index], index);
        if (storm::utility::isOne(bounds.getLowerBoundForState(relevantStates[index], explorationInformation))) {
            targetStates.set(index);
        }
    }

    // Do the actual translation.
    StateType currentRow = 0;
    for (auto const& state : relevantStates) {
        builder.newRowGroup(currentRow);
        StateType rowGroup = explorationInformation.getRowGroup(state);
        for (auto row = explorationInformation.getStartRowOfGroup(rowGroup); row < explorationInformation.getStartRowOfGroup(rowGroup + 1); ++row) {
            ValueType unexpandedProbability = storm::utility::zero<ValueType>();
            for (auto const& entry : explorationInformation.getRowOfMatrix(row)) {
                auto it = relevantStateToNewRowGroupMapping.find(entry.getColumn());
                if (it != relevantStateToNewRowGroupMapping.end()) {
                    // If the entry is a relevant state, we copy it over (and compensate for the offset change).
                    builder.addNextValue(currentRow, it->second, entry.getValue());
                } else {
                    // If the entry is an unexpanded state, we gather the probability to later redirect it to an unexpanded sink.
                    unexpandedProbability += entry.getValue();
                }
            }
            if (unexpandedProbability != storm::utility::zero<ValueType>()) {
                builder.addNextValue(currentRow, sink, unexpandedProbability);
            }
            ++currentRow;
        }
    }
    // Then, make the unexpanded state absorbing.
    builder.newRowGroup(currentRow);
    builder.addNextValue(currentRow, sink, storm::utility::one<ValueType>());
    storm::storage::SparseMatrix<ValueType> relevantStatesMatrix = builder.build();
    storm::storage::SparseMatrix<ValueType> transposedMatrix = relevantStatesMatrix.transpose(true);
    STORM_LOG_TRACE("Successfully built matrix for precomputation.");

    storm::storage::BitVector allStates(sink + 1, true);
    storm::storage::BitVector statesWithProbability0;
    storm::storage::BitVector statesWithProbability1;
    if (explorationInformation.maximize()) {
        // If we are computing maximal probabilities, we first perform a detection of states that have
        // probability 01 and then additionally perform an MEC decomposition. The reason for this somewhat
        // duplicate work is the following. Optimally, we would only do the MEC decomposition, because we need
        // it anyway. However, when only detecting (accepting) MECs, we do not infer which of the other states
        // (not contained in MECs) also have probability 0/1.
        targetStates.set(sink, true);
        statesWithProbability0 = storm::utility::graph::performProb0A(transposedMatrix, allStates, targetStates);
        targetStates.set(sink, false);
        statesWithProbability1 =
            storm::utility::graph::performProb1E(relevantStatesMatrix, relevantStatesMatrix.getRowGroupIndices(), transposedMatrix, allStates, targetStates);

        storm::storage::MaximalEndComponentDecomposition<ValueType> mecDecomposition(relevantStatesMatrix, relevantStatesMatrix.transpose(true));
        ++stats.ecDetections;
        STORM_LOG_TRACE("Successfully computed MEC decomposition. Found " << (mecDecomposition.size() > 1 ? (mecDecomposition.size() - 1) : 0) << " MEC(s).");

        // If the decomposition contains only the MEC consisting of the sink state, we count it as 'failed'.
        STORM_LOG_ASSERT(mecDecomposition.size() > 0, "Expected at least one MEC (the trivial sink MEC).");
        if (mecDecomposition.size() == 1) {
            ++stats.failedEcDetections;
        } else {
            stats.totalNumberOfEcDetected += mecDecomposition.size() - 1;

            // 3. Analyze the MEC decomposition.
            for (auto const& mec : mecDecomposition) {
                // Ignore the (expected) MEC of the sink state.
                if (mec.containsState(sink)) {
                    continue;
                }

                collapseMec(mec, relevantStates, relevantStatesMatrix, explorationInformation, bounds);
            }
        }
    } else {
        // If we are computing minimal probabilities, we do not need to perform an EC-detection. We rather
        // compute all states (of the considered fragment) that have probability 0/1. For states with
        // probability 0, we have to mark the sink as being a target. For states with probability 1, however,
        // we must treat the sink as being rejecting.
        targetStates.set(sink, true);
        statesWithProbability0 =
            storm::utility::graph::performProb0E(relevantStatesMatrix, relevantStatesMatrix.getRowGroupIndices(), transposedMatrix, allStates, targetStates);
        targetStates.set(sink, false);
        statesWithProbability1 =
            storm::utility::graph::performProb1A(relevantStatesMatrix, relevantStatesMatrix.getRowGroupIndices(), transposedMatrix, allStates, targetStates);
    }

    // Set the bounds of the identified states.
    STORM_LOG_ASSERT((statesWithProbability0 & statesWithProbability1).empty(), "States with probability 0 and 1 overlap.");
    for (uint64_t state : statesWithProbability0) {
        // Skip the sink state as it is not contained in the original system.
        if (state == sink) {
            continue;
        }

        StateType originalState = relevantStates[state];
        bounds.setUpperBoundForState(originalState, explorationInformation, storm::utility::zero<ValueType>());
        explorationInformation.addTerminalState(originalState);
    }
    for (uint64_t state : statesWithProbability1) {
        // Skip the sink state as it is not contained in the original system.
        if (state == sink) {
            continue;
        }

        StateType originalState = relevantStates[state];
        bounds.setLowerBoundForState(originalState, explorationInformation, storm::utility::one<ValueType>());
        explorationInformation.addTerminalState(originalState);
    }
    return true;
}

template<typename ModelType, typename StateType>
void SparseExplorationModelChecker<ModelType, StateType>::collapseMec(storm::storage::MaximalEndComponent const& mec,
                                                                      std::vector<StateType> const& relevantStates,
                                                                      storm::storage::SparseMatrix<ValueType> const& relevantStatesMatrix,
                                                                      ExplorationInformation<StateType, ValueType>& explorationInformation,
                                                                      Bounds<StateType, ValueType>& bounds) const {
    bool containsTargetState = false;

    // Now we record all actions leaving the EC.
    std::vector<ActionType> leavingActions;
    for (auto const& stateAndChoices : mec) {
        // Compute the state of the original model that corresponds to the current state.
        StateType originalState = relevantStates[stateAndChoices.first];
        StateType originalRowGroup = explorationInformation.getRowGroup(originalState);

        // Check whether a target state is contained in the MEC.
        if (!containsTargetState && storm::utility::isOne(bounds.getLowerBoundForRowGroup(originalRowGroup))) {
            containsTargetState = true;
        }

        // For each state, compute the actions that leave the MEC.
        auto includedChoicesIt = stateAndChoices.second.begin();
        auto includedChoicesIte = stateAndChoices.second.end();
        for (auto action = explorationInformation.getStartRowOfGroup(originalRowGroup);
             action < explorationInformation.getStartRowOfGroup(originalRowGroup + 1); ++action) {
            if (includedChoicesIt != includedChoicesIte) {
                STORM_LOG_TRACE("Next (local) choice contained in MEC is "
                                << (*includedChoicesIt - relevantStatesMatrix.getRowGroupIndices()[stateAndChoices.first]));
                STORM_LOG_TRACE("Current (local) choice iterated is " << (action - explorationInformation.getStartRowOfGroup(originalRowGroup)));
                if (action - explorationInformation.getStartRowOfGroup(originalRowGroup) !=
                    *includedChoicesIt - relevantStatesMatrix.getRowGroupIndices()[stateAndChoices.first]) {
                    STORM_LOG_TRACE("Choice leaves the EC.");
                    leavingActions.push_back(action);
                } else {
                    STORM_LOG_TRACE("Choice stays in the EC.");
                    ++includedChoicesIt;
                }
            } else {
                STORM_LOG_TRACE("Choice leaves the EC, because there is no more choice staying in the EC.");
                leavingActions.push_back(action);
            }
        }
    }

    // Now, we need to collapse the EC only if it does not contain a target state and the leaving actions are
    // non-empty, because only then have the states a (potentially) non-zero, non-one probability.
    if (!containsTargetState && !leavingActions.empty()) {
        // In this case, no target state is contained in the MEC, but there are actions leaving the MEC. To
        // prevent the simulation getting stuck in this MEC again, we replace all states in the MEC by a new
        // state whose outgoing actions are the ones leaving the MEC. We do this, by assigning all states in the
        // MEC to a new row group, which will then hold all the outgoing choices.

        // Remap all contained states to the new row group.
        StateType nextRowGroup = explorationInformation.getNextRowGroup();
        for (auto const& stateAndChoices : mec) {
            StateType originalState = relevantStates[stateAndChoices.first];
            explorationInformation.assignStateToRowGroup(originalState, nextRowGroup);
        }

        bounds.initializeBoundsForNextState();

        // Add to the new row group all leaving actions of contained states and set the appropriate bounds for
        // the actions and the new state.
        std::pair<ValueType, ValueType> stateBounds = getLowestBounds(explorationInformation.getOptimizationDirection());
        for (auto const& action : leavingActions) {
            explorationInformation.moveActionToBackOfMatrix(action);
            std::pair<ValueType, ValueType> actionBounds = bounds.getBoundsForAction(action);
            bounds.initializeBoundsForNextAction(actionBounds);
            stateBounds = combineBounds(explorationInformation.getOptimizationDirection(), stateBounds, actionBounds);
        }
        bounds.setBoundsForRowGroup(nextRowGroup, stateBounds);

        // Terminate the row group of the newly introduced state.
        explorationInformation.terminateCurrentRowGroup();
    }
}

template<typename ModelType, typename StateType>
typename ModelType::ValueType SparseExplorationModelChecker<ModelType, StateType>::computeLowerBoundOfAction(
    ActionType const& action, ExplorationInformation<StateType, ValueType> const& explorationInformation, Bounds<StateType, ValueType> const& bounds) const {
    ValueType result = storm::utility::zero<ValueType>();
    for (auto const& element : explorationInformation.getRowOfMatrix(action)) {
        result += element.getValue() * bounds.getLowerBoundForState(element.getColumn(), explorationInformation);
    }
    return result;
}

template<typename ModelType, typename StateType>
typename ModelType::ValueType SparseExplorationModelChecker<ModelType, StateType>::computeUpperBoundOfAction(
    ActionType const& action, ExplorationInformation<StateType, ValueType> const& explorationInformation, Bounds<StateType, ValueType> const& bounds) const {
    ValueType result = storm::utility::zero<ValueType>();
    for (auto const& element : explorationInformation.getRowOfMatrix(action)) {
        result += element.getValue() * bounds.getUpperBoundForState(element.getColumn(), explorationInformation);
    }
    return result;
}

template<typename ModelType, typename StateType>
std::pair<typename ModelType::ValueType, typename ModelType::ValueType> SparseExplorationModelChecker<ModelType, StateType>::computeBoundsOfAction(
    ActionType const& action, ExplorationInformation<StateType, ValueType> const& explorationInformation, Bounds<StateType, ValueType> const& bounds) const {
    // TODO: take into account self-loops?
    std::pair<ValueType, ValueType> result = std::make_pair(storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>());
    for (auto const& element : explorationInformation.getRowOfMatrix(action)) {
        result.first += element.getValue() * bounds.getLowerBoundForState(element.getColumn(), explorationInformation);
        result.second += element.getValue() * bounds.getUpperBoundForState(element.getColumn(), explorationInformation);
    }
    return result;
}

template<typename ModelType, typename StateType>
std::pair<typename ModelType::ValueType, typename ModelType::ValueType> SparseExplorationModelChecker<ModelType, StateType>::computeBoundsOfState(
    StateType const& currentStateId, ExplorationInformation<StateType, ValueType> const& explorationInformation,
    Bounds<StateType, ValueType> const& bounds) const {
    StateType group = explorationInformation.getRowGroup(currentStateId);
    std::pair<ValueType, ValueType> result = getLowestBounds(explorationInformation.getOptimizationDirection());
    for (ActionType action = explorationInformation.getStartRowOfGroup(group); action < explorationInformation.getStartRowOfGroup(group + 1); ++action) {
        std::pair<ValueType, ValueType> actionValues = computeBoundsOfAction(action, explorationInformation, bounds);
        result = combineBounds(explorationInformation.getOptimizationDirection(), result, actionValues);
    }
    return result;
}

template<typename ModelType, typename StateType>
void SparseExplorationModelChecker<ModelType, StateType>::updateProbabilityBoundsAlongSampledPath(
    StateActionStack& stack, ExplorationInformation<StateType, ValueType> const& explorationInformation, Bounds<StateType, ValueType>& bounds) const {
    stack.pop_back();
    while (!stack.empty()) {
        updateProbabilityOfAction(stack.back().first, stack.back().second, explorationInformation, bounds);
        stack.pop_back();
    }
}

template<typename ModelType, typename StateType>
void SparseExplorationModelChecker<ModelType, StateType>::updateProbabilityOfAction(StateType const& state, ActionType const& action,
                                                                                    ExplorationInformation<StateType, ValueType> const& explorationInformation,
                                                                                    Bounds<StateType, ValueType>& bounds) const {
    // Compute the new lower/upper values of the action.
    std::pair<ValueType, ValueType> newBoundsForAction = computeBoundsOfAction(action, explorationInformation, bounds);

    // And set them as the current value.
    bounds.setBoundsForAction(action, newBoundsForAction);

    // Check if we need to update the values for the states.
    if (explorationInformation.maximize()) {
        bounds.setLowerBoundOfStateIfGreaterThanOld(state, explorationInformation, newBoundsForAction.first);

        StateType rowGroup = explorationInformation.getRowGroup(state);
        if (newBoundsForAction.second < bounds.getUpperBoundForRowGroup(rowGroup)) {
            if (explorationInformation.getRowGroupSize(rowGroup) > 1) {
                newBoundsForAction.second = std::max(newBoundsForAction.second, computeBoundOverAllOtherActions(storm::OptimizationDirection::Maximize, state,
                                                                                                                action, explorationInformation, bounds));
            }

            bounds.setUpperBoundForRowGroup(rowGroup, newBoundsForAction.second);
        }
    } else {
        bounds.setUpperBoundOfStateIfLessThanOld(state, explorationInformation, newBoundsForAction.second);

        StateType rowGroup = explorationInformation.getRowGroup(state);
        if (bounds.getLowerBoundForRowGroup(rowGroup) < newBoundsForAction.first) {
            if (explorationInformation.getRowGroupSize(rowGroup) > 1) {
                ValueType min = computeBoundOverAllOtherActions(storm::OptimizationDirection::Minimize, state, action, explorationInformation, bounds);
                newBoundsForAction.first = std::min(newBoundsForAction.first, min);
            }

            bounds.setLowerBoundForRowGroup(rowGroup, newBoundsForAction.first);
        }
    }
}

template<typename ModelType, typename StateType>
typename ModelType::ValueType SparseExplorationModelChecker<ModelType, StateType>::computeBoundOverAllOtherActions(
    storm::OptimizationDirection const& direction, StateType const& state, ActionType const& action,
    ExplorationInformation<StateType, ValueType> const& explorationInformation, Bounds<StateType, ValueType> const& bounds) const {
    ValueType bound = getLowestBound(explorationInformation.getOptimizationDirection());

    ActionType group = explorationInformation.getRowGroup(state);
    for (auto currentAction = explorationInformation.getStartRowOfGroup(group); currentAction < explorationInformation.getStartRowOfGroup(group + 1);
         ++currentAction) {
        if (currentAction == action) {
            continue;
        }

        if (direction == storm::OptimizationDirection::Maximize) {
            bound = std::max(bound, computeUpperBoundOfAction(currentAction, explorationInformation, bounds));
        } else {
            bound = std::min(bound, computeLowerBoundOfAction(currentAction, explorationInformation, bounds));
        }
    }
    return bound;
}

template<typename ModelType, typename StateType>
std::pair<typename ModelType::ValueType, typename ModelType::ValueType> SparseExplorationModelChecker<ModelType, StateType>::getLowestBounds(
    storm::OptimizationDirection const& direction) const {
    ValueType val = getLowestBound(direction);
    return std::make_pair(val, val);
}

template<typename ModelType, typename StateType>
typename ModelType::ValueType SparseExplorationModelChecker<ModelType, StateType>::getLowestBound(storm::OptimizationDirection const& direction) const {
    if (direction == storm::OptimizationDirection::Maximize) {
        return storm::utility::zero<ValueType>();
    } else {
        return storm::utility::one<ValueType>();
    }
}

template<typename ModelType, typename StateType>
std::pair<typename ModelType::ValueType, typename ModelType::ValueType> SparseExplorationModelChecker<ModelType, StateType>::combineBounds(
    storm::OptimizationDirection const& direction, std::pair<ValueType, ValueType> const& bounds1, std::pair<ValueType, ValueType> const& bounds2) const {
    if (direction == storm::OptimizationDirection::Maximize) {
        return std::make_pair(std::max(bounds1.first, bounds2.first), std::max(bounds1.second, bounds2.second));
    } else {
        return std::make_pair(std::min(bounds1.first, bounds2.first), std::min(bounds1.second, bounds2.second));
    }
}

template class SparseExplorationModelChecker<storm::models::sparse::Dtmc<double>, uint32_t>;
template class SparseExplorationModelChecker<storm::models::sparse::Mdp<double>, uint32_t>;
}  // namespace modelchecker
}  // namespace storm
