#include "storm/solver/stateelimination/PrioritizedStateEliminator.h"

#include "StaticStatePriorityQueue.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/solver/stateelimination/StatePriorityQueue.h"
#include "storm/utility/constants.h"

namespace storm {
namespace solver {
namespace stateelimination {

template<typename ValueType>
PrioritizedStateEliminator<ValueType>::PrioritizedStateEliminator(storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                                                                  storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions,
                                                                  std::vector<storm::storage::sparse::state_type> const& statesToEliminate,
                                                                  std::vector<ValueType>& stateValues)
    : PrioritizedStateEliminator(transitionMatrix, backwardTransitions, std::make_shared<StaticStatePriorityQueue>(statesToEliminate), stateValues) {}

template<typename ValueType>
PrioritizedStateEliminator<ValueType>::PrioritizedStateEliminator(storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                                                                  storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions,
                                                                  PriorityQueuePointer priorityQueue, std::vector<ValueType>& stateValues)
    : StateEliminator<ValueType>(transitionMatrix, backwardTransitions), priorityQueue(priorityQueue), stateValues(stateValues) {}

template<typename ValueType>
void PrioritizedStateEliminator<ValueType>::updateValue(storm::storage::sparse::state_type const& state, ValueType const& loopProbability) {
    stateValues[state] = storm::utility::simplify((ValueType)(loopProbability * stateValues[state]));
}

template<typename ValueType>
void PrioritizedStateEliminator<ValueType>::updatePredecessor(storm::storage::sparse::state_type const& predecessor, ValueType const& probability,
                                                              storm::storage::sparse::state_type const& state) {
    stateValues[predecessor] =
        storm::utility::simplify((ValueType)(stateValues[predecessor] + storm::utility::simplify((ValueType)(probability * stateValues[state]))));
}

template<typename ValueType>
void PrioritizedStateEliminator<ValueType>::updatePriority(storm::storage::sparse::state_type const& state) {
    priorityQueue->update(state);
}

template<typename ValueType>
void PrioritizedStateEliminator<ValueType>::eliminateAll(bool removeForwardTransitions) {
    while (priorityQueue->hasNext()) {
        storm::storage::sparse::state_type state = priorityQueue->pop();
        this->eliminateState(state, removeForwardTransitions);
        if (removeForwardTransitions) {
            clearStateValues(state);
        }
    }
}

template<typename ValueType>
void PrioritizedStateEliminator<ValueType>::clearStateValues(storm::storage::sparse::state_type const& state) {
    stateValues[state] = storm::utility::zero<ValueType>();
}

template class PrioritizedStateEliminator<double>;

template class PrioritizedStateEliminator<storm::RationalNumber>;
template class PrioritizedStateEliminator<storm::RationalFunction>;
}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
