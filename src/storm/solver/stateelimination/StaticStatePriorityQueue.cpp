#include "storm/solver/stateelimination/StaticStatePriorityQueue.h"

namespace storm {
namespace solver {
namespace stateelimination {

StaticStatePriorityQueue::StaticStatePriorityQueue(std::vector<storm::storage::sparse::state_type> const& sortedStates)
    : StatePriorityQueue(), sortedStates(sortedStates), currentPosition(0) {
    // Intentionally left empty.
}

bool StaticStatePriorityQueue::hasNext() const {
    return currentPosition < sortedStates.size();
}

storm::storage::sparse::state_type StaticStatePriorityQueue::pop() {
    ++currentPosition;
    return sortedStates[currentPosition - 1];
}

std::size_t StaticStatePriorityQueue::size() const {
    return sortedStates.size() - currentPosition;
}

}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
