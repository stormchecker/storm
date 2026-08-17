#pragma once

#include "storm/solver/stateelimination/StateEliminator.h"

namespace storm {
namespace solver {
namespace stateelimination {

template<typename ValueType>
class NondeterministicModelStateEliminator : public StateEliminator<ValueType> {
   public:
    NondeterministicModelStateEliminator(storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                                         storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions, std::vector<ValueType>& rowValues);

    // Instantiation of virtual methods.
    virtual void updateValue(storm::storage::sparse::state_type const& row, ValueType const& loopProbability) override;
    virtual void updatePredecessor(storm::storage::sparse::state_type const& predecessorRow, ValueType const& probability,
                                   storm::storage::sparse::state_type const& row) override;

   protected:
    std::vector<ValueType>& rowValues;
};

}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
