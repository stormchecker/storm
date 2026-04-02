#include "storm/storage/memorystructure/SparseModelMemoryProduct.h"

#include <boost/optional.hpp>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/storage/memorystructure/MemoryStructureBuilder.h"
#include "storm/utility/builder.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {

template<typename ValueType, typename RewardModelType>
SparseModelMemoryProduct<ValueType, RewardModelType>::SparseModelMemoryProduct(storm::models::sparse::Model<ValueType, RewardModelType> const& sparseModel,
                                                                               storm::storage::MemoryStructure const& memoryStructure)
    : isInitialized(false), memoryStateCount(memoryStructure.getNumberOfStates()), model(sparseModel), memory(memoryStructure), scheduler(boost::none) {
    reachableStates = storm::storage::BitVector(model.getNumberOfStates() * memoryStateCount, false);
}

template<typename ValueType, typename RewardModelType>
SparseModelMemoryProduct<ValueType, RewardModelType>::SparseModelMemoryProduct(storm::models::sparse::Model<ValueType, RewardModelType> const& sparseModel,
                                                                               storm::storage::Scheduler<ValueType> const& scheduler)
    : isInitialized(false),
      memoryStateCount(scheduler.getNumberOfMemoryStates()),
      model(sparseModel),
      localMemory(scheduler.getMemoryStructure() ? boost::optional<MemoryStructure>()
                                                 : boost::optional<MemoryStructure>(
                                                       storm::storage::MemoryStructureBuilder<ValueType, RewardModelType>::buildTrivialMemoryStructure(model))),
      memory(scheduler.getMemoryStructure() ? scheduler.getMemoryStructure().get() : localMemory.get()),
      scheduler(scheduler) {
    reachableStates = storm::storage::BitVector(model.getNumberOfStates() * memoryStateCount, false);
}

template<typename ValueType, typename RewardModelType>
void SparseModelMemoryProduct<ValueType, RewardModelType>::initialize() {
    if (!isInitialized) {
        uint64_t modelStateCount = model.getNumberOfStates();

        computeMemorySuccessors();

        // Get the initial states and reachable states. A stateIndex s corresponds to the model state (s / memoryStateCount) and memory state (s %
        // memoryStateCount)
        storm::storage::BitVector initialStates(modelStateCount * memoryStateCount, false);
        auto memoryInitIt = memory.getInitialMemoryStates().begin();
        if (memory.isOnlyInitialStatesRelevantSet()) {
            for (auto modelInit : model.getInitialStates()) {
                initialStates.set(modelInit * memoryStateCount + *memoryInitIt, true);
                ++memoryInitIt;
            }
        } else {
            // Build Product from all model states
            for (uint_fast64_t modelState = 0; modelState < model.getNumberOfStates(); ++modelState) {
                initialStates.set(modelState * memoryStateCount + *memoryInitIt, true);
                ++memoryInitIt;
            }
        }
        STORM_LOG_ASSERT(memoryInitIt == memory.getInitialMemoryStates().end(), "Unexpected number of initial states.");

        computeReachableStates(initialStates);

        // Compute the mapping to the states of the result
        uint64_t reachableStateCount = 0;
        toResultStateMapping = std::vector<uint64_t>(model.getNumberOfStates() * memoryStateCount, std::numeric_limits<uint64_t>::max());
        for (auto reachableState : reachableStates) {
            toResultStateMapping[reachableState] = reachableStateCount;
            ++reachableStateCount;
        }

        isInitialized = true;
    }
}

template<typename ValueType, typename RewardModelType>
void SparseModelMemoryProduct<ValueType, RewardModelType>::addReachableState(uint64_t const& modelState, uint64_t const& memoryState) {
    isInitialized = false;
    reachableStates.set(modelState * memoryStateCount + memoryState, true);
}

template<typename ValueType, typename RewardModelType>
void SparseModelMemoryProduct<ValueType, RewardModelType>::setBuildFullProduct() {
    isInitialized = false;
    reachableStates.fill();
}

template<typename ValueType, typename RewardModelType>
std::shared_ptr<storm::models::sparse::Model<ValueType, RewardModelType>> SparseModelMemoryProduct<ValueType, RewardModelType>::build(bool preserveModelType) {
    initialize();

    // Build the model components
    storm::storage::SparseMatrix<ValueType> transitionMatrix;
    if (scheduler) {
        transitionMatrix = buildTransitionMatrixForScheduler();
    } else if (model.getTransitionMatrix().hasTrivialRowGrouping()) {
        transitionMatrix = buildDeterministicTransitionMatrix();
    } else {
        transitionMatrix = buildNondeterministicTransitionMatrix();
    }
    storm::models::sparse::StateLabeling labeling = buildStateLabeling(transitionMatrix);
    std::unordered_map<std::string, RewardModelType> rewardModels = buildRewardModels(transitionMatrix);

    return buildResult(std::move(transitionMatrix), std::move(labeling), std::move(rewardModels), preserveModelType);
}

template<typename ValueType, typename RewardModelType>
bool SparseModelMemoryProduct<ValueType, RewardModelType>::isStateReachable(uint64_t const& modelState, uint64_t const& memoryState) {
    STORM_LOG_ASSERT(modelState < getOriginalModel().getNumberOfStates(), "Invalid model state: " << modelState << ".");
    STORM_LOG_ASSERT(memoryState < memoryStateCount, "Invalid memory state: " << memoryState << ".");
    initialize();
    return reachableStates.get(modelState * memoryStateCount + memoryState);
}

template<typename ValueType, typename RewardModelType>
uint64_t const& SparseModelMemoryProduct<ValueType, RewardModelType>::getResultState(uint64_t const& modelState, uint64_t const& memoryState) {
    initialize();
    STORM_LOG_ASSERT(isStateReachable(modelState, memoryState), "Tried to get unreachable product state (" << modelState << "," << memoryState << ")");
    return toResultStateMapping[modelState * memoryStateCount + memoryState];
}

template<typename ValueType, typename RewardModelType>
void SparseModelMemoryProduct<ValueType, RewardModelType>::computeMemorySuccessors() {
    uint64_t modelTransitionCount = model.getTransitionMatrix().getEntryCount();
    memorySuccessors = std::vector<uint64_t>(modelTransitionCount * memoryStateCount, std::numeric_limits<uint64_t>::max());

    for (uint64_t memoryState = 0; memoryState < memoryStateCount; ++memoryState) {
        for (uint64_t transitionGoal = 0; transitionGoal < memoryStateCount; ++transitionGoal) {
            auto const& memoryTransition = memory.getTransitionMatrix()[memoryState][transitionGoal];
            if (memoryTransition) {
                for (auto modelTransitionIndex : memoryTransition.get()) {
                    memorySuccessors[modelTransitionIndex * memoryStateCount + memoryState] = transitionGoal;
                }
            }
        }
    }
}

template<typename ValueType, typename RewardModelType>
void SparseModelMemoryProduct<ValueType, RewardModelType>::computeReachableStates(storm::storage::BitVector const& initialStates) {
    // Explore the reachable states via DFS.
    // A state s on the stack corresponds to the model state (s / memoryStateCount) and memory state (s % memoryStateCount)
    reachableStates |= initialStates;
    if (!reachableStates.full()) {
        std::vector<uint64_t> stack(reachableStates.begin(), reachableStates.end());
        while (!stack.empty()) {
            uint64_t stateIndex = stack.back();
            stack.pop_back();
            uint64_t modelState = stateIndex / memoryStateCount;
            uint64_t memoryState = stateIndex % memoryStateCount;

            if (scheduler) {
                auto choices = scheduler->getChoice(modelState, memoryState).getChoiceAsDistribution();
                uint64_t groupStart = model.getTransitionMatrix().getRowGroupIndices()[modelState];
                for (auto const& choice : choices) {
                    STORM_LOG_ASSERT(groupStart + choice.first < model.getTransitionMatrix().getRowGroupIndices()[modelState + 1],
                                     "Invalid choice " << choice.first << " at model state " << modelState << ".");
                    auto const& row = model.getTransitionMatrix().getRow(groupStart + choice.first);
                    for (auto modelTransitionIt = row.begin(); modelTransitionIt != row.end(); ++modelTransitionIt) {
                        if (!storm::utility::isZero(modelTransitionIt->getValue())) {
                            uint64_t successorModelState = modelTransitionIt->getColumn();
                            uint64_t modelTransitionId = modelTransitionIt - model.getTransitionMatrix().begin();
                            uint64_t successorMemoryState = memorySuccessors[modelTransitionId * memoryStateCount + memoryState];
                            uint64_t successorStateIndex = successorModelState * memoryStateCount + successorMemoryState;
                            if (!reachableStates.get(successorStateIndex)) {
                                reachableStates.set(successorStateIndex, true);
                                stack.push_back(successorStateIndex);
                            }
                        }
                    }
                }
            } else {
                auto const& rowGroup = model.getTransitionMatrix().getRowGroup(modelState);
                for (auto modelTransitionIt = rowGroup.begin(); modelTransitionIt != rowGroup.end(); ++modelTransitionIt) {
                    if (!storm::utility::isZero(modelTransitionIt->getValue())) {
                        uint64_t successorModelState = modelTransitionIt->getColumn();
                        uint64_t modelTransitionId = modelTransitionIt - model.getTransitionMatrix().begin();
                        uint64_t successorMemoryState = memorySuccessors[modelTransitionId * memoryStateCount + memoryState];
                        uint64_t successorStateIndex = successorModelState * memoryStateCount + successorMemoryState;
                        if (!reachableStates.get(successorStateIndex)) {
                            reachableStates.set(successorStateIndex, true);
                            stack.push_back(successorStateIndex);
                        }
                    }
                }
            }
        }
    }
}

template<typename ValueType, typename RewardModelType>
storm::storage::SparseMatrix<ValueType> SparseModelMemoryProduct<ValueType, RewardModelType>::buildDeterministicTransitionMatrix() {
    uint64_t numResStates = reachableStates.getNumberOfSetBits();
    uint64_t numResTransitions = 0;
    for (auto stateIndex : reachableStates) {
        numResTransitions += model.getTransitionMatrix().getRow(stateIndex / memoryStateCount).getNumberOfEntries();
    }

    storm::storage::SparseMatrixBuilder<ValueType> builder(numResStates, numResStates, numResTransitions, true);
    uint64_t currentRow = 0;
    for (auto stateIndex : reachableStates) {
        uint64_t modelState = stateIndex / memoryStateCount;
        uint64_t memoryState = stateIndex % memoryStateCount;
        auto const& modelRow = model.getTransitionMatrix().getRow(modelState);
        for (auto entryIt = modelRow.begin(); entryIt != modelRow.end(); ++entryIt) {
            uint64_t transitionId = entryIt - model.getTransitionMatrix().begin();
            uint64_t successorMemoryState = memorySuccessors[transitionId * memoryStateCount + memoryState];
            builder.addNextValue(currentRow, getResultState(entryIt->getColumn(), successorMemoryState), entryIt->getValue());
        }
        ++currentRow;
    }

    return builder.build();
}

template<typename ValueType, typename RewardModelType>
storm::storage::SparseMatrix<ValueType> SparseModelMemoryProduct<ValueType, RewardModelType>::buildNondeterministicTransitionMatrix() {
    uint64_t numResStates = reachableStates.getNumberOfSetBits();
    uint64_t numResChoices = 0;
    uint64_t numResTransitions = 0;
    for (auto stateIndex : reachableStates) {
        uint64_t modelState = stateIndex / memoryStateCount;
        for (uint64_t modelRow = model.getTransitionMatrix().getRowGroupIndices()[modelState];
             modelRow < model.getTransitionMatrix().getRowGroupIndices()[modelState + 1]; ++modelRow) {
            ++numResChoices;
            numResTransitions += model.getTransitionMatrix().getRow(modelRow).getNumberOfEntries();
        }
    }

    storm::storage::SparseMatrixBuilder<ValueType> builder(numResChoices, numResStates, numResTransitions, true, true, numResStates);
    uint64_t currentRow = 0;
    for (auto stateIndex : reachableStates) {
        uint64_t modelState = stateIndex / memoryStateCount;
        uint64_t memoryState = stateIndex % memoryStateCount;
        builder.newRowGroup(currentRow);
        for (uint64_t modelRowIndex = model.getTransitionMatrix().getRowGroupIndices()[modelState];
             modelRowIndex < model.getTransitionMatrix().getRowGroupIndices()[modelState + 1]; ++modelRowIndex) {
            auto const& modelRow = model.getTransitionMatrix().getRow(modelRowIndex);
            for (auto entryIt = modelRow.begin(); entryIt != modelRow.end(); ++entryIt) {
                uint64_t transitionId = entryIt - model.getTransitionMatrix().begin();
                uint64_t successorMemoryState = memorySuccessors[transitionId * memoryStateCount + memoryState];
                builder.addNextValue(currentRow, getResultState(entryIt->getColumn(), successorMemoryState), entryIt->getValue());
            }
            ++currentRow;
        }
    }

    return builder.build();
}

template<typename ValueType, typename RewardModelType>
storm::storage::SparseMatrix<ValueType> SparseModelMemoryProduct<ValueType, RewardModelType>::buildTransitionMatrixForScheduler() {
    uint64_t numResStates = reachableStates.getNumberOfSetBits();
    uint64_t numResChoices = 0;
    uint64_t numResTransitions = 0;
    bool hasTrivialNondeterminism = true;
    for (auto stateIndex : reachableStates) {
        uint64_t modelState = stateIndex / memoryStateCount;
        uint64_t memoryState = stateIndex % memoryStateCount;
        storm::storage::SchedulerChoice<ValueType> choice = scheduler->getChoice(modelState, memoryState);
        if (choice.isDefined()) {
            ++numResChoices;
            if (choice.isDeterministic()) {
                uint64_t modelRow = model.getTransitionMatrix().getRowGroupIndices()[modelState] + choice.getDeterministicChoice();
                numResTransitions += model.getTransitionMatrix().getRow(modelRow).getNumberOfEntries();
            } else {
                std::set<uint64_t> successors;
                for (auto const& choiceIndex : choice.getChoiceAsDistribution()) {
                    if (!storm::utility::isZero(choiceIndex.second)) {
                        uint64_t modelRow = model.getTransitionMatrix().getRowGroupIndices()[modelState] + choiceIndex.first;
                        for (auto const& entry : model.getTransitionMatrix().getRow(modelRow)) {
                            successors.insert(entry.getColumn());
                        }
                    }
                }
                numResTransitions += successors.size();
            }
        } else {
            uint64_t modelRow = model.getTransitionMatrix().getRowGroupIndices()[modelState];
            uint64_t groupEnd = model.getTransitionMatrix().getRowGroupIndices()[modelState + 1];
            hasTrivialNondeterminism = hasTrivialNondeterminism && (groupEnd == modelRow + 1);
            for (; modelRow < groupEnd; ++modelRow) {
                ++numResChoices;
                numResTransitions += model.getTransitionMatrix().getRow(modelRow).getNumberOfEntries();
            }
        }
    }

    storm::storage::SparseMatrixBuilder<ValueType> builder(numResChoices, numResStates, numResTransitions, true, !hasTrivialNondeterminism,
                                                           hasTrivialNondeterminism ? 0 : numResStates);
    uint64_t currentRow = 0;
    for (auto stateIndex : reachableStates) {
        uint64_t modelState = stateIndex / memoryStateCount;
        uint64_t memoryState = stateIndex % memoryStateCount;
        if (!hasTrivialNondeterminism) {
            builder.newRowGroup(currentRow);
        }
        storm::storage::SchedulerChoice<ValueType> choice = scheduler->getChoice(modelState, memoryState);
        if (choice.isDefined()) {
            if (choice.isDeterministic()) {
                uint64_t modelRowIndex = model.getTransitionMatrix().getRowGroupIndices()[modelState] + choice.getDeterministicChoice();
                auto const& modelRow = model.getTransitionMatrix().getRow(modelRowIndex);
                for (auto entryIt = modelRow.begin(); entryIt != modelRow.end(); ++entryIt) {
                    uint64_t transitionId = entryIt - model.getTransitionMatrix().begin();
                    uint64_t successorMemoryState = memorySuccessors[transitionId * memoryStateCount + memoryState];
                    builder.addNextValue(currentRow, getResultState(entryIt->getColumn(), successorMemoryState), entryIt->getValue());
                }
            } else {
                std::map<uint64_t, ValueType> transitions;
                for (auto const& choiceIndex : choice.getChoiceAsDistribution()) {
                    if (!storm::utility::isZero(choiceIndex.second)) {
                        uint64_t modelRowIndex = model.getTransitionMatrix().getRowGroupIndices()[modelState] + choiceIndex.first;
                        auto const& modelRow = model.getTransitionMatrix().getRow(modelRowIndex);
                        for (auto entryIt = modelRow.begin(); entryIt != modelRow.end(); ++entryIt) {
                            uint64_t transitionId = entryIt - model.getTransitionMatrix().begin();
                            uint64_t successorMemoryState = memorySuccessors[transitionId * memoryStateCount + memoryState];
                            ValueType transitionValue = choiceIndex.second * entryIt->getValue();
                            auto insertionRes = transitions.insert(std::make_pair(getResultState(entryIt->getColumn(), successorMemoryState), transitionValue));
                            if (!insertionRes.second) {
                                insertionRes.first->second += transitionValue;
                            }
                        }
                    }
                }
                for (auto const& transition : transitions) {
                    builder.addNextValue(currentRow, transition.first, transition.second);
                }
            }
            ++currentRow;
        } else {
            for (uint64_t modelRowIndex = model.getTransitionMatrix().getRowGroupIndices()[modelState];
                 modelRowIndex < model.getTransitionMatrix().getRowGroupIndices()[modelState + 1]; ++modelRowIndex) {
                auto const& modelRow = model.getTransitionMatrix().getRow(modelRowIndex);
                for (auto entryIt = modelRow.begin(); entryIt != modelRow.end(); ++entryIt) {
                    uint64_t transitionId = entryIt - model.getTransitionMatrix().begin();
                    uint64_t successorMemoryState = memorySuccessors[transitionId * memoryStateCount + memoryState];
                    builder.addNextValue(currentRow, getResultState(entryIt->getColumn(), successorMemoryState), entryIt->getValue());
                }
                ++currentRow;
            }
        }
    }

    return builder.build();
}

template<typename ValueType, typename RewardModelType>
storm::models::sparse::StateLabeling SparseModelMemoryProduct<ValueType, RewardModelType>::buildStateLabeling(
    storm::storage::SparseMatrix<ValueType> const& resultTransitionMatrix) {
    uint64_t modelStateCount = model.getNumberOfStates();

    uint64_t numResStates = resultTransitionMatrix.getRowGroupCount();
    storm::models::sparse::StateLabeling resultLabeling(numResStates);

    for (std::string modelLabel : model.getStateLabeling().getLabels()) {
        if (modelLabel != "init") {
            storm::storage::BitVector resLabeledStates(numResStates, false);
            for (auto modelState : model.getStateLabeling().getStates(modelLabel)) {
                for (uint64_t memoryState = 0; memoryState < memoryStateCount; ++memoryState) {
                    if (isStateReachable(modelState, memoryState)) {
                        resLabeledStates.set(getResultState(modelState, memoryState), true);
                    }
                }
            }
            resultLabeling.addLabel(modelLabel, std::move(resLabeledStates));
        }
    }
    for (std::string memoryLabel : memory.getStateLabeling().getLabels()) {
        STORM_LOG_THROW(!resultLabeling.containsLabel(memoryLabel), storm::exceptions::InvalidOperationException,
                        "Failed to build the product of model and memory structure: State labelings are not disjoint as both structures contain the label "
                            << memoryLabel << ".");
        storm::storage::BitVector resLabeledStates(numResStates, false);
        for (auto memoryState : memory.getStateLabeling().getStates(memoryLabel)) {
            for (uint64_t modelState = 0; modelState < modelStateCount; ++modelState) {
                if (isStateReachable(modelState, memoryState)) {
                    resLabeledStates.set(getResultState(modelState, memoryState), true);
                }
            }
        }
        resultLabeling.addLabel(memoryLabel, std::move(resLabeledStates));
    }

    storm::storage::BitVector initialStates(numResStates, false);
    auto memoryInitIt = memory.getInitialMemoryStates().begin();
    for (auto modelInit : model.getInitialStates()) {
        initialStates.set(getResultState(modelInit, *memoryInitIt), true);
        ++memoryInitIt;
    }
    resultLabeling.addLabel("init", std::move(initialStates));

    return resultLabeling;
}

template<typename ValueType, typename RewardModelType>
std::unordered_map<std::string, RewardModelType> SparseModelMemoryProduct<ValueType, RewardModelType>::buildRewardModels(
    storm::storage::SparseMatrix<ValueType> const& resultTransitionMatrix) {
    typedef typename RewardModelType::ValueType RewardValueType;

    std::unordered_map<std::string, RewardModelType> result;
    uint64_t numResStates = resultTransitionMatrix.getRowGroupCount();

    for (auto const& rewardModel : model.getRewardModels()) {
        std::optional<std::vector<RewardValueType>> stateRewards;
        if (rewardModel.second.hasStateRewards()) {
            stateRewards = std::vector<RewardValueType>(numResStates, storm::utility::zero<RewardValueType>());
            uint64_t modelState = 0;
            for (auto const& modelStateReward : rewardModel.second.getStateRewardVector()) {
                if (!storm::utility::isZero(modelStateReward)) {
                    for (uint64_t memoryState = 0; memoryState < memoryStateCount; ++memoryState) {
                        if (isStateReachable(modelState, memoryState)) {
                            stateRewards.value()[getResultState(modelState, memoryState)] = modelStateReward;
                        }
                    }
                }
                ++modelState;
            }
        }
        std::optional<std::vector<RewardValueType>> stateActionRewards;
        if (rewardModel.second.hasStateActionRewards()) {
            stateActionRewards = std::vector<RewardValueType>(resultTransitionMatrix.getRowCount(), storm::utility::zero<RewardValueType>());
            uint64_t modelState = 0;
            uint64_t modelRow = 0;
            for (auto const& modelStateActionReward : rewardModel.second.getStateActionRewardVector()) {
                if (!storm::utility::isZero(modelStateActionReward)) {
                    while (modelRow >= model.getTransitionMatrix().getRowGroupIndices()[modelState + 1]) {
                        ++modelState;
                    }
                    uint64_t rowOffset = modelRow - model.getTransitionMatrix().getRowGroupIndices()[modelState];
                    for (uint64_t memoryState = 0; memoryState < memoryStateCount; ++memoryState) {
                        if (isStateReachable(modelState, memoryState)) {
                            if (scheduler && scheduler->getChoice(modelState, memoryState).isDefined()) {
                                ValueType factor = scheduler->getChoice(modelState, memoryState).getChoiceAsDistribution().getProbability(rowOffset);
                                stateActionRewards.value()[resultTransitionMatrix.getRowGroupIndices()[getResultState(modelState, memoryState)]] +=
                                    factor * modelStateActionReward;
                            } else {
                                stateActionRewards.value()[resultTransitionMatrix.getRowGroupIndices()[getResultState(modelState, memoryState)] + rowOffset] =
                                    modelStateActionReward;
                            }
                        }
                    }
                }
                ++modelRow;
            }
        }
        std::optional<storm::storage::SparseMatrix<RewardValueType>> transitionRewards;
        if (rewardModel.second.hasTransitionRewards()) {
            bool const useRowGrouping = !resultTransitionMatrix.hasTrivialRowGrouping();
            storm::storage::SparseMatrixBuilder<RewardValueType> builder(resultTransitionMatrix.getRowCount(), resultTransitionMatrix.getColumnCount(), 0ull,
                                                                         true, useRowGrouping,
                                                                         useRowGrouping ? resultTransitionMatrix.getRowGroupCount() : 0ull);
            uint64_t stateIndex = 0;
            for (auto const& resState : toResultStateMapping) {
                if (resState < numResStates) {
                    uint64_t modelState = stateIndex / memoryStateCount;
                    uint64_t memoryState = stateIndex % memoryStateCount;
                    uint64_t rowGroupSize = resultTransitionMatrix.getRowGroupSize(resState);
                    if (useRowGrouping) {
                        builder.newRowGroup(resultTransitionMatrix.getRowGroupIndices()[resState]);
                    }
                    if (scheduler && scheduler->getChoice(modelState, memoryState).isDefined()) {
                        std::map<uint64_t, RewardValueType> rewards;
                        for (uint64_t rowOffset = 0; rowOffset < rowGroupSize; ++rowOffset) {
                            uint64_t modelRowIndex = model.getTransitionMatrix().getRowGroupIndices()[modelState] + rowOffset;
                            auto transitionEntryIt = model.getTransitionMatrix().getRow(modelRowIndex).begin();
                            for (auto const& rewardEntry : rewardModel.second.getTransitionRewardMatrix().getRow(modelRowIndex)) {
                                while (transitionEntryIt->getColumn() != rewardEntry.getColumn()) {
                                    STORM_LOG_ASSERT(transitionEntryIt != model.getTransitionMatrix().getRow(modelRowIndex).end(),
                                                     "The reward transition matrix is not a submatrix of the model transition matrix.");
                                    ++transitionEntryIt;
                                }
                                uint64_t transitionId = transitionEntryIt - model.getTransitionMatrix().begin();
                                uint64_t successorMemoryState = memorySuccessors[transitionId * memoryStateCount + memoryState];
                                auto insertionRes =
                                    rewards.insert(std::make_pair(getResultState(rewardEntry.getColumn(), successorMemoryState), rewardEntry.getValue()));
                                if (!insertionRes.second) {
                                    insertionRes.first->second += rewardEntry.getValue();
                                }
                            }
                        }
                        uint64_t resRowIndex = resultTransitionMatrix.getRowGroupIndices()[resState];
                        for (auto& reward : rewards) {
                            builder.addNextValue(resRowIndex, reward.first, reward.second);
                        }
                    } else {
                        for (uint64_t rowOffset = 0; rowOffset < rowGroupSize; ++rowOffset) {
                            uint64_t resRowIndex = resultTransitionMatrix.getRowGroupIndices()[resState] + rowOffset;
                            uint64_t modelRowIndex = model.getTransitionMatrix().getRowGroupIndices()[modelState] + rowOffset;
                            auto transitionEntryIt = model.getTransitionMatrix().getRow(modelRowIndex).begin();
                            for (auto const& rewardEntry : rewardModel.second.getTransitionRewardMatrix().getRow(modelRowIndex)) {
                                while (transitionEntryIt->getColumn() != rewardEntry.getColumn()) {
                                    STORM_LOG_ASSERT(transitionEntryIt != model.getTransitionMatrix().getRow(modelRowIndex).end(),
                                                     "The reward transition matrix is not a submatrix of the model transition matrix.");
                                    ++transitionEntryIt;
                                }
                                uint64_t transitionId = transitionEntryIt - model.getTransitionMatrix().begin();
                                uint64_t successorMemoryState = memorySuccessors[transitionId * memoryStateCount + memoryState];
                                builder.addNextValue(resRowIndex, getResultState(rewardEntry.getColumn(), successorMemoryState), rewardEntry.getValue());
                            }
                        }
                    }
                }
                ++stateIndex;
            }
            transitionRewards = builder.build();
        }
        result.insert(std::make_pair(rewardModel.first, RewardModelType(std::move(stateRewards), std::move(stateActionRewards), std::move(transitionRewards))));
    }
    return result;
}

template<typename ValueType, typename RewardModelType>
std::shared_ptr<storm::models::sparse::Model<ValueType, RewardModelType>> SparseModelMemoryProduct<ValueType, RewardModelType>::buildResult(
    storm::storage::SparseMatrix<ValueType>&& matrix, storm::models::sparse::StateLabeling&& labeling,
    std::unordered_map<std::string, RewardModelType>&& rewardModels, bool preserveModelType) {
    storm::storage::sparse::ModelComponents<ValueType, RewardModelType> components(std::move(matrix), std::move(labeling), std::move(rewardModels));

    auto targetModelType = model.getType();
    if (model.isOfType(storm::models::ModelType::Ctmc)) {
        components.rateTransitions = true;
    } else if (model.isOfType(storm::models::ModelType::Mdp)) {
        if (!preserveModelType && matrix.hasTrivialRowGrouping()) {
            targetModelType = storm::models::ModelType::Dtmc;
        }
    } else if (model.isOfType(storm::models::ModelType::MarkovAutomaton)) {
        // We also need to translate the exit rates and the Markovian states
        uint64_t numResStates = components.transitionMatrix.getRowGroupCount();
        std::vector<ValueType> resultExitRates;
        resultExitRates.reserve(components.transitionMatrix.getRowGroupCount());
        storm::storage::BitVector resultMarkovianStates(numResStates, false);
        auto const& modelExitRates = dynamic_cast<storm::models::sparse::MarkovAutomaton<ValueType, RewardModelType> const&>(model).getExitRates();
        auto const& modelMarkovianStates = dynamic_cast<storm::models::sparse::MarkovAutomaton<ValueType, RewardModelType> const&>(model).getMarkovianStates();

        uint64_t stateIndex = 0;
        for (auto const& resState : toResultStateMapping) {
            if (resState < numResStates) {
                assert(resState == resultExitRates.size());
                uint64_t modelState = stateIndex / memoryStateCount;
                resultExitRates.push_back(modelExitRates[modelState]);
                if (modelMarkovianStates.get(modelState)) {
                    resultMarkovianStates.set(resState, true);
                }
            }
            ++stateIndex;
        }
        components.markovianStates = std::move(resultMarkovianStates);
        components.exitRates = std::move(resultExitRates);
    }

    return storm::utility::builder::buildModelFromComponents(targetModelType, std::move(components));
}

template<typename ValueType, typename RewardModelType>
storm::models::sparse::Model<ValueType, RewardModelType> const& SparseModelMemoryProduct<ValueType, RewardModelType>::getOriginalModel() const {
    return model;
}

template<typename ValueType, typename RewardModelType>
storm::storage::MemoryStructure const& SparseModelMemoryProduct<ValueType, RewardModelType>::getMemory() const {
    return memory;
}

template<typename ValueType, typename RewardModelType>
SparseModelMemoryProductReverseData SparseModelMemoryProduct<ValueType, RewardModelType>::computeReverseData() const {
    STORM_LOG_ASSERT(isInitialized, "The product model has not been built yet. Cannot extract reverse data.");
    return SparseModelMemoryProductReverseData(getOriginalModel().getNumberOfStates(), getMemory(), toResultStateMapping);
}

template class SparseModelMemoryProduct<double>;
template class SparseModelMemoryProduct<double, storm::models::sparse::StandardRewardModel<storm::Interval>>;
template class SparseModelMemoryProduct<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>;
template class SparseModelMemoryProduct<storm::RationalNumber>;
template class SparseModelMemoryProduct<storm::RationalFunction>;
template class SparseModelMemoryProduct<storm::Interval>;
template class SparseModelMemoryProduct<storm::RationalInterval>;

}  // namespace storage
}  // namespace storm
