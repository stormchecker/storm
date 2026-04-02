#include "storm/utility/graph.h"

#include <algorithm>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/models/sparse/DeterministicModel.h"
#include "storm/models/sparse/NondeterministicModel.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/symbolic/DeterministicModel.h"
#include "storm/models/symbolic/NondeterministicModel.h"
#include "storm/models/symbolic/StochasticTwoPlayerGame.h"
#include "storm/storage/ExplicitGameStrategyPair.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/sparse/StateType.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace utility {
namespace graph {

template<typename T>
storm::storage::BitVector getReachableOneStep(storm::storage::SparseMatrix<T> const& transitionMatrix, storm::storage::BitVector const& initialStates) {
    storm::storage::BitVector result{initialStates.size()};
    for (uint64_t currentState : initialStates) {
        for (auto const& successor : transitionMatrix.getRowGroup(currentState)) {
            STORM_LOG_ASSERT(!storm::utility::isZero(successor.getValue()), "Matrix should not have zero entries.");
            result.set(successor.getColumn());
        }
    }
    return result;
}

template<typename T>
storm::storage::BitVector getReachableStates(storm::storage::SparseMatrix<T> const& transitionMatrix, storm::storage::BitVector const& initialStates,
                                             storm::storage::BitVector const& constraintStates, storm::storage::BitVector const& targetStates,
                                             bool useStepBound, uint_fast64_t maximalSteps, boost::optional<storm::storage::BitVector> const& choiceFilter) {
    storm::storage::BitVector reachableStates(initialStates);

    uint_fast64_t numberOfStates = transitionMatrix.getRowGroupCount();

    // Initialize the stack used for the DFS with the states.
    std::vector<uint_fast64_t> stack;
    stack.reserve(initialStates.size());
    for (auto state : initialStates) {
        if (constraintStates.get(state)) {
            stack.push_back(state);
        }
    }

    // Initialize the stack for the step bound, if the number of steps is bounded.
    std::vector<uint_fast64_t> stepStack;
    std::vector<uint_fast64_t> remainingSteps;
    if (useStepBound) {
        stepStack.reserve(numberOfStates);
        stepStack.insert(stepStack.begin(), stack.size(), maximalSteps);
        remainingSteps.resize(numberOfStates);
        for (auto state : stack) {
            remainingSteps[state] = maximalSteps;
        }
    }

    // Perform the actual DFS.
    uint_fast64_t currentState = 0, currentStepBound = 0;
    while (!stack.empty()) {
        currentState = stack.back();
        stack.pop_back();

        if (useStepBound) {
            currentStepBound = stepStack.back();
            stepStack.pop_back();

            if (currentStepBound == 0) {
                continue;
            }
        }

        uint64_t row = transitionMatrix.getRowGroupIndices()[currentState];
        if (choiceFilter) {
            row = choiceFilter->getNextSetIndex(row);
        }
        uint64_t const rowGroupEnd = transitionMatrix.getRowGroupIndices()[currentState + 1];
        while (row < rowGroupEnd) {
            for (auto const& successor : transitionMatrix.getRow(row)) {
                // Only explore the state if the transition was actually there and the successor has not yet
                // been visited.
                if (!storm::utility::isZero(successor.getValue()) &&
                    (!reachableStates.get(successor.getColumn()) || (useStepBound && remainingSteps[successor.getColumn()] < currentStepBound - 1))) {
                    // If the successor is one of the target states, we need to include it, but must not explore
                    // it further.
                    if (targetStates.get(successor.getColumn())) {
                        reachableStates.set(successor.getColumn());
                    } else if (constraintStates.get(successor.getColumn())) {
                        // However, if the state is in the constrained set of states, we potentially need to follow it.
                        if (useStepBound) {
                            // As there is at least one more step to go, we need to push the state and the new number of steps.
                            remainingSteps[successor.getColumn()] = currentStepBound - 1;
                            stepStack.push_back(currentStepBound - 1);
                        }
                        reachableStates.set(successor.getColumn());
                        stack.push_back(successor.getColumn());
                    }
                }
            }
            ++row;
            if (choiceFilter) {
                row = choiceFilter->getNextSetIndex(row);
            }
        }
    }

    return reachableStates;
}

template<typename T>
storm::storage::BitVector getBsccCover(storm::storage::SparseMatrix<T> const& transitionMatrix) {
    storm::storage::BitVector result(transitionMatrix.getRowGroupCount());
    storm::storage::StronglyConnectedComponentDecomposition<T> decomposition(transitionMatrix,
                                                                             storm::storage::StronglyConnectedComponentDecompositionOptions().onlyBottomSccs());

    // Take the first state out of each BSCC.
    for (auto const& scc : decomposition) {
        result.set(*scc.begin());
    }

    return result;
}

template<typename T>
bool hasCycle(storm::storage::SparseMatrix<T> const& transitionMatrix, boost::optional<storm::storage::BitVector> const& subsystem) {
    storm::storage::BitVector unexploredStates;  // States that have not been visited yet
    storm::storage::BitVector acyclicStates;     // States that are known to not lie on a cycle consisting of subsystem states
    if (subsystem) {
        unexploredStates = subsystem.get();
        acyclicStates = ~subsystem.get();
    } else {
        unexploredStates.resize(transitionMatrix.getRowGroupCount(), true);
        acyclicStates.resize(transitionMatrix.getRowGroupCount(), false);
    }
    std::vector<uint64_t> dfsStack;
    for (uint64_t start = unexploredStates.getNextSetIndex(0); start < unexploredStates.size(); start = unexploredStates.getNextSetIndex(start + 1)) {
        dfsStack.push_back(start);
        while (!dfsStack.empty()) {
            uint64_t state = dfsStack.back();
            if (unexploredStates.get(state)) {
                unexploredStates.set(state, false);
                for (auto const& entry : transitionMatrix.getRowGroup(state)) {
                    if (!storm::utility::isZero(entry.getValue())) {
                        if (unexploredStates.get(entry.getColumn())) {
                            dfsStack.push_back(entry.getColumn());
                        } else {
                            if (!acyclicStates.get(entry.getColumn())) {
                                // The state has been visited before but is not known to be acyclic.
                                return true;
                            }
                        }
                    }
                }
            } else {
                acyclicStates.set(state, true);
                dfsStack.pop_back();
            }
        }
    }
    return false;
}

template<typename T>
bool checkIfECWithChoiceExists(storm::storage::SparseMatrix<T> const& transitionMatrix, storm::storage::SparseMatrix<T> const& backwardTransitions,
                               storm::storage::BitVector const& subsystem, storm::storage::BitVector const& choices) {
    STORM_LOG_THROW(subsystem.size() == transitionMatrix.getRowGroupCount(), storm::exceptions::InvalidArgumentException, "Invalid size of subsystem");
    STORM_LOG_THROW(choices.size() == transitionMatrix.getRowCount(), storm::exceptions::InvalidArgumentException, "Invalid size of choice vector");

    if (subsystem.empty() || choices.empty()) {
        return false;
    }

    storm::storage::BitVector statesWithChoice(transitionMatrix.getRowGroupCount(), false);
    uint_fast64_t state = 0;
    for (auto choice : choices) {
        // Get the correct state
        while (choice >= transitionMatrix.getRowGroupIndices()[state + 1]) {
            ++state;
        }
        assert(choice >= transitionMatrix.getRowGroupIndices()[state]);
        // make sure that the choice originates from the subsystem and also stays within the subsystem
        if (subsystem.get(state)) {
            bool choiceStaysInSubsys = true;
            for (auto const& entry : transitionMatrix.getRow(choice)) {
                if (!subsystem.get(entry.getColumn())) {
                    choiceStaysInSubsys = false;
                    break;
                }
            }
            if (choiceStaysInSubsys) {
                statesWithChoice.set(state, true);
            }
        }
    }

    // Initialize candidate states that satisfy some necessary conditions for being part of an EC with a specified choice:

    // Get the states for which a policy can enforce that a choice is reached while staying inside the subsystem
    storm::storage::BitVector candidateStates =
        storm::utility::graph::performProb1E(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, subsystem, statesWithChoice);

    // Only keep the states that can stay in the set of candidates forever
    candidateStates =
        storm::utility::graph::performProb0E(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, candidateStates, ~candidateStates);

    // Only keep the states that can be reached after performing one of the specified choices
    statesWithChoice &= candidateStates;
    storm::storage::BitVector choiceTargets(transitionMatrix.getRowGroupCount(), false);
    for (auto state : statesWithChoice) {
        for (uint_fast64_t choice = choices.getNextSetIndex(transitionMatrix.getRowGroupIndices()[state]);
             choice < transitionMatrix.getRowGroupIndices()[state + 1]; choice = choices.getNextSetIndex(choice + 1)) {
            bool choiceStaysInCandidateSet = true;
            for (auto const& entry : transitionMatrix.getRow(choice)) {
                if (!candidateStates.get(entry.getColumn())) {
                    choiceStaysInCandidateSet = false;
                    break;
                }
            }
            if (choiceStaysInCandidateSet) {
                for (auto const& entry : transitionMatrix.getRow(choice)) {
                    choiceTargets.set(entry.getColumn(), true);
                }
            }
        }
    }
    candidateStates =
        storm::utility::graph::getReachableStates(transitionMatrix, choiceTargets, candidateStates, storm::storage::BitVector(candidateStates.size(), false));

    // At this point we know that every candidate state can reach a state with a choice without leaving the set of candidate states.
    // We now compute the states that can reach a choice at least twice, three times, four times, ... until a fixpoint is reached.
    while (!candidateStates.empty()) {
        // Update the states with a choice that stays within the set of candidates
        statesWithChoice &= candidateStates;
        for (auto state : statesWithChoice) {
            bool stateHasChoice = false;
            for (uint_fast64_t choice = choices.getNextSetIndex(transitionMatrix.getRowGroupIndices()[state]);
                 choice < transitionMatrix.getRowGroupIndices()[state + 1]; choice = choices.getNextSetIndex(choice + 1)) {
                bool choiceStaysInCandidateSet = true;
                for (auto const& entry : transitionMatrix.getRow(choice)) {
                    if (!candidateStates.get(entry.getColumn())) {
                        choiceStaysInCandidateSet = false;
                        break;
                    }
                }
                if (choiceStaysInCandidateSet) {
                    stateHasChoice = true;
                    break;
                }
            }
            if (!stateHasChoice) {
                statesWithChoice.set(state, false);
            }
        }

        // Update the candidates
        storm::storage::BitVector newCandidates = storm::utility::graph::performProb1E(transitionMatrix, transitionMatrix.getRowGroupIndices(),
                                                                                       backwardTransitions, candidateStates, statesWithChoice);

        // Check if converged
        if (newCandidates == candidateStates) {
            assert(!candidateStates.empty());
            return true;
        }
        candidateStates = std::move(newCandidates);
    }
    return false;
}

template<typename T>
std::vector<uint_fast64_t> getDistances(storm::storage::SparseMatrix<T> const& transitionMatrix, storm::storage::BitVector const& initialStates,
                                        boost::optional<storm::storage::BitVector> const& subsystem) {
    std::vector<uint_fast64_t> distances(transitionMatrix.getRowGroupCount());

    std::vector<std::pair<storm::storage::sparse::state_type, uint_fast64_t>> stateQueue;
    stateQueue.reserve(transitionMatrix.getRowGroupCount());
    storm::storage::BitVector statesInQueue(transitionMatrix.getRowGroupCount());

    storm::storage::sparse::state_type currentPosition = 0;
    for (auto initialState : initialStates) {
        stateQueue.emplace_back(initialState, 0);
        statesInQueue.set(initialState);
    }

    // Perform a BFS.
    while (currentPosition < stateQueue.size()) {
        std::pair<storm::storage::sparse::state_type, std::size_t> const& stateDistancePair = stateQueue[currentPosition];
        distances[stateDistancePair.first] = stateDistancePair.second;

        for (auto const& successorEntry : transitionMatrix.getRowGroup(stateDistancePair.first)) {
            if (!statesInQueue.get(successorEntry.getColumn())) {
                if (!subsystem || subsystem.get()[successorEntry.getColumn()]) {
                    stateQueue.emplace_back(successorEntry.getColumn(), stateDistancePair.second + 1);
                    statesInQueue.set(successorEntry.getColumn());
                }
            }
        }
        ++currentPosition;
    }

    return distances;
}

template<typename T>
storm::storage::BitVector performProbGreater0(storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                              storm::storage::BitVector const& psiStates, bool useStepBound, uint_fast64_t maximalSteps) {
    // Prepare the resulting bit vector.
    uint_fast64_t numberOfStates = phiStates.size();
    storm::storage::BitVector statesWithProbabilityGreater0(numberOfStates);

    // Add all psi states as they already satisfy the condition.
    statesWithProbabilityGreater0 |= psiStates;

    // Initialize the stack used for the DFS with the states.
    std::vector<uint_fast64_t> stack(psiStates.begin(), psiStates.end());

    // Initialize the stack for the step bound, if the number of steps is bounded.
    std::vector<uint_fast64_t> stepStack;
    std::vector<uint_fast64_t> remainingSteps;
    if (useStepBound) {
        stepStack.reserve(numberOfStates);
        stepStack.insert(stepStack.begin(), psiStates.getNumberOfSetBits(), maximalSteps);
        remainingSteps.resize(numberOfStates);
        for (auto state : psiStates) {
            remainingSteps[state] = maximalSteps;
        }
    }

    // Perform the actual DFS.
    uint_fast64_t currentState, currentStepBound;
    while (!stack.empty()) {
        currentState = stack.back();
        stack.pop_back();

        if (useStepBound) {
            currentStepBound = stepStack.back();
            stepStack.pop_back();
            if (currentStepBound == 0) {
                continue;
            }
        }

        for (typename storm::storage::SparseMatrix<T>::const_iterator entryIt = backwardTransitions.begin(currentState),
                                                                      entryIte = backwardTransitions.end(currentState);
             entryIt != entryIte; ++entryIt) {
            if (phiStates[entryIt->getColumn()] &&
                (!statesWithProbabilityGreater0.get(entryIt->getColumn()) || (useStepBound && remainingSteps[entryIt->getColumn()] < currentStepBound - 1))) {
                statesWithProbabilityGreater0.set(entryIt->getColumn(), true);

                // If we don't have a bound on the number of steps to take, just add the state to the stack.
                if (useStepBound) {
                    // As there is at least one more step to go, we need to push the state and the new number of steps.
                    remainingSteps[entryIt->getColumn()] = currentStepBound - 1;
                    stepStack.push_back(currentStepBound - 1);
                }
                stack.push_back(entryIt->getColumn());
            }
        }
    }

    // Return result.
    return statesWithProbabilityGreater0;
}

template<typename T>
storm::storage::BitVector performProb1(storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const&,
                                       storm::storage::BitVector const& psiStates, storm::storage::BitVector const& statesWithProbabilityGreater0) {
    storm::storage::BitVector statesWithProbability1 = performProbGreater0(backwardTransitions, ~psiStates, ~statesWithProbabilityGreater0);
    statesWithProbability1.complement();
    return statesWithProbability1;
}

template<typename T>
storm::storage::BitVector performProb1(storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                       storm::storage::BitVector const& psiStates) {
    storm::storage::BitVector statesWithProbabilityGreater0 = performProbGreater0(backwardTransitions, phiStates, psiStates);
    storm::storage::BitVector statesWithProbability1 = performProbGreater0(backwardTransitions, ~psiStates, ~(statesWithProbabilityGreater0));
    statesWithProbability1.complement();
    return statesWithProbability1;
}

template<typename T>
std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(storm::models::sparse::DeterministicModel<T> const& model,
                                                                              storm::storage::BitVector const& phiStates,
                                                                              storm::storage::BitVector const& psiStates) {
    std::pair<storm::storage::BitVector, storm::storage::BitVector> result;
    storm::storage::SparseMatrix<T> backwardTransitions = model.getBackwardTransitions();
    result.first = performProbGreater0(backwardTransitions, phiStates, psiStates);
    result.second = performProb1(backwardTransitions, phiStates, psiStates, result.first);
    result.first.complement();
    return result;
}

template<typename T>
std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(storm::storage::SparseMatrix<T> const& backwardTransitions,
                                                                              storm::storage::BitVector const& phiStates,
                                                                              storm::storage::BitVector const& psiStates) {
    std::pair<storm::storage::BitVector, storm::storage::BitVector> result;
    result.first = performProbGreater0(backwardTransitions, phiStates, psiStates);
    result.second = performProb1(backwardTransitions, phiStates, psiStates, result.first);
    result.first.complement();
    return result;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> performProbGreater0(storm::models::symbolic::Model<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                         storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates,
                                         boost::optional<uint_fast64_t> const& stepBound) {
    // Initialize environment for backward search.
    storm::dd::DdManager<Type> const& manager = model.getManager();
    storm::dd::Bdd<Type> lastIterationStates = manager.getBddZero();
    storm::dd::Bdd<Type> statesWithProbabilityGreater0 = psiStates;

    uint_fast64_t iterations = 0;
    while (lastIterationStates != statesWithProbabilityGreater0) {
        if (stepBound && iterations >= stepBound.get()) {
            break;
        }

        lastIterationStates = statesWithProbabilityGreater0;
        statesWithProbabilityGreater0 =
            statesWithProbabilityGreater0.inverseRelationalProduct(transitionMatrix, model.getRowVariables(), model.getColumnVariables());
        statesWithProbabilityGreater0 &= phiStates;
        statesWithProbabilityGreater0 |= lastIterationStates;
        ++iterations;
    }

    return statesWithProbabilityGreater0;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> performProb1(storm::models::symbolic::Model<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                  storm::dd::Bdd<Type> const&, storm::dd::Bdd<Type> const& psiStates,
                                  storm::dd::Bdd<Type> const& statesWithProbabilityGreater0) {
    storm::dd::Bdd<Type> statesWithProbability1 =
        performProbGreater0(model, transitionMatrix, !psiStates && model.getReachableStates(), !statesWithProbabilityGreater0 && model.getReachableStates());
    statesWithProbability1 = !statesWithProbability1 && model.getReachableStates();
    return statesWithProbability1;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> performProb1(storm::models::symbolic::Model<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                  storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates) {
    storm::dd::Bdd<Type> statesWithProbabilityGreater0 = performProbGreater0(model, transitionMatrix, phiStates, psiStates);
    return performProb1(model, transitionMatrix, phiStates, psiStates, statesWithProbabilityGreater0);
}

template<storm::dd::DdType Type, typename ValueType>
std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> performProb01(storm::models::symbolic::DeterministicModel<Type, ValueType> const& model,
                                                                    storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates) {
    std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> result;
    storm::dd::Bdd<Type> transitionMatrix = model.getTransitionMatrix().notZero();
    result.first = performProbGreater0(model, transitionMatrix, phiStates, psiStates);
    result.second = !performProbGreater0(model, transitionMatrix, !psiStates && model.getReachableStates(), !result.first && model.getReachableStates()) &&
                    model.getReachableStates();
    result.first = !result.first && model.getReachableStates();
    return result;
}

template<storm::dd::DdType Type, typename ValueType>
std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> performProb01(storm::models::symbolic::Model<Type, ValueType> const& model,
                                                                    storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                                                    storm::dd::Bdd<Type> const& psiStates) {
    std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> result;
    result.first = performProbGreater0(model, transitionMatrix, phiStates, psiStates);
    result.second = !performProbGreater0(model, transitionMatrix, !psiStates && model.getReachableStates(), !result.first && model.getReachableStates()) &&
                    model.getReachableStates();
    result.first = !result.first && model.getReachableStates();
    return result;
}

template<typename T, typename SchedulerValueType>
void computeSchedulerStayingInStates(storm::storage::BitVector const& states, storm::storage::SparseMatrix<T> const& transitionMatrix,
                                     storm::storage::Scheduler<SchedulerValueType>& scheduler, boost::optional<storm::storage::BitVector> const& rowFilter) {
    std::vector<uint_fast64_t> const& nondeterministicChoiceIndices = transitionMatrix.getRowGroupIndices();

    for (auto state : states) {
        [[maybe_unused]] bool setValue{false};
        for (auto choice : transitionMatrix.getRowGroupIndices(state)) {
            if (rowFilter && !rowFilter->get(choice)) {
                continue;
            }
            auto const row = transitionMatrix.getRow(choice);
            bool const allSuccessorsInStates = std::all_of(row.begin(), row.end(), [&states](auto const& entry) { return states.get(entry.getColumn()); });
            if (allSuccessorsInStates) {
                for (uint_fast64_t memState = 0; memState < scheduler.getNumberOfMemoryStates(); ++memState) {
                    scheduler.setChoice(choice - nondeterministicChoiceIndices[state], state, memState);
                }
                setValue = true;
                break;
            }
        }
        STORM_LOG_ASSERT(setValue, "Expected that at least one action for state " << state << " stays within the selected state");
    }
}

template<typename T, typename SchedulerValueType>
void computeSchedulerWithOneSuccessorInStates(storm::storage::BitVector const& states, storm::storage::SparseMatrix<T> const& transitionMatrix,
                                              storm::storage::Scheduler<SchedulerValueType>& scheduler) {
    std::vector<uint_fast64_t> const& nondeterministicChoiceIndices = transitionMatrix.getRowGroupIndices();

    for (auto state : states) {
        [[maybe_unused]] bool setValue = false;
        for (uint_fast64_t choice = nondeterministicChoiceIndices[state]; choice < nondeterministicChoiceIndices[state + 1]; ++choice) {
            bool oneSuccessorInStates = false;
            for (auto const& element : transitionMatrix.getRow(choice)) {
                if (states.get(element.getColumn())) {
                    oneSuccessorInStates = true;
                    break;
                }
            }
            if (oneSuccessorInStates) {
                for (uint_fast64_t memState = 0; memState < scheduler.getNumberOfMemoryStates(); ++memState) {
                    scheduler.setChoice(choice - nondeterministicChoiceIndices[state], state, memState);
                }
                setValue = true;
                break;
            }
        }
        STORM_LOG_ASSERT(setValue, "Expected that at least one action for state " << state << " leads with positive probability to the selected state");
    }
}

template<typename T, typename SchedulerValueType>
void computeSchedulerProbGreater0E(storm::storage::SparseMatrix<T> const& transitionMatrix, storm::storage::SparseMatrix<T> const& backwardTransitions,
                                   storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                   storm::storage::Scheduler<SchedulerValueType>& scheduler, boost::optional<storm::storage::BitVector> const& rowFilter) {
    // Perform backwards DFS from psiStates and find a valid choice for each visited state.

    std::vector<uint_fast64_t> stack;
    storm::storage::BitVector currentStates(psiStates);  // the states that are either psiStates or for which we have found a valid choice.
    stack.insert(stack.end(), currentStates.begin(), currentStates.end());
    uint_fast64_t currentState = 0;

    while (!stack.empty()) {
        currentState = stack.back();
        stack.pop_back();

        for (typename storm::storage::SparseMatrix<T>::const_iterator predecessorEntryIt = backwardTransitions.begin(currentState),
                                                                      predecessorEntryIte = backwardTransitions.end(currentState);
             predecessorEntryIt != predecessorEntryIte; ++predecessorEntryIt) {
            uint_fast64_t const& predecessor = predecessorEntryIt->getColumn();
            if (phiStates.get(predecessor) && !currentStates.get(predecessor)) {
                // The predecessor is a probGreater0E state that has not been considered yet. Let's find the right choice that leads to a state in
                // currentStates.
                bool foundValidChoice = false;
                for (uint_fast64_t row = transitionMatrix.getRowGroupIndices()[predecessor]; row < transitionMatrix.getRowGroupIndices()[predecessor + 1];
                     ++row) {
                    if (rowFilter && !rowFilter->get(row)) {
                        continue;
                    }
                    for (typename storm::storage::SparseMatrix<T>::const_iterator successorEntryIt = transitionMatrix.begin(row),
                                                                                  successorEntryIte = transitionMatrix.end(row);
                         successorEntryIt != successorEntryIte; ++successorEntryIt) {
                        if (currentStates.get(successorEntryIt->getColumn())) {
                            foundValidChoice = true;
                            break;
                        }
                    }
                    if (foundValidChoice) {
                        for (uint_fast64_t memState = 0; memState < scheduler.getNumberOfMemoryStates(); ++memState) {
                            scheduler.setChoice(row - transitionMatrix.getRowGroupIndices()[predecessor], predecessor, memState);
                        }
                        currentStates.set(predecessor, true);
                        stack.push_back(predecessor);
                        break;
                    }
                }
                STORM_LOG_INFO_COND(foundValidChoice, "Could not find a valid choice for ProbGreater0E state " << predecessor << ".");
            }
        }
    }
}

template<typename T, typename SchedulerValueType>
void computeSchedulerProb0E(storm::storage::BitVector const& prob0EStates, storm::storage::SparseMatrix<T> const& transitionMatrix,
                            storm::storage::Scheduler<SchedulerValueType>& scheduler) {
    computeSchedulerStayingInStates(prob0EStates, transitionMatrix, scheduler);
}

template<typename T, typename SchedulerValueType>
void computeSchedulerRewInf(storm::storage::BitVector const& rewInfStates, storm::storage::SparseMatrix<T> const& transitionMatrix,
                            storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::Scheduler<SchedulerValueType>& scheduler) {
    // Get the states from which we can never exit the rewInfStates, i.e. the states satisfying  Pmax=1 [ G "rewInfStates"]
    // or, equivalently, the states satisfying Pmin=0 [ F !"rewInfStates"].
    auto trapStates = performProb0E(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, rewInfStates, ~rewInfStates);
    // Also set a corresponding choice for all those states
    computeSchedulerProb0E(trapStates, transitionMatrix, scheduler);

    // All remaining rewInfStates must reach a trapState with positive probability
    auto remainingStates = rewInfStates & ~trapStates;
    if (!remainingStates.empty()) {
        computeSchedulerProbGreater0E(transitionMatrix, backwardTransitions, remainingStates, trapStates, scheduler);
    }
}

template<typename T, typename SchedulerValueType>
void computeSchedulerProb1E(storm::storage::BitVector const& prob1EStates, storm::storage::SparseMatrix<T> const& transitionMatrix,
                            storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                            storm::storage::BitVector const& psiStates, storm::storage::Scheduler<SchedulerValueType>& scheduler,
                            boost::optional<storm::storage::BitVector> const& rowFilter) {
    // set an arbitrary (valid) choice for the psi states.
    for (auto psiState : psiStates) {
        for (uint_fast64_t memState = 0; memState < scheduler.getNumberOfMemoryStates(); ++memState) {
            if (!scheduler.getChoice(psiState, memState).isDefined()) {
                scheduler.setChoice(0, psiState, memState);
            }
        }
    }

    // Now perform a backwards search from the psi states and store choices with prob. 1
    std::vector<uint_fast64_t> const& nondeterministicChoiceIndices = transitionMatrix.getRowGroupIndices();
    std::vector<uint_fast64_t> stack;
    storm::storage::BitVector currentStates(psiStates);
    stack.insert(stack.end(), currentStates.begin(), currentStates.end());
    uint_fast64_t currentState = 0;

    while (!stack.empty()) {
        currentState = stack.back();
        stack.pop_back();

        for (typename storm::storage::SparseMatrix<T>::const_iterator predecessorEntryIt = backwardTransitions.begin(currentState),
                                                                      predecessorEntryIte = backwardTransitions.end(currentState);
             predecessorEntryIt != predecessorEntryIte; ++predecessorEntryIt) {
            if (phiStates.get(predecessorEntryIt->getColumn()) && !currentStates.get(predecessorEntryIt->getColumn())) {
                // Check whether the predecessor has only successors in the prob1E state set for one of the
                // nondeterminstic choices.
                for (uint_fast64_t row = nondeterministicChoiceIndices[predecessorEntryIt->getColumn()];
                     row < nondeterministicChoiceIndices[predecessorEntryIt->getColumn() + 1]; ++row) {
                    if (!rowFilter || rowFilter.get().get(row)) {
                        bool allSuccessorsInProb1EStates = true;
                        bool hasSuccessorInCurrentStates = false;
                        for (typename storm::storage::SparseMatrix<T>::const_iterator successorEntryIt = transitionMatrix.begin(row),
                                                                                      successorEntryIte = transitionMatrix.end(row);
                             successorEntryIt != successorEntryIte; ++successorEntryIt) {
                            if (!prob1EStates.get(successorEntryIt->getColumn())) {
                                allSuccessorsInProb1EStates = false;
                                break;
                            } else if (currentStates.get(successorEntryIt->getColumn())) {
                                hasSuccessorInCurrentStates = true;
                            }
                        }

                        // If all successors for a given nondeterministic choice are in the prob1E state set, we
                        // perform a backward search from that state.
                        if (allSuccessorsInProb1EStates && hasSuccessorInCurrentStates) {
                            for (uint_fast64_t memState = 0; memState < scheduler.getNumberOfMemoryStates(); ++memState) {
                                scheduler.setChoice(row - nondeterministicChoiceIndices[predecessorEntryIt->getColumn()], predecessorEntryIt->getColumn(),
                                                    memState);
                            }
                            currentStates.set(predecessorEntryIt->getColumn(), true);
                            stack.push_back(predecessorEntryIt->getColumn());
                            break;
                        }
                    }
                }
            }
        }
    }
}

template<typename T>
storm::storage::BitVector performProbGreater0E(storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                               storm::storage::BitVector const& psiStates, bool useStepBound, uint_fast64_t maximalSteps) {
    size_t numberOfStates = phiStates.size();

    // Prepare resulting bit vector.
    storm::storage::BitVector statesWithProbabilityGreater0(numberOfStates);

    // Add all psi states as the already satisfy the condition.
    statesWithProbabilityGreater0 |= psiStates;

    // Initialize the stack used for the DFS with the states
    std::vector<uint_fast64_t> stack(psiStates.begin(), psiStates.end());

    // Initialize the stack for the step bound, if the number of steps is bounded.
    std::vector<uint_fast64_t> stepStack;
    std::vector<uint_fast64_t> remainingSteps;
    if (useStepBound) {
        stepStack.reserve(numberOfStates);
        stepStack.insert(stepStack.begin(), psiStates.getNumberOfSetBits(), maximalSteps);
        remainingSteps.resize(numberOfStates);
        for (auto state : psiStates) {
            remainingSteps[state] = maximalSteps;
        }
    }

    // Perform the actual DFS.
    uint_fast64_t currentState, currentStepBound;
    while (!stack.empty()) {
        currentState = stack.back();
        stack.pop_back();

        if (useStepBound) {
            currentStepBound = stepStack.back();
            stepStack.pop_back();
            if (currentStepBound == 0) {
                continue;
            }
        }

        for (typename storm::storage::SparseMatrix<T>::const_iterator entryIt = backwardTransitions.begin(currentState),
                                                                      entryIte = backwardTransitions.end(currentState);
             entryIt != entryIte; ++entryIt) {
            if (phiStates.get(entryIt->getColumn()) &&
                (!statesWithProbabilityGreater0.get(entryIt->getColumn()) || (useStepBound && remainingSteps[entryIt->getColumn()] < currentStepBound - 1))) {
                // If we don't have a bound on the number of steps to take, just add the state to the stack.
                if (useStepBound) {
                    // If there is at least one more step to go, we need to push the state and the new number of steps.
                    remainingSteps[entryIt->getColumn()] = currentStepBound - 1;
                    stepStack.push_back(currentStepBound - 1);
                }
                statesWithProbabilityGreater0.set(entryIt->getColumn(), true);
                stack.push_back(entryIt->getColumn());
            }
        }
    }

    return statesWithProbabilityGreater0;
}

template<typename T>
storm::storage::BitVector performProb0A(storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates) {
    storm::storage::BitVector statesWithProbability0 = performProbGreater0E(backwardTransitions, phiStates, psiStates);
    statesWithProbability0.complement();
    return statesWithProbability0;
}

template<typename T>
storm::storage::BitVector performProb1E(storm::storage::SparseMatrix<T> const& transitionMatrix,
                                        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                        storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates, boost::optional<storm::storage::BitVector> const& choiceConstraint) {
    size_t numberOfStates = phiStates.size();

    // Initialize the environment for the iterative algorithm.
    storm::storage::BitVector currentStates(numberOfStates, true);
    std::vector<uint_fast64_t> stack;
    stack.reserve(numberOfStates);

    // Perform the loop as long as the set of states gets larger.
    bool done = false;
    uint_fast64_t currentState;
    while (!done) {
        stack.clear();
        storm::storage::BitVector nextStates(psiStates);
        stack.insert(stack.end(), psiStates.begin(), psiStates.end());

        while (!stack.empty()) {
            currentState = stack.back();
            stack.pop_back();

            for (typename storm::storage::SparseMatrix<T>::const_iterator predecessorEntryIt = backwardTransitions.begin(currentState),
                                                                          predecessorEntryIte = backwardTransitions.end(currentState);
                 predecessorEntryIt != predecessorEntryIte; ++predecessorEntryIt) {
                if (phiStates.get(predecessorEntryIt->getColumn()) && !nextStates.get(predecessorEntryIt->getColumn())) {
                    // Check whether the predecessor has only successors in the current state set for one of the
                    // nondeterminstic choices.
                    for (uint_fast64_t row = nondeterministicChoiceIndices[predecessorEntryIt->getColumn()];
                         row < nondeterministicChoiceIndices[predecessorEntryIt->getColumn() + 1]; ++row) {
                        if (!choiceConstraint || choiceConstraint.get().get(row)) {
                            bool allSuccessorsInCurrentStates = true;
                            bool hasNextStateSuccessor = false;
                            for (typename storm::storage::SparseMatrix<T>::const_iterator successorEntryIt = transitionMatrix.begin(row),
                                                                                          successorEntryIte = transitionMatrix.end(row);
                                 successorEntryIt != successorEntryIte; ++successorEntryIt) {
                                if (!currentStates.get(successorEntryIt->getColumn())) {
                                    allSuccessorsInCurrentStates = false;
                                    break;
                                } else if (nextStates.get(successorEntryIt->getColumn())) {
                                    hasNextStateSuccessor = true;
                                }
                            }

                            // If all successors for a given nondeterministic choice are in the current state set, we
                            // add it to the set of states for the next iteration and perform a backward search from
                            // that state.
                            if (allSuccessorsInCurrentStates && hasNextStateSuccessor) {
                                nextStates.set(predecessorEntryIt->getColumn(), true);
                                stack.push_back(predecessorEntryIt->getColumn());
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Check whether we need to perform an additional iteration.
        if (currentStates == nextStates) {
            done = true;
        } else {
            currentStates = std::move(nextStates);
        }
    }

    return currentStates;
}

template<typename T, typename RM>
storm::storage::BitVector performProb1E(storm::models::sparse::NondeterministicModel<T, RM> const& model,
                                        storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates) {
    return performProb1E(model.getTransitionMatrix(), model.getNondeterministicChoiceIndices(), backwardTransitions, phiStates, psiStates);
}

template<typename T>
std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(storm::storage::SparseMatrix<T> const& transitionMatrix,
                                                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                                                 storm::storage::SparseMatrix<T> const& backwardTransitions,
                                                                                 storm::storage::BitVector const& phiStates,
                                                                                 storm::storage::BitVector const& psiStates) {
    std::pair<storm::storage::BitVector, storm::storage::BitVector> result;

    result.first = performProb0A(backwardTransitions, phiStates, psiStates);

    result.second = performProb1E(transitionMatrix, nondeterministicChoiceIndices, backwardTransitions, phiStates, psiStates);
    return result;
}

template<typename T, typename RM>
std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(storm::models::sparse::NondeterministicModel<T, RM> const& model,
                                                                                 storm::storage::BitVector const& phiStates,
                                                                                 storm::storage::BitVector const& psiStates) {
    return performProb01Max(model.getTransitionMatrix(), model.getTransitionMatrix().getRowGroupIndices(), model.getBackwardTransitions(), phiStates,
                            psiStates);
}

template<typename T>
storm::storage::BitVector performProbGreater0A(storm::storage::SparseMatrix<T> const& transitionMatrix,
                                               std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                               storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                               storm::storage::BitVector const& psiStates, bool useStepBound, uint_fast64_t maximalSteps,
                                               boost::optional<storm::storage::BitVector> const& choiceConstraint) {
    size_t numberOfStates = phiStates.size();

    // Prepare resulting bit vector.
    storm::storage::BitVector statesWithProbabilityGreater0(numberOfStates);

    // Add all psi states as the already satisfy the condition.
    statesWithProbabilityGreater0 |= psiStates;

    // Initialize the stack used for the DFS with the states
    std::vector<uint_fast64_t> stack(psiStates.begin(), psiStates.end());

    // Initialize the stack for the step bound, if the number of steps is bounded.
    std::vector<uint_fast64_t> stepStack;
    std::vector<uint_fast64_t> remainingSteps;
    if (useStepBound) {
        stepStack.reserve(numberOfStates);
        stepStack.insert(stepStack.begin(), psiStates.getNumberOfSetBits(), maximalSteps);
        remainingSteps.resize(numberOfStates);
        for (auto state : psiStates) {
            remainingSteps[state] = maximalSteps;
        }
    }

    // Perform the actual DFS.
    uint_fast64_t currentState, currentStepBound;
    while (!stack.empty()) {
        currentState = stack.back();
        stack.pop_back();

        if (useStepBound) {
            currentStepBound = stepStack.back();
            stepStack.pop_back();
            if (currentStepBound == 0) {
                continue;
            }
        }

        for (typename storm::storage::SparseMatrix<T>::const_iterator predecessorEntryIt = backwardTransitions.begin(currentState),
                                                                      predecessorEntryIte = backwardTransitions.end(currentState);
             predecessorEntryIt != predecessorEntryIte; ++predecessorEntryIt) {
            if (phiStates.get(predecessorEntryIt->getColumn())) {
                if (!statesWithProbabilityGreater0.get(predecessorEntryIt->getColumn())) {
                    // Check whether the predecessor has at least one successor in the current state set for every
                    // nondeterministic choice within the possibly given choiceConstraint.

                    // Note: The backwards edge might be induced by a choice that violates the choiceConstraint.
                    // However this is not problematic as long as there is at least one enabled choice for the predecessor.
                    uint_fast64_t row = nondeterministicChoiceIndices[predecessorEntryIt->getColumn()];
                    uint_fast64_t const& endOfGroup = nondeterministicChoiceIndices[predecessorEntryIt->getColumn() + 1];
                    if (!choiceConstraint || choiceConstraint->getNextSetIndex(row) < endOfGroup) {
                        bool addToStatesWithProbabilityGreater0 = true;
                        for (; row < endOfGroup; ++row) {
                            if (!choiceConstraint || choiceConstraint->get(row)) {
                                bool hasAtLeastOneSuccessorWithProbabilityGreater0 = false;
                                for (typename storm::storage::SparseMatrix<T>::const_iterator successorEntryIt = transitionMatrix.begin(row),
                                                                                              successorEntryIte = transitionMatrix.end(row);
                                     successorEntryIt != successorEntryIte; ++successorEntryIt) {
                                    if (statesWithProbabilityGreater0.get(successorEntryIt->getColumn())) {
                                        hasAtLeastOneSuccessorWithProbabilityGreater0 = true;
                                        break;
                                    }
                                }

                                if (!hasAtLeastOneSuccessorWithProbabilityGreater0) {
                                    addToStatesWithProbabilityGreater0 = false;
                                    break;
                                }
                            }
                        }

                        // If we need to add the state, then actually add it and perform further search from the state.
                        if (addToStatesWithProbabilityGreater0) {
                            // If we don't have a bound on the number of steps to take, just add the state to the stack.
                            if (useStepBound) {
                                // If there is at least one more step to go, we need to push the state and the new number of steps.
                                remainingSteps[predecessorEntryIt->getColumn()] = currentStepBound - 1;
                                stepStack.push_back(currentStepBound - 1);
                            }
                            statesWithProbabilityGreater0.set(predecessorEntryIt->getColumn(), true);
                            stack.push_back(predecessorEntryIt->getColumn());
                        }
                    }

                } else if (useStepBound && remainingSteps[predecessorEntryIt->getColumn()] < currentStepBound - 1) {
                    // We have found a shorter path to the predecessor. Hence, we need to explore it again.
                    // If there is a choiceConstraint, we still need to check whether the backwards edge was induced by a valid action
                    bool predecessorIsValid = true;
                    if (choiceConstraint) {
                        predecessorIsValid = false;
                        uint_fast64_t row = choiceConstraint->getNextSetIndex(nondeterministicChoiceIndices[predecessorEntryIt->getColumn()]);
                        uint_fast64_t const& endOfGroup = nondeterministicChoiceIndices[predecessorEntryIt->getColumn() + 1];
                        for (; row < endOfGroup && !predecessorIsValid; row = choiceConstraint->getNextSetIndex(row + 1)) {
                            for (auto const& entry : transitionMatrix.getRow(row)) {
                                if (entry.getColumn() == currentState) {
                                    predecessorIsValid = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (predecessorIsValid) {
                        remainingSteps[predecessorEntryIt->getColumn()] = currentStepBound - 1;
                        stepStack.push_back(currentStepBound - 1);
                        stack.push_back(predecessorEntryIt->getColumn());
                    }
                }
            }
        }
    }

    return statesWithProbabilityGreater0;
}

template<typename T, typename RM>
storm::storage::BitVector performProb0E(storm::models::sparse::NondeterministicModel<T, RM> const& model,
                                        storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates) {
    storm::storage::BitVector statesWithProbability0 =
        performProbGreater0A(model.getTransitionMatrix(), model.getNondeterministicChoiceIndices(), backwardTransitions, phiStates, psiStates);
    statesWithProbability0.complement();
    return statesWithProbability0;
}

template<typename T>
storm::storage::BitVector performProb0E(storm::storage::SparseMatrix<T> const& transitionMatrix,
                                        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                        storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates) {
    storm::storage::BitVector statesWithProbability0 =
        performProbGreater0A(transitionMatrix, nondeterministicChoiceIndices, backwardTransitions, phiStates, psiStates);
    statesWithProbability0.complement();
    return statesWithProbability0;
}

template<typename T, typename RM>
storm::storage::BitVector performProb1A(storm::models::sparse::NondeterministicModel<T, RM> const& model,
                                        storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates) {
    return performProb1A(model.getTransitionMatrix(), model.getNondeterministicChoiceIndices(), backwardTransitions, phiStates, psiStates);
}

template<typename T>
storm::storage::BitVector performProb1A(storm::storage::SparseMatrix<T> const& transitionMatrix,
                                        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                        storm::storage::SparseMatrix<T> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                        storm::storage::BitVector const& psiStates) {
    size_t numberOfStates = phiStates.size();

    // Initialize the environment for the iterative algorithm.
    storm::storage::BitVector currentStates(numberOfStates, true);
    std::vector<uint_fast64_t> stack;
    stack.reserve(numberOfStates);

    // Perform the loop as long as the set of states gets smaller.
    bool done = false;
    uint_fast64_t currentState;
    while (!done) {
        stack.clear();
        storm::storage::BitVector nextStates(psiStates);
        stack.insert(stack.end(), psiStates.begin(), psiStates.end());

        while (!stack.empty()) {
            currentState = stack.back();
            stack.pop_back();

            for (typename storm::storage::SparseMatrix<T>::const_iterator predecessorEntryIt = backwardTransitions.begin(currentState),
                                                                          predecessorEntryIte = backwardTransitions.end(currentState);
                 predecessorEntryIt != predecessorEntryIte; ++predecessorEntryIt) {
                if (phiStates.get(predecessorEntryIt->getColumn()) && !nextStates.get(predecessorEntryIt->getColumn())) {
                    // Check whether the predecessor has only successors in the current state set for all of the
                    // nondeterminstic choices and that for each choice there exists a successor that is already
                    // in the next states.
                    bool addToStatesWithProbability1 = true;
                    for (uint_fast64_t row = nondeterministicChoiceIndices[predecessorEntryIt->getColumn()];
                         row < nondeterministicChoiceIndices[predecessorEntryIt->getColumn() + 1]; ++row) {
                        bool hasAtLeastOneSuccessorWithProbability1 = false;
                        for (typename storm::storage::SparseMatrix<T>::const_iterator successorEntryIt = transitionMatrix.begin(row),
                                                                                      successorEntryIte = transitionMatrix.end(row);
                             successorEntryIt != successorEntryIte; ++successorEntryIt) {
                            if (!currentStates.get(successorEntryIt->getColumn())) {
                                addToStatesWithProbability1 = false;
                                goto afterCheckLoop;
                            }
                            if (nextStates.get(successorEntryIt->getColumn())) {
                                hasAtLeastOneSuccessorWithProbability1 = true;
                            }
                        }

                        if (!hasAtLeastOneSuccessorWithProbability1) {
                            addToStatesWithProbability1 = false;
                            break;
                        }
                    }

                afterCheckLoop:
                    // If all successors for all nondeterministic choices are in the current state set, we
                    // add it to the set of states for the next iteration and perform a backward search from
                    // that state.
                    if (addToStatesWithProbability1) {
                        nextStates.set(predecessorEntryIt->getColumn(), true);
                        stack.push_back(predecessorEntryIt->getColumn());
                    }
                }
            }
        }

        // Check whether we need to perform an additional iteration.
        if (currentStates == nextStates) {
            done = true;
        } else {
            currentStates = std::move(nextStates);
        }
    }
    return currentStates;
}

template<typename T>
std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(storm::storage::SparseMatrix<T> const& transitionMatrix,
                                                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                                                 storm::storage::SparseMatrix<T> const& backwardTransitions,
                                                                                 storm::storage::BitVector const& phiStates,
                                                                                 storm::storage::BitVector const& psiStates) {
    std::pair<storm::storage::BitVector, storm::storage::BitVector> result;
    result.first = performProb0E(transitionMatrix, nondeterministicChoiceIndices, backwardTransitions, phiStates, psiStates);
    // Instead of calling performProb1A, we call the (more easier) performProb0A on the Prob0E states.
    // This is valid because, when minimizing probabilities, states that have prob1 cannot reach a state with prob 0 (and will eventually reach a psiState).
    // States that do not have prob1 will eventually reach a state with prob0.
    result.second = performProb0A(backwardTransitions, ~psiStates, result.first);
    return result;
}

template<typename T, typename RM>
std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(storm::models::sparse::NondeterministicModel<T, RM> const& model,
                                                                                 storm::storage::BitVector const& phiStates,
                                                                                 storm::storage::BitVector const& psiStates) {
    return performProb01Min(model.getTransitionMatrix(), model.getTransitionMatrix().getRowGroupIndices(), model.getBackwardTransitions(), phiStates,
                            psiStates);
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> computeSchedulerProbGreater0E(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                                   storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                                   storm::dd::Bdd<Type> const& psiStates) {
    // Initialize environment for backward search.
    storm::dd::DdManager<Type> const& manager = model.getManager();
    storm::dd::Bdd<Type> statesWithProbabilityGreater0E = manager.getBddZero();
    storm::dd::Bdd<Type> frontier = psiStates;
    storm::dd::Bdd<Type> scheduler = manager.getBddZero();

    while (!frontier.isZero()) {
        storm::dd::Bdd<Type> statesAndChoicesWithProbabilityGreater0E =
            frontier.inverseRelationalProductWithExtendedRelation(transitionMatrix, model.getRowVariables(), model.getColumnVariables());
        frontier = phiStates && statesAndChoicesWithProbabilityGreater0E.existsAbstract(model.getNondeterminismVariables()) && !statesWithProbabilityGreater0E;
        scheduler = scheduler || (frontier && statesAndChoicesWithProbabilityGreater0E).existsAbstractRepresentative(model.getNondeterminismVariables());
        statesWithProbabilityGreater0E |= frontier;
    }

    return scheduler;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> performProbGreater0E(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                          storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                          storm::dd::Bdd<Type> const& psiStates) {
    // Initialize environment for backward search.
    storm::dd::DdManager<Type> const& manager = model.getManager();
    storm::dd::Bdd<Type> lastIterationStates = manager.getBddZero();
    storm::dd::Bdd<Type> statesWithProbabilityGreater0E = psiStates;

    storm::dd::Bdd<Type> abstractedTransitionMatrix = transitionMatrix.existsAbstract(model.getNondeterminismVariables());
    while (lastIterationStates != statesWithProbabilityGreater0E) {
        lastIterationStates = statesWithProbabilityGreater0E;
        statesWithProbabilityGreater0E =
            statesWithProbabilityGreater0E.inverseRelationalProduct(abstractedTransitionMatrix, model.getRowVariables(), model.getColumnVariables());
        statesWithProbabilityGreater0E &= phiStates;
        statesWithProbabilityGreater0E |= lastIterationStates;
    }

    return statesWithProbabilityGreater0E;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> performProb0A(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                   storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates) {
    return !performProbGreater0E(model, transitionMatrix, phiStates, psiStates) && model.getReachableStates();
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> performProbGreater0A(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                          storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                          storm::dd::Bdd<Type> const& psiStates) {
    // Initialize environment for backward search.
    storm::dd::DdManager<Type> const& manager = model.getManager();
    storm::dd::Bdd<Type> lastIterationStates = manager.getBddZero();
    storm::dd::Bdd<Type> statesWithProbabilityGreater0A = psiStates;

    while (lastIterationStates != statesWithProbabilityGreater0A) {
        lastIterationStates = statesWithProbabilityGreater0A;
        statesWithProbabilityGreater0A =
            statesWithProbabilityGreater0A.inverseRelationalProductWithExtendedRelation(transitionMatrix, model.getRowVariables(), model.getColumnVariables());
        statesWithProbabilityGreater0A |= model.getIllegalMask();
        statesWithProbabilityGreater0A = statesWithProbabilityGreater0A.universalAbstract(model.getNondeterminismVariables());
        statesWithProbabilityGreater0A &= phiStates;
        statesWithProbabilityGreater0A |= psiStates;
    }

    return statesWithProbabilityGreater0A;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> performProb0E(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                   storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates) {
    return !performProbGreater0A(model, transitionMatrix, phiStates, psiStates) && model.getReachableStates();
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> performProb1A(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                   storm::dd::Bdd<Type> const& psiStates, storm::dd::Bdd<Type> const& statesWithProbabilityGreater0A) {
    // Initialize environment for backward search.
    storm::dd::DdManager<Type> const& manager = model.getManager();
    storm::dd::Bdd<Type> lastIterationStates = manager.getBddZero();
    storm::dd::Bdd<Type> statesWithProbability1A = psiStates || statesWithProbabilityGreater0A;

    while (lastIterationStates != statesWithProbability1A) {
        lastIterationStates = statesWithProbability1A;
        statesWithProbability1A = statesWithProbability1A.swapVariables(model.getRowColumnMetaVariablePairs());
        statesWithProbability1A = transitionMatrix.implies(statesWithProbability1A).universalAbstract(model.getColumnVariables());
        statesWithProbability1A |= model.getIllegalMask();
        statesWithProbability1A = statesWithProbability1A.universalAbstract(model.getNondeterminismVariables());
        statesWithProbability1A &= statesWithProbabilityGreater0A;
        statesWithProbability1A |= psiStates;
    }

    return statesWithProbability1A;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> performProb1E(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model, storm::dd::Bdd<Type> const& transitionMatrix,
                                   storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates,
                                   storm::dd::Bdd<Type> const& statesWithProbabilityGreater0E) {
    // Initialize environment for backward search.
    storm::dd::DdManager<Type> const& manager = model.getManager();
    storm::dd::Bdd<Type> statesWithProbability1E = statesWithProbabilityGreater0E;

    bool outerLoopDone = false;
    while (!outerLoopDone) {
        storm::dd::Bdd<Type> innerStates = manager.getBddZero();

        bool innerLoopDone = false;
        while (!innerLoopDone) {
            storm::dd::Bdd<Type> temporary = statesWithProbability1E.swapVariables(model.getRowColumnMetaVariablePairs());
            temporary = transitionMatrix.implies(temporary).universalAbstract(model.getColumnVariables());

            storm::dd::Bdd<Type> temporary2 =
                innerStates.inverseRelationalProductWithExtendedRelation(transitionMatrix, model.getRowVariables(), model.getColumnVariables());

            temporary = temporary.andExists(temporary2, model.getNondeterminismVariables());
            temporary &= phiStates;
            temporary |= psiStates;

            if (innerStates == temporary) {
                innerLoopDone = true;
            } else {
                innerStates = temporary;
            }
        }

        if (statesWithProbability1E == innerStates) {
            outerLoopDone = true;
        } else {
            statesWithProbability1E = innerStates;
        }
    }

    return statesWithProbability1E;
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> computeSchedulerProb1E(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                            storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                            storm::dd::Bdd<Type> const& psiStates, storm::dd::Bdd<Type> const& statesWithProbability1E) {
    // Initialize environment for backward search.
    storm::dd::DdManager<Type> const& manager = model.getManager();
    storm::dd::Bdd<Type> scheduler = manager.getBddZero();

    storm::dd::Bdd<Type> innerStates = manager.getBddZero();

    bool innerLoopDone = false;
    while (!innerLoopDone) {
        storm::dd::Bdd<Type> temporary = statesWithProbability1E.swapVariables(model.getRowColumnMetaVariablePairs());
        temporary = transitionMatrix.implies(temporary).universalAbstract(model.getColumnVariables());

        storm::dd::Bdd<Type> temporary2 =
            innerStates.inverseRelationalProductWithExtendedRelation(transitionMatrix, model.getRowVariables(), model.getColumnVariables());
        temporary &= temporary2;
        temporary &= phiStates;

        // Extend the scheduler for those states that have not been seen as inner states before.
        scheduler |= (temporary && !innerStates).existsAbstractRepresentative(model.getNondeterminismVariables());

        temporary = temporary.existsAbstract(model.getNondeterminismVariables());
        temporary |= psiStates;

        if (innerStates == temporary) {
            innerLoopDone = true;
        } else {
            innerStates = temporary;
        }
    }

    return scheduler;
}

template<storm::dd::DdType Type, typename ValueType>
std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> performProb01Max(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                                                       storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates) {
    return performProb01Max(model, model.getTransitionMatrix().notZero(), phiStates, psiStates);
}

template<storm::dd::DdType Type, typename ValueType>
std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> performProb01Max(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                                                       storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                                                       storm::dd::Bdd<Type> const& psiStates) {
    std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> result;
    result.first = performProb0A(model, transitionMatrix, phiStates, psiStates);
    result.second = performProb1E(model, transitionMatrix, phiStates, psiStates, !result.first && model.getReachableStates());
    return result;
}

template<storm::dd::DdType Type, typename ValueType>
std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> performProb01Min(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                                                       storm::dd::Bdd<Type> const& phiStates, storm::dd::Bdd<Type> const& psiStates) {
    return performProb01Min(model, model.getTransitionMatrix().notZero(), phiStates, psiStates);
}

template<storm::dd::DdType Type, typename ValueType>
std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> performProb01Min(storm::models::symbolic::NondeterministicModel<Type, ValueType> const& model,
                                                                       storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                                                       storm::dd::Bdd<Type> const& psiStates) {
    std::pair<storm::dd::Bdd<Type>, storm::dd::Bdd<Type>> result;
    result.first = performProb0E(model, transitionMatrix, phiStates, psiStates);
    result.second = performProb1A(model, transitionMatrix, psiStates, !result.first && model.getReachableStates());
    return result;
}

template<typename ValueType>
ExplicitGameProb01Result performProb0(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<uint64_t> const& player1Groups,
                                      storm::storage::SparseMatrix<ValueType> const& player1BackwardTransitions,
                                      std::vector<uint64_t> const& player2BackwardTransitions, storm::storage::BitVector const& phiStates,
                                      storm::storage::BitVector const& psiStates, storm::OptimizationDirection const& player1Direction,
                                      storm::OptimizationDirection const& player2Direction, storm::storage::ExplicitGameStrategyPair* strategyPair) {
    ExplicitGameProb01Result result(psiStates, storm::storage::BitVector(transitionMatrix.getRowGroupCount()));

    // Initialize the stack used for the DFS with the states
    std::vector<uint_fast64_t> stack(psiStates.begin(), psiStates.end());

    // Perform the actual DFS.
    uint_fast64_t currentState;
    while (!stack.empty()) {
        currentState = stack.back();
        stack.pop_back();

        // Check which player 2 predecessors of the current player 1 state to add.
        for (auto const& player2PredecessorEntry : player1BackwardTransitions.getRow(currentState)) {
            uint64_t player2Predecessor = player2PredecessorEntry.getColumn();
            if (!result.player2States.get(player2Predecessor)) {
                bool addPlayer2State = false;
                if (player2Direction == OptimizationDirection::Minimize) {
                    bool allChoicesHavePlayer1State = true;
                    for (uint64_t row = transitionMatrix.getRowGroupIndices()[player2Predecessor];
                         row < transitionMatrix.getRowGroupIndices()[player2Predecessor + 1]; ++row) {
                        bool choiceHasPlayer1State = false;
                        for (auto const& entry : transitionMatrix.getRow(row)) {
                            if (result.player1States.get(entry.getColumn())) {
                                choiceHasPlayer1State = true;
                                break;
                            }
                        }
                        if (!choiceHasPlayer1State) {
                            allChoicesHavePlayer1State = false;
                        }
                    }
                    if (allChoicesHavePlayer1State) {
                        addPlayer2State = true;
                    }
                } else {
                    addPlayer2State = true;
                }

                if (addPlayer2State) {
                    result.player2States.set(player2Predecessor);

                    // Now check whether adding the player 2 state changes something with respect to the
                    // (single) player 1 predecessor.
                    uint64_t player1Predecessor = player2BackwardTransitions[player2Predecessor];

                    if (!result.player1States.get(player1Predecessor)) {
                        bool addPlayer1State = false;
                        if (player1Direction == OptimizationDirection::Minimize) {
                            bool allPlayer2Successors = true;
                            for (uint64_t player2State = player1Groups[player1Predecessor]; player2State < player1Groups[player1Predecessor + 1];
                                 ++player2State) {
                                if (!result.player2States.get(player2State)) {
                                    allPlayer2Successors = false;
                                    break;
                                }
                            }
                            if (allPlayer2Successors) {
                                addPlayer1State = true;
                            }
                        } else {
                            addPlayer1State = true;
                        }

                        if (addPlayer1State) {
                            result.player1States.set(player1Predecessor);
                            stack.emplace_back(player1Predecessor);
                        }
                    }
                }
            }
        }
    }

    // Since we have determined the complements of the desired sets, we need to complement it now.
    result.player1States.complement();
    result.player2States.complement();

    // Generate player 1 strategy if required.
    if (strategyPair) {
        for (auto player1State : result.player1States) {
            if (player1Direction == storm::OptimizationDirection::Minimize) {
                // At least one player 2 successor is a state with probability 0, find it.
                [[maybe_unused]] bool foundProb0Successor = false;
                uint64_t player2State;
                for (player2State = player1Groups[player1State]; player2State < player1Groups[player1State + 1]; ++player2State) {
                    if (result.player2States.get(player2State)) {
                        foundProb0Successor = true;
                        break;
                    }
                }
                STORM_LOG_ASSERT(foundProb0Successor, "Expected at least one state 2 successor with probability 0.");
                strategyPair->getPlayer1Strategy().setChoice(player1State, player2State);
            } else {
                // Since all player 2 successors are states with probability 0, just pick any.
                strategyPair->getPlayer1Strategy().setChoice(player1State, player1Groups[player1State]);
            }
        }
    }

    // Generate player 2 strategy if required.
    if (strategyPair) {
        for (auto player2State : result.player2States) {
            if (player2Direction == storm::OptimizationDirection::Minimize) {
                // At least one distribution only has successors with probability 0, find it.
                [[maybe_unused]] bool foundProb0SuccessorDistribution = false;

                uint64_t row;
                for (row = transitionMatrix.getRowGroupIndices()[player2State]; row < transitionMatrix.getRowGroupIndices()[player2State + 1]; ++row) {
                    bool distributionHasOnlyProb0Successors = true;
                    for (auto const& player1SuccessorEntry : transitionMatrix.getRow(row)) {
                        if (!result.player1States.get(player1SuccessorEntry.getColumn())) {
                            distributionHasOnlyProb0Successors = false;
                            break;
                        }
                    }
                    if (distributionHasOnlyProb0Successors) {
                        foundProb0SuccessorDistribution = true;
                        break;
                    }
                }

                STORM_LOG_ASSERT(foundProb0SuccessorDistribution, "Expected at least one distribution with only successors with probability 0.");
                strategyPair->getPlayer2Strategy().setChoice(player2State, row);
            } else {
                // Since all player 1 successors are states with probability 0, just pick any.
                strategyPair->getPlayer2Strategy().setChoice(player2State, transitionMatrix.getRowGroupIndices()[player2State]);
            }
        }
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
SymbolicGameProb01Result<Type> performProb0(storm::models::symbolic::StochasticTwoPlayerGame<Type, ValueType> const& model,
                                            storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                            storm::dd::Bdd<Type> const& psiStates, storm::OptimizationDirection const& player1Strategy,
                                            storm::OptimizationDirection const& player2Strategy, bool producePlayer1Strategy, bool producePlayer2Strategy) {
    // The solution sets.
    storm::dd::Bdd<Type> player1States = psiStates;
    storm::dd::Bdd<Type> player2States = model.getManager().getBddZero();

    bool done = false;
    while (!done) {
        storm::dd::Bdd<Type> tmp =
            (transitionMatrix && player1States.swapVariables(model.getRowColumnMetaVariablePairs())).existsAbstract(model.getColumnVariables()) && phiStates;

        if (player2Strategy == OptimizationDirection::Minimize) {
            tmp = (tmp || model.getIllegalPlayer2Mask()).universalAbstract(model.getPlayer2Variables());
        } else {
            tmp = tmp.existsAbstract(model.getPlayer2Variables());
        }
        player2States |= tmp;

        if (player1Strategy == OptimizationDirection::Minimize) {
            tmp = (tmp || model.getIllegalPlayer1Mask()).universalAbstract(model.getPlayer1Variables());
        } else {
            tmp = tmp.existsAbstract(model.getPlayer1Variables());
        }

        // Re-add all previous player 1 states.
        tmp |= player1States;

        if (tmp == player1States) {
            done = true;
        }

        player1States = tmp;
    }

    // Since we have determined the complements of the desired sets, we need to complement it now.
    player1States = !player1States && model.getReachableStates();

    std::set<storm::expressions::Variable> variablesToAbstract(model.getColumnVariables());
    variablesToAbstract.insert(model.getPlayer2Variables().begin(), model.getPlayer2Variables().end());
    player2States = !player2States && transitionMatrix.existsAbstract(variablesToAbstract);

    // Determine all transitions between prob0 states.
    storm::dd::Bdd<Type> transitionsBetweenProb0States =
        player2States && (transitionMatrix && player1States.swapVariables(model.getRowColumnMetaVariablePairs()));

    // Determine the distributions that have only successors that are prob0 states.
    storm::dd::Bdd<Type> onlyProb0Successors = (transitionsBetweenProb0States || model.getIllegalSuccessorMask()).universalAbstract(model.getColumnVariables());

    boost::optional<storm::dd::Bdd<Type>> player2StrategyBdd;
    if (producePlayer2Strategy) {
        // Pick a distribution that has only prob0 successors.
        player2StrategyBdd = onlyProb0Successors.existsAbstractRepresentative(model.getPlayer2Variables());
    }

    boost::optional<storm::dd::Bdd<Type>> player1StrategyBdd;
    if (producePlayer1Strategy) {
        // Move from player 2 choices with only prob0 successors to player 1 choices with only prob 0 successors.
        onlyProb0Successors = (player1States && onlyProb0Successors).existsAbstract(model.getPlayer2Variables());

        // Pick a prob0 player 2 state.
        player1StrategyBdd = onlyProb0Successors.existsAbstractRepresentative(model.getPlayer1Variables());
    }

    return SymbolicGameProb01Result<Type>(player1States, player2States, player1StrategyBdd, player2StrategyBdd);
}

template<typename ValueType>
ExplicitGameProb01Result performProb1(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<uint64_t> const& player1Groups,
                                      storm::storage::SparseMatrix<ValueType> const& player1BackwardTransitions,
                                      std::vector<uint64_t> const& player2BackwardTransitions, storm::storage::BitVector const& phiStates,
                                      storm::storage::BitVector const& psiStates, storm::OptimizationDirection const& player1Direction,
                                      storm::OptimizationDirection const& player2Direction, storm::storage::ExplicitGameStrategyPair* strategyPair,
                                      boost::optional<storm::storage::BitVector> const& player1Candidates) {
    // During the execution, the two state sets in the result hold the potential player 1/2 states.
    ExplicitGameProb01Result result;
    if (player1Candidates) {
        result = ExplicitGameProb01Result(player1Candidates.get(), storm::storage::BitVector(transitionMatrix.getRowGroupCount()));
    } else {
        result = ExplicitGameProb01Result(storm::storage::BitVector(phiStates.size(), true), storm::storage::BitVector(transitionMatrix.getRowGroupCount()));
    }

    // A flag that governs whether strategies are produced in the current iteration.
    bool produceStrategiesInIteration = false;

    // Initialize the stack used for the DFS with the states
    std::vector<uint_fast64_t> stack;
    bool maybeStatesDone = false;
    while (!maybeStatesDone || produceStrategiesInIteration) {
        storm::storage::BitVector player1Solution = psiStates;
        storm::storage::BitVector player2Solution(result.player2States.size());

        stack.clear();
        stack.insert(stack.end(), psiStates.begin(), psiStates.end());

        // Perform the actual DFS.
        uint_fast64_t currentState;
        while (!stack.empty()) {
            currentState = stack.back();
            stack.pop_back();

            for (auto player2PredecessorEntry : player1BackwardTransitions.getRow(currentState)) {
                uint64_t player2Predecessor = player2PredecessorEntry.getColumn();
                if (!player2Solution.get(player2PredecessorEntry.getColumn())) {
                    bool addPlayer2State = player2Direction == storm::OptimizationDirection::Minimize;

                    uint64_t validChoice = transitionMatrix.getRowGroupIndices()[player2Predecessor];
                    for (uint64_t row = validChoice; row < transitionMatrix.getRowGroupIndices()[player2Predecessor + 1]; ++row) {
                        bool choiceHasSolutionSuccessor = false;
                        bool choiceStaysInMaybeStates = true;
                        for (auto const& entry : transitionMatrix.getRow(row)) {
                            if (player1Solution.get(entry.getColumn())) {
                                choiceHasSolutionSuccessor = true;
                            }
                            if (!result.player1States.get(entry.getColumn())) {
                                choiceStaysInMaybeStates = false;
                                break;
                            }
                        }

                        if (choiceHasSolutionSuccessor && choiceStaysInMaybeStates) {
                            if (player2Direction == storm::OptimizationDirection::Maximize) {
                                validChoice = row;
                                addPlayer2State = true;
                                break;
                            }
                        } else if (player2Direction == storm::OptimizationDirection::Minimize) {
                            addPlayer2State = false;
                            break;
                        }
                    }

                    if (addPlayer2State) {
                        player2Solution.set(player2Predecessor);
                        if (produceStrategiesInIteration) {
                            strategyPair->getPlayer2Strategy().setChoice(player2Predecessor, validChoice);
                        }

                        // Check whether the addition of the player 2 state changes the state of the (single)
                        // player 1 predecessor.
                        uint64_t player1Predecessor = player2BackwardTransitions[player2Predecessor];

                        if (!player1Solution.get(player1Predecessor)) {
                            bool addPlayer1State = player1Direction == storm::OptimizationDirection::Minimize;

                            validChoice = player1Groups[player1Predecessor];
                            for (uint64_t player2Successor = validChoice; player2Successor < player1Groups[player1Predecessor + 1]; ++player2Successor) {
                                if (player2Solution.get(player2Successor)) {
                                    if (player1Direction == storm::OptimizationDirection::Maximize) {
                                        validChoice = player2Successor;
                                        addPlayer1State = true;
                                        break;
                                    }
                                } else if (player1Direction == storm::OptimizationDirection::Minimize) {
                                    addPlayer1State = false;
                                    break;
                                }
                            }

                            if (addPlayer1State) {
                                player1Solution.set(player1Predecessor);

                                if (produceStrategiesInIteration) {
                                    strategyPair->getPlayer1Strategy().setChoice(player1Predecessor, validChoice);
                                }

                                stack.emplace_back(player1Predecessor);
                            }
                        }
                    }
                }
            }
        }

        if (result.player1States == player1Solution) {
            maybeStatesDone = true;
            result.player2States = player2Solution;

            // If we were asked to produce strategies, we propagate that by triggering another iteration.
            // We only do this if at least one strategy will be produced.
            produceStrategiesInIteration = !produceStrategiesInIteration && strategyPair;
        } else {
            result.player1States = player1Solution;
            result.player2States = player2Solution;
        }
    }

    return result;
}

template<storm::dd::DdType Type, typename ValueType>
SymbolicGameProb01Result<Type> performProb1(storm::models::symbolic::StochasticTwoPlayerGame<Type, ValueType> const& model,
                                            storm::dd::Bdd<Type> const& transitionMatrix, storm::dd::Bdd<Type> const& phiStates,
                                            storm::dd::Bdd<Type> const& psiStates, storm::OptimizationDirection const& player1Strategy,
                                            storm::OptimizationDirection const& player2Strategy, bool producePlayer1Strategy, bool producePlayer2Strategy,
                                            boost::optional<storm::dd::Bdd<Type>> const& player1Candidates) {
    // Create the potential prob1 states of player 1.
    storm::dd::Bdd<Type> maybePlayer1States = model.getReachableStates();
    if (player1Candidates) {
        maybePlayer1States &= player1Candidates.get();
    }

    // Initialize potential prob1 states of player 2.
    storm::dd::Bdd<Type> maybePlayer2States = model.getManager().getBddZero();

    // A flag that governs whether strategies are produced in the current iteration.
    bool produceStrategiesInIteration = false;
    boost::optional<storm::dd::Bdd<Type>> player1StrategyBdd;
    boost::optional<storm::dd::Bdd<Type>> consideredPlayer1States;
    boost::optional<storm::dd::Bdd<Type>> player2StrategyBdd;
    boost::optional<storm::dd::Bdd<Type>> consideredPlayer2States;

    bool maybeStatesDone = false;
    while (!maybeStatesDone || produceStrategiesInIteration) {
        bool solutionStatesDone = false;

        // If we are to produce strategies in this iteration, we prepare some storage.
        if (produceStrategiesInIteration) {
            if (player1Strategy == storm::OptimizationDirection::Maximize) {
                player1StrategyBdd = model.getManager().getBddZero();
                consideredPlayer1States = model.getManager().getBddZero();
            }
            if (player2Strategy == storm::OptimizationDirection::Maximize) {
                player2StrategyBdd = model.getManager().getBddZero();
                consideredPlayer2States = model.getManager().getBddZero();
            }
        }

        storm::dd::Bdd<Type> player1Solution = psiStates;
        storm::dd::Bdd<Type> player2Solution = model.getManager().getBddZero();
        while (!solutionStatesDone) {
            // Start by computing the transitions that have only maybe states as successors. Note that at
            // this point, there may be illegal transitions.
            storm::dd::Bdd<Type> distributionsStayingInMaybe =
                (!transitionMatrix || maybePlayer1States.swapVariables(model.getRowColumnMetaVariablePairs())).universalAbstract(model.getColumnVariables());

            // Then, determine all distributions that have at least one successor in the states that have
            // probability 1.
            storm::dd::Bdd<Type> distributionsWithProb1Successor =
                (transitionMatrix && player1Solution.swapVariables(model.getRowColumnMetaVariablePairs())).existsAbstract(model.getColumnVariables());

            // The valid distributions are then those that emanate from phi states, stay completely in the
            // maybe states and have at least one successor with probability 1.
            storm::dd::Bdd<Type> valid = phiStates && distributionsStayingInMaybe && distributionsWithProb1Successor;

            // Depending on the strategy of player 2, we need to check whether all choices are valid or
            // there exists a valid choice.
            if (player2Strategy == OptimizationDirection::Minimize) {
                valid = (valid || model.getIllegalPlayer2Mask()).universalAbstract(model.getPlayer2Variables());
            } else {
                if (produceStrategiesInIteration) {
                    storm::dd::Bdd<Type> newValidDistributions = valid && !consideredPlayer2States.get();
                    player2StrategyBdd.get() = player2StrategyBdd.get() || newValidDistributions.existsAbstractRepresentative(model.getPlayer2Variables());
                }

                valid = valid.existsAbstract(model.getPlayer2Variables());

                if (produceStrategiesInIteration) {
                    consideredPlayer2States.get() |= valid;
                }
            }

            player2Solution |= valid;

            // And do the same for player 1.
            if (player1Strategy == OptimizationDirection::Minimize) {
                valid = (valid || model.getIllegalPlayer1Mask()).universalAbstract(model.getPlayer1Variables());
            } else {
                if (produceStrategiesInIteration) {
                    storm::dd::Bdd<Type> newValidDistributions = valid && !consideredPlayer1States.get();
                    player1StrategyBdd.get() = player1StrategyBdd.get() || newValidDistributions.existsAbstractRepresentative(model.getPlayer1Variables());
                }

                valid = valid.existsAbstract(model.getPlayer1Variables());

                if (produceStrategiesInIteration) {
                    consideredPlayer1States.get() |= valid;
                }
            }

            // Explicitly add psi states to result since they may have transitions going to some state that
            // does not have a reachability probability of 1.
            valid |= psiStates;

            // If no new states were added, we have found the current hypothesis for the states with
            // probability 1.
            if (valid == player1Solution) {
                solutionStatesDone = true;
            } else {
                player1Solution = valid;
            }
        }

        // If the states with probability 1 and the potential probability 1 states coincide, we have found
        // the solution.
        if (player1Solution == maybePlayer1States) {
            maybePlayer2States = player2Solution;
            maybeStatesDone = true;

            // If we were asked to produce strategies, we propagate that by triggering another iteration.
            // We only do this if at least one strategy will be produced.
            produceStrategiesInIteration = !produceStrategiesInIteration && ((producePlayer1Strategy && player1Strategy == OptimizationDirection::Maximize) ||
                                                                             (producePlayer2Strategy && player2Strategy == OptimizationDirection::Maximize));
        } else {
            // Otherwise, we use the current hypothesis for the states with probability 1 as the new maybe
            // state set.
            maybePlayer1States = player1Solution;
        }
    }

    // From now on, the solution is stored in maybeStates (as it coincides with the previous solution).

    // If we were asked to produce strategies that do not need to pick a certain successor but are
    // 'arbitrary', do so now.
    bool strategiesToCompute = (producePlayer1Strategy && !player1StrategyBdd) || (producePlayer2Strategy && !player2StrategyBdd);
    if (strategiesToCompute) {
        storm::dd::Bdd<Type> relevantStates = (transitionMatrix && maybePlayer2States).existsAbstract(model.getColumnVariables());
        if (producePlayer2Strategy && !player2StrategyBdd) {
            player2StrategyBdd = relevantStates.existsAbstractRepresentative(model.getPlayer2Variables());
        }
        if (producePlayer1Strategy && !player1StrategyBdd) {
            relevantStates = (maybePlayer1States && relevantStates).existsAbstract(model.getPlayer2Variables());
            player1StrategyBdd = relevantStates.existsAbstractRepresentative(model.getPlayer1Variables());
        }
    }

    return SymbolicGameProb01Result<Type>(maybePlayer1States, maybePlayer2States, player1StrategyBdd, player2StrategyBdd);
}

template<typename T>
void topologicalSortHelper(storm::storage::SparseMatrix<T> const& matrix, uint64_t state, std::vector<uint_fast64_t>& topologicalSort,
                           std::vector<uint_fast64_t>& recursionStack,
                           std::vector<typename storm::storage::SparseMatrix<T>::const_iterator>& iteratorRecursionStack,
                           storm::storage::BitVector& visitedStates) {
    if (!visitedStates.get(state)) {
        recursionStack.push_back(state);
        iteratorRecursionStack.push_back(matrix.begin(state));

    recursionStepForward:
        while (!recursionStack.empty()) {
            uint_fast64_t currentState = recursionStack.back();
            typename storm::storage::SparseMatrix<T>::const_iterator successorIterator = iteratorRecursionStack.back();

            visitedStates.set(currentState, true);

        recursionStepBackward:
            for (; successorIterator != matrix.end(currentState); ++successorIterator) {
                if (!visitedStates.get(successorIterator->getColumn())) {
                    // Put unvisited successor on top of our recursion stack and remember that.
                    recursionStack.push_back(successorIterator->getColumn());

                    // Also, put initial value for iterator on corresponding recursion stack.
                    iteratorRecursionStack.push_back(matrix.begin(successorIterator->getColumn()));

                    goto recursionStepForward;
                }
            }

            topologicalSort.push_back(currentState);

            // If we reach this point, we have completed the recursive descent for the current state.
            // That is, we need to pop it from the recursion stacks.
            recursionStack.pop_back();
            iteratorRecursionStack.pop_back();

            // If there is at least one state under the current one in our recursion stack, we need
            // to restore the topmost state as the current state and jump to the part after the
            // original recursive call.
            if (recursionStack.size() > 0) {
                currentState = recursionStack.back();
                successorIterator = iteratorRecursionStack.back();

                goto recursionStepBackward;
            }
        }
    }
}

template<typename T>
std::vector<uint_fast64_t> getBFSSort(storm::storage::SparseMatrix<T> const& matrix, std::vector<uint_fast64_t> const& firstStates) {
    storm::storage::BitVector seenStates(matrix.getRowGroupCount());

    std::vector<uint_fast64_t> stateQueue;
    stateQueue.reserve(matrix.getRowGroupCount());
    std::vector<uint_fast64_t> result;
    result.reserve(matrix.getRowGroupCount());

    //                storm::storage::sparse::state_type currentPosition = 0;
    auto count = matrix.getRowGroupCount() - 1;
    for (auto const& state : firstStates) {
        stateQueue.push_back(state);
        result[count] = state;
        count--;
    }

    // Perform a BFS.
    while (!stateQueue.empty()) {
        auto state = stateQueue.back();
        stateQueue.pop_back();
        seenStates.set(state);
        for (auto const& successorEntry : matrix.getRowGroup(state)) {
            auto succ = successorEntry.geColumn();
            if (!seenStates[succ]) {
                result[count] = succ;
                count--;
                stateQueue.insert(stateQueue.begin(), succ);
            }
        }
    }
    return result;
}

template<typename T>
std::vector<uint_fast64_t> getTopologicalSort(storm::storage::SparseMatrix<T> const& matrix, std::vector<uint64_t> const& firstStates) {
    if (matrix.getRowCount() != matrix.getColumnCount()) {
        STORM_LOG_ERROR("Provided matrix is required to be square.");
        throw storm::exceptions::InvalidArgumentException() << "Provided matrix is required to be square.";
    }

    uint_fast64_t numberOfStates = matrix.getRowCount();

    // Prepare the result. This relies on the matrix being square.
    std::vector<uint_fast64_t> topologicalSort;
    topologicalSort.reserve(numberOfStates);

    // Prepare the stacks needed for recursion.
    std::vector<uint_fast64_t> recursionStack;
    recursionStack.reserve(matrix.getRowCount());
    std::vector<typename storm::storage::SparseMatrix<T>::const_iterator> iteratorRecursionStack;
    iteratorRecursionStack.reserve(numberOfStates);

    // Perform a depth-first search over the given transitions and record states in the reverse order they were visited.
    storm::storage::BitVector visitedStates(numberOfStates);
    for (auto const state : firstStates) {
        topologicalSortHelper<T>(matrix, state, topologicalSort, recursionStack, iteratorRecursionStack, visitedStates);
    }
    for (uint_fast64_t state = 0; state < numberOfStates; ++state) {
        topologicalSortHelper<T>(matrix, state, topologicalSort, recursionStack, iteratorRecursionStack, visitedStates);
    }

    return topologicalSort;
}

template storm::storage::BitVector getReachableStates(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                      storm::storage::BitVector const& initialStates, storm::storage::BitVector const& constraintStates,
                                                      storm::storage::BitVector const& targetStates, bool useStepBound, uint_fast64_t maximalSteps,
                                                      boost::optional<storm::storage::BitVector> const& choiceFilter);

template storm::storage::BitVector getBsccCover(storm::storage::SparseMatrix<double> const& transitionMatrix);

template bool hasCycle(storm::storage::SparseMatrix<double> const& transitionMatrix, boost::optional<storm::storage::BitVector> const& subsystem);

template bool checkIfECWithChoiceExists(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                        storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& subsystem,
                                        storm::storage::BitVector const& choices);

template std::vector<uint_fast64_t> getDistances(storm::storage::SparseMatrix<double> const& transitionMatrix, storm::storage::BitVector const& initialStates,
                                                 boost::optional<storm::storage::BitVector> const& subsystem);

template storm::storage::BitVector performProbGreater0(storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                       storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                       bool useStepBound = false, uint_fast64_t maximalSteps = 0);

template storm::storage::BitVector performProb1(storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                                storm::storage::BitVector const& psiStates, storm::storage::BitVector const& statesWithProbabilityGreater0);

template storm::storage::BitVector performProb1(storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                                storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(storm::models::sparse::DeterministicModel<double> const& model,
                                                                                       storm::storage::BitVector const& phiStates,
                                                                                       storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                                                       storm::storage::BitVector const& phiStates,
                                                                                       storm::storage::BitVector const& psiStates);

template void computeSchedulerProbGreater0E(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                            storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                            storm::storage::BitVector const& psiStates, storm::storage::Scheduler<double>& scheduler,
                                            boost::optional<storm::storage::BitVector> const& rowFilter);

template void computeSchedulerProb0E(storm::storage::BitVector const& prob0EStates, storm::storage::SparseMatrix<double> const& transitionMatrix,
                                     storm::storage::Scheduler<double>& scheduler);

template void computeSchedulerRewInf(storm::storage::BitVector const& rewInfStates, storm::storage::SparseMatrix<double> const& transitionMatrix,
                                     storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::Scheduler<double>& scheduler);

template void computeSchedulerProb1E(storm::storage::BitVector const& prob1EStates, storm::storage::SparseMatrix<double> const& transitionMatrix,
                                     storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                     storm::storage::BitVector const& psiStates, storm::storage::Scheduler<double>& scheduler,
                                     boost::optional<storm::storage::BitVector> const& rowFilter = boost::none);

template storm::storage::BitVector performProbGreater0E(storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                        storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                        bool useStepBound = false, uint_fast64_t maximalSteps = 0);

template storm::storage::BitVector performProb0A(storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                                 storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProb1E(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                                 storm::storage::BitVector const& psiStates,
                                                 boost::optional<storm::storage::BitVector> const& choiceConstraint = boost::none);

template storm::storage::BitVector performProb1E(
    storm::models::sparse::NondeterministicModel<double, storm::models::sparse::StandardRewardModel<double>> const& model,
    storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                                          std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                                                          storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                                                          storm::storage::BitVector const& phiStates,
                                                                                          storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(
    storm::models::sparse::NondeterministicModel<double, storm::models::sparse::StandardRewardModel<double>> const& model,
    storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProbGreater0A(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                        storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                        storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                        bool useStepBound = false, uint_fast64_t maximalSteps = 0,
                                                        boost::optional<storm::storage::BitVector> const& choiceConstraint = boost::none);

template storm::storage::BitVector performProb0E(
    storm::models::sparse::NondeterministicModel<double, storm::models::sparse::StandardRewardModel<double>> const& model,
    storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);
template storm::storage::BitVector performProb0E(
    storm::models::sparse::NondeterministicModel<double, storm::models::sparse::StandardRewardModel<storm::Interval>> const& model,
    storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);
template storm::storage::BitVector performProb0E(
    storm::models::sparse::NondeterministicModel<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>> const& model,
    storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);
template storm::storage::BitVector performProb0E(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                                 storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProb1A(
    storm::models::sparse::NondeterministicModel<double, storm::models::sparse::StandardRewardModel<double>> const& model,
    storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);
template storm::storage::BitVector performProb1A(
    storm::models::sparse::NondeterministicModel<double, storm::models::sparse::StandardRewardModel<storm::Interval>> const& model,
    storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);
template storm::storage::BitVector performProb1A(
    storm::models::sparse::NondeterministicModel<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>> const& model,
    storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);
template storm::storage::BitVector performProb1A(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                                 storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(storm::storage::SparseMatrix<double> const& transitionMatrix,
                                                                                          std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                                                          storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                                                          storm::storage::BitVector const& phiStates,
                                                                                          storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(
    storm::models::sparse::NondeterministicModel<double, storm::models::sparse::StandardRewardModel<double>> const& model,
    storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);
template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(
    storm::models::sparse::NondeterministicModel<double, storm::models::sparse::StandardRewardModel<storm::Interval>> const& model,
    storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template ExplicitGameProb01Result performProb0(storm::storage::SparseMatrix<double> const& transitionMatrix, std::vector<uint64_t> const& player1RowGrouping,
                                               storm::storage::SparseMatrix<double> const& player1BackwardTransitions,
                                               std::vector<uint64_t> const& player2BackwardTransitions, storm::storage::BitVector const& phiStates,
                                               storm::storage::BitVector const& psiStates, storm::OptimizationDirection const& player1Direction,
                                               storm::OptimizationDirection const& player2Direction, storm::storage::ExplicitGameStrategyPair* strategyPair);

template ExplicitGameProb01Result performProb1(storm::storage::SparseMatrix<double> const& transitionMatrix, std::vector<uint64_t> const& player1RowGrouping,
                                               storm::storage::SparseMatrix<double> const& player1BackwardTransitions,
                                               std::vector<uint64_t> const& player2BackwardTransitions, storm::storage::BitVector const& phiStates,
                                               storm::storage::BitVector const& psiStates, storm::OptimizationDirection const& player1Direction,
                                               storm::OptimizationDirection const& player2Direction, storm::storage::ExplicitGameStrategyPair* strategyPair,
                                               boost::optional<storm::storage::BitVector> const& player1Candidates);

template std::vector<uint_fast64_t> getTopologicalSort(storm::storage::SparseMatrix<double> const& matrix, std::vector<uint64_t> const& firstStates);

// Instantiations for storm::RationalNumber.
template storm::storage::BitVector getReachableStates(storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                                      storm::storage::BitVector const& initialStates, storm::storage::BitVector const& constraintStates,
                                                      storm::storage::BitVector const& targetStates, bool useStepBound, uint_fast64_t maximalSteps,
                                                      boost::optional<storm::storage::BitVector> const& choiceFilter);

template storm::storage::BitVector getBsccCover(storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix);

template bool hasCycle(storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                       boost::optional<storm::storage::BitVector> const& subsystem);

template bool checkIfECWithChoiceExists(storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                        storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                        storm::storage::BitVector const& subsystem, storm::storage::BitVector const& choices);

template std::vector<uint_fast64_t> getDistances(storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                                 storm::storage::BitVector const& initialStates, boost::optional<storm::storage::BitVector> const& subsystem);

template storm::storage::BitVector performProbGreater0(storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                                       storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                       bool useStepBound = false, uint_fast64_t maximalSteps = 0);

template storm::storage::BitVector performProb1(storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                                storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                storm::storage::BitVector const& statesWithProbabilityGreater0);

template storm::storage::BitVector performProb1(storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                                storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(
    storm::models::sparse::DeterministicModel<storm::RationalNumber> const& model, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(
    storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template void computeSchedulerProbGreater0E(storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                            storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                            storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                            storm::storage::Scheduler<storm::RationalNumber>& scheduler,
                                            boost::optional<storm::storage::BitVector> const& rowFilter);

template void computeSchedulerProb0E(storm::storage::BitVector const& prob0EStates, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                     storm::storage::Scheduler<storm::RationalNumber>& scheduler);

template void computeSchedulerRewInf(storm::storage::BitVector const& rewInfStates, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                     storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                     storm::storage::Scheduler<storm::RationalNumber>& scheduler);

template void computeSchedulerProb1E(storm::storage::BitVector const& prob1EStates, storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                     storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                     storm::storage::BitVector const& psiStates, storm::storage::Scheduler<storm::RationalNumber>& scheduler,
                                     boost::optional<storm::storage::BitVector> const& rowFilter = boost::none);

template storm::storage::BitVector performProbGreater0E(storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                                        storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                        bool useStepBound = false, uint_fast64_t maximalSteps = 0);

template storm::storage::BitVector performProb0A(storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProb1E(storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                 boost::optional<storm::storage::BitVector> const& choiceConstraint = boost::none);

template storm::storage::BitVector performProb1E(storm::models::sparse::NondeterministicModel<storm::RationalNumber> const& model,
                                                 storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
    storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(
    storm::models::sparse::NondeterministicModel<storm::RationalNumber> const& model, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProbGreater0A(storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                                        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                        storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                                        storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                        bool useStepBound = false, uint_fast64_t maximalSteps = 0,
                                                        boost::optional<storm::storage::BitVector> const& choiceConstraint = boost::none);

template storm::storage::BitVector performProb0E(storm::models::sparse::NondeterministicModel<storm::RationalNumber> const& model,
                                                 storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProb0E(storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProb1A(storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix, std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
    storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(
    storm::models::sparse::NondeterministicModel<storm::RationalNumber> const& model, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template ExplicitGameProb01Result performProb0(storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                               std::vector<uint64_t> const& player1RowGrouping,
                                               storm::storage::SparseMatrix<storm::RationalNumber> const& player1BackwardTransitions,
                                               std::vector<uint64_t> const& player2BackwardTransitions, storm::storage::BitVector const& phiStates,
                                               storm::storage::BitVector const& psiStates, storm::OptimizationDirection const& player1Direction,
                                               storm::OptimizationDirection const& player2Direction, storm::storage::ExplicitGameStrategyPair* strategyPair);

template ExplicitGameProb01Result performProb1(storm::storage::SparseMatrix<storm::RationalNumber> const& transitionMatrix,
                                               std::vector<uint64_t> const& player1RowGrouping,
                                               storm::storage::SparseMatrix<storm::RationalNumber> const& player1BackwardTransitions,
                                               std::vector<uint64_t> const& player2BackwardTransitions, storm::storage::BitVector const& phiStates,
                                               storm::storage::BitVector const& psiStates, storm::OptimizationDirection const& player1Direction,
                                               storm::OptimizationDirection const& player2Direction, storm::storage::ExplicitGameStrategyPair* strategyPair,
                                               boost::optional<storm::storage::BitVector> const& player1Candidates);

template std::vector<uint_fast64_t> getTopologicalSort(storm::storage::SparseMatrix<storm::RationalNumber> const& matrix,
                                                       std::vector<uint64_t> const& firstStates);
// End of instantiations for storm::RationalNumber.
// Instantiations for storm::interval

template storm::storage::BitVector getReachableOneStep(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                                       storm::storage::BitVector const& initialStates);

template storm::storage::BitVector getReachableStates(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                                      storm::storage::BitVector const& initialStates, storm::storage::BitVector const& constraintStates,
                                                      storm::storage::BitVector const& targetStates, bool useStepBound, uint_fast64_t maximalSteps,
                                                      boost::optional<storm::storage::BitVector> const& choiceFilter);

template storm::storage::BitVector getBsccCover(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix);

template bool hasCycle(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix, boost::optional<storm::storage::BitVector> const& subsystem);

template bool checkIfECWithChoiceExists(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                        storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions, storm::storage::BitVector const& subsystem,
                                        storm::storage::BitVector const& choices);

template std::vector<uint_fast64_t> getDistances(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                                 storm::storage::BitVector const& initialStates, boost::optional<storm::storage::BitVector> const& subsystem);

template storm::storage::BitVector performProbGreater0(storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                                       storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                       bool useStepBound = false, uint_fast64_t maximalSteps = 0);

template storm::storage::BitVector performProb1(storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                                storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                storm::storage::BitVector const& statesWithProbabilityGreater0);

template storm::storage::BitVector performProb1(storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                                storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(storm::models::sparse::DeterministicModel<storm::Interval> const& model,
                                                                                       storm::storage::BitVector const& phiStates,
                                                                                       storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                                                                       storm::storage::BitVector const& phiStates,
                                                                                       storm::storage::BitVector const& psiStates);

template void computeSchedulerProbGreater0E(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                            storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                            storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                            storm::storage::Scheduler<double>& scheduler, boost::optional<storm::storage::BitVector> const& rowFilter);

template void computeSchedulerProb0E(storm::storage::BitVector const& prob0EStates, storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                     storm::storage::Scheduler<double>& scheduler);

template void computeSchedulerRewInf(storm::storage::BitVector const& rewInfStates, storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                     storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions, storm::storage::Scheduler<double>& scheduler);

template void computeSchedulerProb1E(storm::storage::BitVector const& prob1EStates, storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                     storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                     storm::storage::BitVector const& psiStates, storm::storage::Scheduler<double>& scheduler,
                                     boost::optional<storm::storage::BitVector> const& rowFilter = boost::none);

template storm::storage::BitVector performProbGreater0E(storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                                        storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                        bool useStepBound = false, uint_fast64_t maximalSteps = 0);

template storm::storage::BitVector performProb0A(storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProb1E(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                 boost::optional<storm::storage::BitVector> const& choiceConstraint = boost::none);

template storm::storage::BitVector performProb1E(storm::models::sparse::NondeterministicModel<storm::Interval> const& model,
                                                 storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(
    storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix, std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
    storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(
    storm::models::sparse::NondeterministicModel<storm::Interval> const& model, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProbGreater0A(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                                        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                        storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                                        storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                        bool useStepBound = false, uint_fast64_t maximalSteps = 0,
                                                        boost::optional<storm::storage::BitVector> const& choiceConstraint = boost::none);

template storm::storage::BitVector performProb0E(storm::models::sparse::NondeterministicModel<storm::Interval> const& model,
                                                 storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProb0E(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProb1A(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(
    storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix, std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
    storm::storage::SparseMatrix<storm::Interval> const& backwardTransitions, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(
    storm::models::sparse::NondeterministicModel<storm::Interval> const& model, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template ExplicitGameProb01Result performProb0(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                               std::vector<uint64_t> const& player1RowGrouping,
                                               storm::storage::SparseMatrix<storm::Interval> const& player1BackwardTransitions,
                                               std::vector<uint64_t> const& player2BackwardTransitions, storm::storage::BitVector const& phiStates,
                                               storm::storage::BitVector const& psiStates, storm::OptimizationDirection const& player1Direction,
                                               storm::OptimizationDirection const& player2Direction, storm::storage::ExplicitGameStrategyPair* strategyPair);

template ExplicitGameProb01Result performProb1(storm::storage::SparseMatrix<storm::Interval> const& transitionMatrix,
                                               std::vector<uint64_t> const& player1RowGrouping,
                                               storm::storage::SparseMatrix<storm::Interval> const& player1BackwardTransitions,
                                               std::vector<uint64_t> const& player2BackwardTransitions, storm::storage::BitVector const& phiStates,
                                               storm::storage::BitVector const& psiStates, storm::OptimizationDirection const& player1Direction,
                                               storm::OptimizationDirection const& player2Direction, storm::storage::ExplicitGameStrategyPair* strategyPair,
                                               boost::optional<storm::storage::BitVector> const& player1Candidates);

template std::vector<uint_fast64_t> getTopologicalSort(storm::storage::SparseMatrix<storm::Interval> const& matrix, std::vector<uint64_t> const& firstStates);
// End storm::interval
// Instantiations for storm::Rationalinterval

template storm::storage::BitVector getReachableOneStep(storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                                       storm::storage::BitVector const& initialStates);

template storm::storage::BitVector getReachableStates(storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                                      storm::storage::BitVector const& initialStates, storm::storage::BitVector const& constraintStates,
                                                      storm::storage::BitVector const& targetStates, bool useStepBound, uint_fast64_t maximalSteps,
                                                      boost::optional<storm::storage::BitVector> const& choiceFilter);

template storm::storage::BitVector getBsccCover(storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix);

template bool hasCycle(storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                       boost::optional<storm::storage::BitVector> const& subsystem);

template bool checkIfECWithChoiceExists(storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                        storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                        storm::storage::BitVector const& subsystem, storm::storage::BitVector const& choices);

template std::vector<uint_fast64_t> getDistances(storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                                 storm::storage::BitVector const& initialStates, boost::optional<storm::storage::BitVector> const& subsystem);

template storm::storage::BitVector performProbGreater0(storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                                       storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                       bool useStepBound = false, uint_fast64_t maximalSteps = 0);

template storm::storage::BitVector performProb1(storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                                storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                storm::storage::BitVector const& statesWithProbabilityGreater0);

template storm::storage::BitVector performProb1(storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                                storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(
    storm::models::sparse::DeterministicModel<storm::RationalInterval> const& model, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(
    storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template void computeSchedulerProbGreater0E(storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                            storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                            storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                            storm::storage::Scheduler<storm::RationalNumber>& scheduler,
                                            boost::optional<storm::storage::BitVector> const& rowFilter);

template void computeSchedulerProb0E(storm::storage::BitVector const& prob0EStates,
                                     storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                     storm::storage::Scheduler<storm::RationalNumber>& scheduler);

template void computeSchedulerRewInf(storm::storage::BitVector const& rewInfStates,
                                     storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                     storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                     storm::storage::Scheduler<storm::RationalNumber>& scheduler);

template void computeSchedulerProb1E(storm::storage::BitVector const& prob1EStates,
                                     storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                     storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                     storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                     storm::storage::Scheduler<storm::RationalNumber>& scheduler,
                                     boost::optional<storm::storage::BitVector> const& rowFilter = boost::none);

template storm::storage::BitVector performProbGreater0E(storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                                        storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                        bool useStepBound = false, uint_fast64_t maximalSteps = 0);

template storm::storage::BitVector performProb0A(storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProb1E(storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                 boost::optional<storm::storage::BitVector> const& choiceConstraint = boost::none);

template storm::storage::BitVector performProb1E(storm::models::sparse::NondeterministicModel<storm::RationalInterval> const& model,
                                                 storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(
    storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix, std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
    storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(
    storm::models::sparse::NondeterministicModel<storm::RationalInterval> const& model, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProbGreater0A(storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                                        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                        storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                                        storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                        bool useStepBound = false, uint_fast64_t maximalSteps = 0,
                                                        boost::optional<storm::storage::BitVector> const& choiceConstraint = boost::none);

template storm::storage::BitVector performProb0E(storm::models::sparse::NondeterministicModel<storm::RationalInterval> const& model,
                                                 storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProb0E(storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProb1A(storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(
    storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix, std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
    storm::storage::SparseMatrix<storm::RationalInterval> const& backwardTransitions, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(
    storm::models::sparse::NondeterministicModel<storm::RationalInterval> const& model, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template ExplicitGameProb01Result performProb0(storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                               std::vector<uint64_t> const& player1RowGrouping,
                                               storm::storage::SparseMatrix<storm::RationalInterval> const& player1BackwardTransitions,
                                               std::vector<uint64_t> const& player2BackwardTransitions, storm::storage::BitVector const& phiStates,
                                               storm::storage::BitVector const& psiStates, storm::OptimizationDirection const& player1Direction,
                                               storm::OptimizationDirection const& player2Direction, storm::storage::ExplicitGameStrategyPair* strategyPair);

template ExplicitGameProb01Result performProb1(storm::storage::SparseMatrix<storm::RationalInterval> const& transitionMatrix,
                                               std::vector<uint64_t> const& player1RowGrouping,
                                               storm::storage::SparseMatrix<storm::RationalInterval> const& player1BackwardTransitions,
                                               std::vector<uint64_t> const& player2BackwardTransitions, storm::storage::BitVector const& phiStates,
                                               storm::storage::BitVector const& psiStates, storm::OptimizationDirection const& player1Direction,
                                               storm::OptimizationDirection const& player2Direction, storm::storage::ExplicitGameStrategyPair* strategyPair,
                                               boost::optional<storm::storage::BitVector> const& player1Candidates);

template std::vector<uint_fast64_t> getTopologicalSort(storm::storage::SparseMatrix<storm::RationalInterval> const& matrix,
                                                       std::vector<uint64_t> const& firstStates);
// End storm::Rationalinterval

template storm::storage::BitVector getReachableStates(storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix,
                                                      storm::storage::BitVector const& initialStates, storm::storage::BitVector const& constraintStates,
                                                      storm::storage::BitVector const& targetStates, bool useStepBound, uint_fast64_t maximalSteps,
                                                      boost::optional<storm::storage::BitVector> const& choiceFilter);

template storm::storage::BitVector getBsccCover(storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix);

template bool hasCycle(storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix,
                       boost::optional<storm::storage::BitVector> const& subsystem);

template bool checkIfECWithChoiceExists(storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix,
                                        storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                        storm::storage::BitVector const& subsystem, storm::storage::BitVector const& choices);

template std::vector<uint_fast64_t> getDistances(storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix,
                                                 storm::storage::BitVector const& initialStates, boost::optional<storm::storage::BitVector> const& subsystem);

template storm::storage::BitVector performProbGreater0(storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                                       storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                       bool useStepBound = false, uint_fast64_t maximalSteps = 0);

template storm::storage::BitVector performProb1(storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                                storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                storm::storage::BitVector const& statesWithProbabilityGreater0);

template storm::storage::BitVector performProb1(storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                                storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(
    storm::models::sparse::DeterministicModel<storm::RationalFunction> const& model, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01(
    storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template void computeSchedulerProb1E(storm::storage::BitVector const& prob1EStates,
                                     storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix,
                                     storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                     storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                     storm::storage::Scheduler<storm::RationalFunction>& scheduler,
                                     boost::optional<storm::storage::BitVector> const& rowFilter = boost::none);

template storm::storage::BitVector performProbGreater0E(storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                                        storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                        bool useStepBound = false, uint_fast64_t maximalSteps = 0);

template storm::storage::BitVector performProb0A(storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProb1E(storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                 boost::optional<storm::storage::BitVector> const& choiceConstraint = boost::none);

template storm::storage::BitVector performProb1E(storm::models::sparse::NondeterministicModel<storm::RationalFunction> const& model,
                                                 storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(
    storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix, std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
    storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Max(
    storm::models::sparse::NondeterministicModel<storm::RationalFunction> const& model, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProbGreater0A(storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix,
                                                        std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                        storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                                        storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                        bool useStepBound = false, uint_fast64_t maximalSteps = 0,
                                                        boost::optional<storm::storage::BitVector> const& choiceConstraint = boost::none);

template storm::storage::BitVector performProb0E(storm::models::sparse::NondeterministicModel<storm::RationalFunction> const& model,
                                                 storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);
template storm::storage::BitVector performProb0E(storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template storm::storage::BitVector performProb1A(storm::models::sparse::NondeterministicModel<storm::RationalFunction> const& model,
                                                 storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);
template storm::storage::BitVector performProb1A(storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix,
                                                 std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                 storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions,
                                                 storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(
    storm::storage::SparseMatrix<storm::RationalFunction> const& transitionMatrix, std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
    storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template std::pair<storm::storage::BitVector, storm::storage::BitVector> performProb01Min(
    storm::models::sparse::NondeterministicModel<storm::RationalFunction> const& model, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template std::vector<uint_fast64_t> getTopologicalSort(storm::storage::SparseMatrix<storm::RationalFunction> const& matrix,
                                                       std::vector<uint64_t> const& firstStates);

// Instantiations for CUDD.

template storm::dd::Bdd<storm::dd::DdType::CUDD> performProbGreater0(storm::models::symbolic::Model<storm::dd::DdType::CUDD, double> const& model,
                                                                     storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix,
                                                                     storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
                                                                     storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates,
                                                                     boost::optional<uint_fast64_t> const& stepBound = boost::optional<uint_fast64_t>());

template storm::dd::Bdd<storm::dd::DdType::CUDD> performProb1(storm::models::symbolic::Model<storm::dd::DdType::CUDD, double> const& model,
                                                              storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix,
                                                              storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
                                                              storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates,
                                                              storm::dd::Bdd<storm::dd::DdType::CUDD> const& statesWithProbabilityGreater0);

template storm::dd::Bdd<storm::dd::DdType::CUDD> performProb1(storm::models::symbolic::Model<storm::dd::DdType::CUDD, double> const& model,
                                                              storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix,
                                                              storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
                                                              storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::CUDD>, storm::dd::Bdd<storm::dd::DdType::CUDD>> performProb01(
    storm::models::symbolic::DeterministicModel<storm::dd::DdType::CUDD, double> const& model, storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::CUDD>, storm::dd::Bdd<storm::dd::DdType::CUDD>> performProb01(
    storm::models::symbolic::Model<storm::dd::DdType::CUDD, double> const& model, storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates, storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::CUDD> computeSchedulerProbGreater0E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::CUDD, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::CUDD> performProbGreater0E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::CUDD, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::CUDD> performProb0A(storm::models::symbolic::NondeterministicModel<storm::dd::DdType::CUDD, double> const& model,
                                                               storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix,
                                                               storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
                                                               storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::CUDD> performProbGreater0A(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::CUDD, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::CUDD> performProb0E(storm::models::symbolic::NondeterministicModel<storm::dd::DdType::CUDD, double> const& model,
                                                               storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix,
                                                               storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
                                                               storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::CUDD> performProb1A(storm::models::symbolic::NondeterministicModel<storm::dd::DdType::CUDD, double> const& model,
                                                               storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix,
                                                               storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates,
                                                               storm::dd::Bdd<storm::dd::DdType::CUDD> const& statesWithProbabilityGreater0A);

template storm::dd::Bdd<storm::dd::DdType::CUDD> performProb1E(storm::models::symbolic::NondeterministicModel<storm::dd::DdType::CUDD, double> const& model,
                                                               storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix,
                                                               storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
                                                               storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates,
                                                               storm::dd::Bdd<storm::dd::DdType::CUDD> const& statesWithProbabilityGreater0E);

template storm::dd::Bdd<storm::dd::DdType::CUDD> computeSchedulerProb1E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::CUDD, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates, storm::dd::Bdd<storm::dd::DdType::CUDD> const& statesWithProbability1E);

template std::pair<storm::dd::Bdd<storm::dd::DdType::CUDD>, storm::dd::Bdd<storm::dd::DdType::CUDD>> performProb01Max(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::CUDD, double> const& model, storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::CUDD>, storm::dd::Bdd<storm::dd::DdType::CUDD>> performProb01Max(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::CUDD, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::CUDD>, storm::dd::Bdd<storm::dd::DdType::CUDD>> performProb01Min(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::CUDD, double> const& model, storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::CUDD>, storm::dd::Bdd<storm::dd::DdType::CUDD>> performProb01Min(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::CUDD, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates);

template SymbolicGameProb01Result<storm::dd::DdType::CUDD> performProb0(
    storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::CUDD, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates, storm::OptimizationDirection const& player1Strategy,
    storm::OptimizationDirection const& player2Strategy, bool producePlayer1Strategy, bool producePlayer2Strategy);

template SymbolicGameProb01Result<storm::dd::DdType::CUDD> performProb1(
    storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::CUDD, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates, storm::OptimizationDirection const& player1Strategy,
    storm::OptimizationDirection const& player2Strategy, bool producePlayer1Strategy, bool producePlayer2Strategy,
    boost::optional<storm::dd::Bdd<storm::dd::DdType::CUDD>> const& player1Candidates);

// Instantiations for Sylvan (double).

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProbGreater0(storm::models::symbolic::Model<storm::dd::DdType::Sylvan, double> const& model,
                                                                       storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
                                                                       storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
                                                                       storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates,
                                                                       boost::optional<uint_fast64_t> const& stepBound = boost::optional<uint_fast64_t>());

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb1(storm::models::symbolic::Model<storm::dd::DdType::Sylvan, double> const& model,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& statesWithProbabilityGreater0);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb1(storm::models::symbolic::Model<storm::dd::DdType::Sylvan, double> const& model,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01(
    storm::models::symbolic::DeterministicModel<storm::dd::DdType::Sylvan, double> const& model, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01(
    storm::models::symbolic::Model<storm::dd::DdType::Sylvan, double> const& model, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> computeSchedulerProbGreater0E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProbGreater0E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb0A(storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, double> const& model,
                                                                 storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
                                                                 storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
                                                                 storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProbGreater0A(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb0E(storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, double> const& model,
                                                                 storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
                                                                 storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
                                                                 storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb1A(storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, double> const& model,
                                                                 storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
                                                                 storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates,
                                                                 storm::dd::Bdd<storm::dd::DdType::Sylvan> const& statesWithProbabilityGreater0A);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb1E(storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, double> const& model,
                                                                 storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
                                                                 storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
                                                                 storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates,
                                                                 storm::dd::Bdd<storm::dd::DdType::Sylvan> const& statesWithProbabilityGreater0E);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> computeSchedulerProb1E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& statesWithProbability1E);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01Max(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, double> const& model, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01Max(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01Min(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, double> const& model, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01Min(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, double> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template SymbolicGameProb01Result<storm::dd::DdType::Sylvan> performProb0(
    storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::Sylvan> const& model, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates,
    storm::OptimizationDirection const& player1Strategy, storm::OptimizationDirection const& player2Strategy, bool producePlayer1Strategy,
    bool producePlayer2Strategy);

template SymbolicGameProb01Result<storm::dd::DdType::Sylvan> performProb1(
    storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::Sylvan> const& model, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates,
    storm::OptimizationDirection const& player1Strategy, storm::OptimizationDirection const& player2Strategy, bool producePlayer1Strategy,
    bool producePlayer2Strategy, boost::optional<storm::dd::Bdd<storm::dd::DdType::Sylvan>> const& player1Candidates);

// Instantiations for Sylvan (rational number).

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProbGreater0(
    storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, boost::optional<uint_fast64_t> const& stepBound = boost::optional<uint_fast64_t>());

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb1(storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& statesWithProbabilityGreater0);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb1(storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01(
    storm::models::symbolic::DeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01(
    storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> computeSchedulerProbGreater0E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProbGreater0E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb0A(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProbGreater0A(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb0E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb1A(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& statesWithProbabilityGreater0A);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb1E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& statesWithProbabilityGreater0E);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> computeSchedulerProb1E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& statesWithProbability1E);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01Max(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01Max(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01Min(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01Min(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template SymbolicGameProb01Result<storm::dd::DdType::Sylvan> performProb0(
    storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, storm::OptimizationDirection const& player1Strategy,
    storm::OptimizationDirection const& player2Strategy, bool producePlayer1Strategy, bool producePlayer2Strategy);

template SymbolicGameProb01Result<storm::dd::DdType::Sylvan> performProb1(
    storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, storm::OptimizationDirection const& player1Strategy,
    storm::OptimizationDirection const& player2Strategy, bool producePlayer1Strategy, bool producePlayer2Strategy,
    boost::optional<storm::dd::Bdd<storm::dd::DdType::Sylvan>> const& player1Candidates);

// Instantiations for Sylvan (rational function).

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProbGreater0(
    storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, boost::optional<uint_fast64_t> const& stepBound = boost::optional<uint_fast64_t>());

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb1(storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& statesWithProbabilityGreater0);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb1(storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
                                                                storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01(
    storm::models::symbolic::DeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01(
    storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> computeSchedulerProbGreater0E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProbGreater0E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb0A(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProbGreater0A(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb0E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb1A(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& statesWithProbabilityGreater0A);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> performProb1E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& statesWithProbabilityGreater0E);

template storm::dd::Bdd<storm::dd::DdType::Sylvan> computeSchedulerProb1E(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& statesWithProbability1E);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01Max(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01Max(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01Min(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

template std::pair<storm::dd::Bdd<storm::dd::DdType::Sylvan>, storm::dd::Bdd<storm::dd::DdType::Sylvan>> performProb01Min(
    storm::models::symbolic::NondeterministicModel<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates);

}  // namespace graph
}  // namespace utility
}  // namespace storm
