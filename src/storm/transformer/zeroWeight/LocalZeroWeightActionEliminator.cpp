#include "storm/transformer/zeroWeight/LocalZeroWeightActionEliminator.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/transformer/zeroWeight/MixedWeightStateSplitter.h"
#include "storm/transformer/zeroWeight/ZeroWeightAnalysis.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::transformer {

template<typename ValueType>
typename LocalZeroWeightActionEliminator<ValueType>::Result LocalZeroWeightActionEliminator<ValueType>::eliminate(
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType> const& actionWeights, storm::storage::BitVector const& targetStates,
    storm::storage::BitVector const& initialStates) {
    uint64_t const invalidIndex = std::numeric_limits<uint64_t>::max();
    auto splitResult = MixedWeightStateSplitter<ValueType>::split(transitionMatrix, actionWeights, targetStates, initialStates);
    ZeroWeightAnalysis<ValueType>::analyzeComponentInterfaces(splitResult.transitionMatrix, splitResult.analysis);

    LocalZeroWeightActionEliminator<ValueType> eliminator(splitResult.transitionMatrix, splitResult.actionWeights, splitResult.targetStates,
                                                          splitResult.initialStates);
    for (auto const& component : splitResult.analysis.weakComponents) {
        for (uint64_t state : component.states) {
            eliminator.eliminateState(state);
        }
    }
    auto result = eliminator.build();

    std::vector<uint64_t> newToOldStateMapping;
    newToOldStateMapping.reserve(result.newToOldStateMapping.size());
    for (uint64_t splitState : result.newToOldStateMapping) {
        STORM_LOG_ASSERT(splitState < splitResult.newToOldStateMapping.size(), "Invalid split state mapping.");
        newToOldStateMapping.push_back(splitResult.newToOldStateMapping[splitState]);
    }

    std::vector<uint64_t> oldToNewStateMapping(transitionMatrix.getRowGroupCount(), invalidIndex);
    for (uint64_t oldState = 0; oldState < oldToNewStateMapping.size(); ++oldState) {
        uint64_t splitState = oldState;
        if (result.oldToNewStateMapping[splitState] == invalidIndex && splitResult.positiveShells[oldState] != invalidIndex) {
            splitState = splitResult.positiveShells[oldState];
        }
        oldToNewStateMapping[oldState] = result.oldToNewStateMapping[splitState];
    }

    std::vector<std::vector<uint64_t>> oldToNewRowMapping(transitionMatrix.getRowCount());
    for (uint64_t oldRow = 0; oldRow < oldToNewRowMapping.size(); ++oldRow) {
        uint64_t const splitRow = splitResult.oldToNewRowMapping[oldRow];
        STORM_LOG_ASSERT(splitRow < result.oldToNewRowMapping.size(), "Invalid split row mapping.");
        oldToNewRowMapping[oldRow] = std::move(result.oldToNewRowMapping[splitRow]);
    }
    for (auto& splitRow : result.newToOldRowMapping) {
        STORM_LOG_ASSERT(splitRow < splitResult.newToOldRowMapping.size(), "Invalid output row mapping.");
        splitRow = splitResult.newToOldRowMapping[splitRow];
    }
    for (auto& selections : result.choiceSelections) {
        for (auto& selection : selections) {
            STORM_LOG_ASSERT(selection.state < splitResult.newToOldStateMapping.size() && selection.row < splitResult.newToOldRowMapping.size(),
                             "Invalid choice selection mapping.");
            selection.state = splitResult.newToOldStateMapping[selection.state];
            selection.row = splitResult.newToOldRowMapping[selection.row];
        }
    }

    result.oldToNewStateMapping = std::move(oldToNewStateMapping);
    result.newToOldStateMapping = std::move(newToOldStateMapping);
    result.oldToNewRowMapping = std::move(oldToNewRowMapping);
    return result;
}

template<typename ValueType>
LocalZeroWeightActionEliminator<ValueType>::LocalZeroWeightActionEliminator(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                            std::vector<ValueType> const& actionWeights,
                                                                            storm::storage::BitVector const& targetStates,
                                                                            storm::storage::BitVector const& initialStates)
    : numberOfStates(transitionMatrix.getRowGroupCount()),
      numberOfInputRows(transitionMatrix.getRowCount()),
      targetStates(targetStates),
      initialStates(initialStates),
      eliminatedStates(numberOfStates, false),
      stateActions(numberOfStates),
      incomingActions(numberOfStates) {
    STORM_LOG_THROW(transitionMatrix.getColumnCount() == numberOfStates, storm::exceptions::InvalidArgumentException,
                    "Expected one transition matrix column per state.");
    STORM_LOG_THROW(actionWeights.size() == numberOfInputRows, storm::exceptions::InvalidArgumentException,
                    "Expected one action weight per transition matrix row.");
    STORM_LOG_THROW(targetStates.size() == numberOfStates && initialStates.size() == numberOfStates, storm::exceptions::InvalidArgumentException,
                    "Expected target and initial-state bits for every state.");

    for (auto const& weight : actionWeights) {
        STORM_LOG_THROW(storm::utility::isNonNegative(weight), storm::exceptions::InvalidArgumentException,
                        "Local zero-weight action elimination requires nonnegative action weights.");
    }

    auto const& rowGroupIndices = transitionMatrix.getRowGroupIndices();
    for (uint64_t state : initialStates) {
        if (targetStates.get(state)) {
            continue;
        }
        for (uint64_t row = rowGroupIndices[state]; row < rowGroupIndices[state + 1]; ++row) {
            STORM_LOG_THROW(!storm::utility::isZero(actionWeights[row]), storm::exceptions::InvalidArgumentException,
                            "Local zero-weight action elimination requires non-target initial states to have only positive-weight actions.");
        }
    }

    actions.reserve(numberOfInputRows);
    for (uint64_t state = 0; state < numberOfStates; ++state) {
        for (uint64_t row = rowGroupIndices[state]; row < rowGroupIndices[state + 1]; ++row) {
            WorkingAction action{true, state, {}, actionWeights[row], row, {}};
            action.transitions.reserve(transitionMatrix.getRow(row).getNumberOfEntries());
            for (auto const& entry : transitionMatrix.getRow(row)) {
                STORM_LOG_THROW(storm::utility::isNonNegative(entry.getValue()), storm::exceptions::InvalidArgumentException,
                                "Local zero-weight action elimination requires nonnegative transition probabilities.");
                if (storm::utility::isPositive(entry.getValue())) {
                    action.transitions.emplace_back(entry.getColumn(), entry.getValue());
                }
            }
            addAction(std::move(action));
        }
    }
}

template<typename ValueType>
uint64_t LocalZeroWeightActionEliminator<ValueType>::addAction(WorkingAction&& action) {
    uint64_t const actionId = actions.size();
    uint64_t const sourceState = action.sourceState;
    for (auto const& [successor, probability] : action.transitions) {
        if (storm::utility::isPositive(probability)) {
            incomingActions[successor].push_back(actionId);
        }
    }
    actions.push_back(std::move(action));
    stateActions[sourceState].push_back(actionId);
    return actionId;
}

template<typename ValueType>
bool LocalZeroWeightActionEliminator<ValueType>::mergeChoiceSelections(std::vector<ChoiceSelection> const& first, std::vector<ChoiceSelection> const& second,
                                                                       std::vector<ChoiceSelection>& result) {
    result.clear();
    result.reserve(first.size() + second.size());
    auto firstIt = first.begin();
    auto secondIt = second.begin();
    while (firstIt != first.end() && secondIt != second.end()) {
        if (firstIt->state < secondIt->state) {
            result.push_back(*firstIt);
            ++firstIt;
        } else if (secondIt->state < firstIt->state) {
            result.push_back(*secondIt);
            ++secondIt;
        } else {
            if (firstIt->row != secondIt->row) {
                return false;
            }
            result.push_back(*firstIt);
            ++firstIt;
            ++secondIt;
        }
    }
    result.insert(result.end(), firstIt, first.end());
    result.insert(result.end(), secondIt, second.end());
    return true;
}

template<typename ValueType>
std::vector<typename LocalZeroWeightActionEliminator<ValueType>::Transition> LocalZeroWeightActionEliminator<ValueType>::composeTransitions(
    std::vector<Transition> const& predecessorTransitions, uint64_t eliminatedState, ValueType const& probabilityToState,
    std::vector<Transition> const& normalizedTransitions) {
    std::vector<Transition> directTransitions;
    directTransitions.reserve(predecessorTransitions.size());
    for (auto const& transition : predecessorTransitions) {
        if (transition.first != eliminatedState) {
            directTransitions.push_back(transition);
        }
    }

    std::vector<Transition> result;
    result.reserve(directTransitions.size() + normalizedTransitions.size());
    auto directIt = directTransitions.begin();
    auto normalizedIt = normalizedTransitions.begin();
    while (directIt != directTransitions.end() && normalizedIt != normalizedTransitions.end()) {
        if (directIt->first < normalizedIt->first) {
            result.push_back(*directIt);
            ++directIt;
        } else if (normalizedIt->first < directIt->first) {
            ValueType const probability = storm::utility::simplify((ValueType)(probabilityToState * normalizedIt->second));
            if (storm::utility::isPositive(probability)) {
                result.emplace_back(normalizedIt->first, probability);
            }
            ++normalizedIt;
        } else {
            ValueType const probability = storm::utility::simplify((ValueType)(directIt->second + probabilityToState * normalizedIt->second));
            if (storm::utility::isPositive(probability)) {
                result.emplace_back(directIt->first, probability);
            }
            ++directIt;
            ++normalizedIt;
        }
    }
    result.insert(result.end(), directIt, directTransitions.end());
    for (; normalizedIt != normalizedTransitions.end(); ++normalizedIt) {
        ValueType const probability = storm::utility::simplify((ValueType)(probabilityToState * normalizedIt->second));
        if (storm::utility::isPositive(probability)) {
            result.emplace_back(normalizedIt->first, probability);
        }
    }
    return result;
}

template<typename ValueType>
void LocalZeroWeightActionEliminator<ValueType>::eliminateState(uint64_t state) {
    STORM_LOG_THROW(state < numberOfStates, storm::exceptions::InvalidArgumentException, "Cannot eliminate a state outside the transition matrix.");
    STORM_LOG_THROW(!eliminatedStates.get(state), storm::exceptions::InvalidArgumentException, "Cannot eliminate the same state twice.");
    STORM_LOG_THROW(!targetStates.get(state) && !initialStates.get(state), storm::exceptions::InvalidArgumentException,
                    "Target and initial states must not be eliminated.");

    std::vector<NormalizedChoice> normalizedChoices;
    normalizedChoices.reserve(stateActions[state].size());
    for (uint64_t actionId : stateActions[state]) {
        auto const& action = actions[actionId];
        if (!action.active) {
            continue;
        }
        STORM_LOG_THROW(storm::utility::isZero(action.weight), storm::exceptions::InvalidArgumentException,
                        "Split mixed states before eliminating zero-weight actions.");

        ValueType selfLoopProbability = storm::utility::zero<ValueType>();
        std::vector<Transition> exitTransitions;
        exitTransitions.reserve(action.transitions.size());
        for (auto const& transition : action.transitions) {
            if (transition.first == state) {
                selfLoopProbability = transition.second;
            } else {
                exitTransitions.push_back(transition);
            }
        }
        ValueType const exitProbability = storm::utility::simplify((ValueType)(storm::utility::one<ValueType>() - selfLoopProbability));
        STORM_LOG_THROW(storm::utility::isPositive(exitProbability), storm::exceptions::InvalidArgumentException,
                        "Every eliminated action must leave its state with positive probability.");
        for (auto& transition : exitTransitions) {
            transition.second = storm::utility::simplify((ValueType)(transition.second / exitProbability));
        }

        NormalizedChoice normalizedChoice{std::move(exitTransitions), {}};
        std::vector<ChoiceSelection> stateChoice{{state, action.inputRow}};
        bool const compatible = mergeChoiceSelections(action.choiceSelections, stateChoice, normalizedChoice.choiceSelections);
        STORM_LOG_ASSERT(compatible, "An active action must use one choice at its source state.");
        normalizedChoices.push_back(std::move(normalizedChoice));
    }
    STORM_LOG_THROW(!normalizedChoices.empty(), storm::exceptions::InvalidArgumentException, "Expected at least one active action at the eliminated state.");

    auto const predecessorActionIds = incomingActions[state];
    for (uint64_t predecessorActionId : predecessorActionIds) {
        if (!actions[predecessorActionId].active || actions[predecessorActionId].sourceState == state) {
            continue;
        }
        WorkingAction const predecessorAction = actions[predecessorActionId];
        auto const stateTransition = std::lower_bound(predecessorAction.transitions.begin(), predecessorAction.transitions.end(), state,
                                                      [](Transition const& transition, uint64_t successor) { return transition.first < successor; });
        if (stateTransition == predecessorAction.transitions.end() || stateTransition->first != state || !storm::utility::isPositive(stateTransition->second)) {
            continue;
        }

        actions[predecessorActionId].active = false;
        for (auto const& normalizedChoice : normalizedChoices) {
            std::vector<ChoiceSelection> choiceSelections;
            if (!mergeChoiceSelections(predecessorAction.choiceSelections, normalizedChoice.choiceSelections, choiceSelections)) {
                continue;
            }
            WorkingAction replacement{true,
                                      predecessorAction.sourceState,
                                      composeTransitions(predecessorAction.transitions, state, stateTransition->second, normalizedChoice.transitions),
                                      predecessorAction.weight,
                                      predecessorAction.inputRow,
                                      std::move(choiceSelections)};
            addAction(std::move(replacement));
        }
    }

    for (uint64_t actionId : stateActions[state]) {
        actions[actionId].active = false;
    }
    eliminatedStates.set(state);
}

template<typename ValueType>
typename LocalZeroWeightActionEliminator<ValueType>::Result LocalZeroWeightActionEliminator<ValueType>::build() const {
    uint64_t const invalidIndex = std::numeric_limits<uint64_t>::max();
    Result result;
    result.oldToNewStateMapping.assign(numberOfStates, invalidIndex);
    result.newToOldStateMapping.reserve(numberOfStates - eliminatedStates.getNumberOfSetBits());
    for (uint64_t state = 0; state < numberOfStates; ++state) {
        if (!eliminatedStates.get(state)) {
            result.oldToNewStateMapping[state] = result.newToOldStateMapping.size();
            result.newToOldStateMapping.push_back(state);
        }
    }

    uint64_t numberOfRows = 0;
    uint64_t numberOfEntries = 0;
    for (uint64_t state : result.newToOldStateMapping) {
        uint64_t numberOfStateActions = 0;
        for (uint64_t actionId : stateActions[state]) {
            if (actions[actionId].active) {
                ++numberOfStateActions;
                numberOfEntries += actions[actionId].transitions.size();
            }
        }
        STORM_LOG_THROW(numberOfStateActions > 0, storm::exceptions::InvalidArgumentException, "Elimination left a state without actions.");
        numberOfRows += numberOfStateActions;
    }

    uint64_t const newNumberOfStates = result.newToOldStateMapping.size();
    storm::storage::SparseMatrixBuilder<ValueType> builder(numberOfRows, newNumberOfStates, numberOfEntries, true, true, newNumberOfStates);
    result.actionWeights.reserve(numberOfRows);
    result.oldToNewRowMapping.resize(numberOfInputRows);
    result.newToOldRowMapping.reserve(numberOfRows);
    result.choiceSelections.reserve(numberOfRows);
    result.targetStates = storm::storage::BitVector(newNumberOfStates, false);
    result.initialStates = storm::storage::BitVector(newNumberOfStates, false);

    uint64_t newRow = 0;
    for (uint64_t oldState : result.newToOldStateMapping) {
        builder.newRowGroup(newRow);
        uint64_t const newState = result.oldToNewStateMapping[oldState];
        if (targetStates.get(oldState)) {
            result.targetStates.set(newState);
        }
        if (initialStates.get(oldState)) {
            result.initialStates.set(newState);
        }
        for (uint64_t actionId : stateActions[oldState]) {
            auto const& action = actions[actionId];
            if (!action.active) {
                continue;
            }
            for (auto const& [successor, probability] : action.transitions) {
                STORM_LOG_THROW(!eliminatedStates.get(successor), storm::exceptions::InvalidArgumentException,
                                "An active action still reaches an eliminated state.");
                builder.addNextValue(newRow, result.oldToNewStateMapping[successor], probability);
            }
            result.actionWeights.push_back(action.weight);
            result.oldToNewRowMapping[action.inputRow].push_back(newRow);
            result.newToOldRowMapping.push_back(action.inputRow);
            result.choiceSelections.push_back(action.choiceSelections);
            ++newRow;
        }
    }
    result.transitionMatrix = builder.build();
    return result;
}

template class LocalZeroWeightActionEliminator<double>;
template class LocalZeroWeightActionEliminator<storm::RationalNumber>;

}  // namespace storm::transformer
