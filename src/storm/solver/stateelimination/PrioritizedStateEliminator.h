#pragma once

#include "storm/solver/stateelimination/StateEliminator.h"

namespace storm {
namespace solver {
namespace stateelimination {

class StatePriorityQueue;

template<typename ValueType>
class PrioritizedStateEliminator : public StateEliminator<ValueType> {
   public:
    typedef typename std::shared_ptr<StatePriorityQueue> PriorityQueuePointer;

    PrioritizedStateEliminator(storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                               storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions, PriorityQueuePointer priorityQueue,
                               std::vector<ValueType>& stateValues);
    PrioritizedStateEliminator(storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                               storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions,
                               std::vector<storm::storage::sparse::state_type> const& statesToEliminate, std::vector<ValueType>& stateValues);

    // Instantiaton of virtual methods.
    virtual void updateValue(storm::storage::sparse::state_type const& state, ValueType const& loopProbability) override;
    virtual void updatePredecessor(storm::storage::sparse::state_type const& predecessor, ValueType const& probability,
                                   storm::storage::sparse::state_type const& state) override;
    virtual void updatePriority(storm::storage::sparse::state_type const& state) override;

    virtual void eliminateAll(bool eliminateForwardTransitions = true);
    virtual void clearStateValues(storm::storage::sparse::state_type const& state);

   protected:
    PriorityQueuePointer priorityQueue;
    std::vector<ValueType>& stateValues;
};

}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
