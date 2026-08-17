#include "storm/solver/stateelimination/DynamicStatePriorityQueue.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {
namespace stateelimination {

template<typename ValueType>
DynamicStatePriorityQueue<ValueType>::DynamicStatePriorityQueue(
    std::vector<std::pair<storm::storage::sparse::state_type, uint_fast64_t>> const& sortedStatePenaltyPairs,
    storm::storage::FlexibleSparseMatrix<ValueType> const& transitionMatrix, storm::storage::FlexibleSparseMatrix<ValueType> const& backwardTransitions,
    std::vector<ValueType> const& oneStepProbabilities, PenaltyFunctionType const& penaltyFunction)
    : StatePriorityQueue(),
      transitionMatrix(transitionMatrix),
      backwardTransitions(backwardTransitions),
      oneStepProbabilities(oneStepProbabilities),
      priorityQueue(),
      stateToPriorityQueueEntry(),
      penaltyFunction(penaltyFunction) {
    // Insert all state-penalty pairs into our priority queue.
    for (auto const& statePenalty : sortedStatePenaltyPairs) {
        auto it = priorityQueue.insert(priorityQueue.end(), statePenalty);
        stateToPriorityQueueEntry.emplace(statePenalty.first, it);
    }
}

template<typename ValueType>
bool DynamicStatePriorityQueue<ValueType>::hasNext() const {
    return !priorityQueue.empty();
}

template<typename ValueType>
storm::storage::sparse::state_type DynamicStatePriorityQueue<ValueType>::pop() {
    auto it = priorityQueue.begin();
    STORM_LOG_TRACE("Popping state " << it->first << " with priority " << it->second << ".");
    storm::storage::sparse::state_type result = it->first;
    priorityQueue.erase(priorityQueue.begin());
    stateToPriorityQueueEntry.erase(result);
    return result;
}

template<typename ValueType>
void DynamicStatePriorityQueue<ValueType>::update(storm::storage::sparse::state_type state) {
    // First, we need to find the old priority queue entry for the state.
    auto priorityQueueEntryIt = stateToPriorityQueueEntry.find(state);

    // If the priority queue does not store the priority of the given state, we must not update it.
    if (priorityQueueEntryIt == stateToPriorityQueueEntry.end()) {
        return;
    }

    // Compute the new priority.
    uint_fast64_t newPriority = penaltyFunction(state, transitionMatrix, backwardTransitions, oneStepProbabilities);

    if (priorityQueueEntryIt->second->second != newPriority) {
        // Erase and re-insert the entry into priority queue (with the new priority).
        priorityQueue.erase(priorityQueueEntryIt->second);
        stateToPriorityQueueEntry.erase(priorityQueueEntryIt);

        auto newElementIt = priorityQueue.emplace(state, newPriority);
        stateToPriorityQueueEntry.emplace(state, newElementIt.first);
    }
}

template<typename ValueType>
std::size_t DynamicStatePriorityQueue<ValueType>::size() const {
    return priorityQueue.size();
}

template class DynamicStatePriorityQueue<double>;

template class DynamicStatePriorityQueue<storm::RationalNumber>;
template class DynamicStatePriorityQueue<storm::RationalFunction>;
}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
