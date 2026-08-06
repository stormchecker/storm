#pragma once

#include "storm/solver/stateelimination/EliminatorBase.h"

namespace storm {
namespace solver {
namespace stateelimination {

template<typename ValueType>
class StateEliminator : public EliminatorBase<ValueType, ScalingMode::DivideOneMinus> {
   public:
    StateEliminator(storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix, storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions);

    void eliminateState(storm::storage::sparse::state_type state, bool removeForwardTransitions);
};

}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
