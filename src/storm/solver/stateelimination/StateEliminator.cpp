#include "storm/solver/stateelimination/StateEliminator.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/solver/stateelimination/StateEliminationUtility.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {
namespace stateelimination {

template<typename ValueType>
StateEliminator<ValueType>::StateEliminator(storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                                            storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions)
    : EliminatorBase<ValueType, ScalingMode::DivideOneMinus>(transitionMatrix, backwardTransitions) {
    // Intentionally left empty.
}

template<typename ValueType>
void StateEliminator<ValueType>::eliminateState(storm::storage::sparse::state_type state, bool removeForwardTransitions) {
    STORM_LOG_TRACE("Eliminating state " << state << ".");
    if (this->matrix.hasTrivialRowGrouping()) {
        this->eliminate(state, state, removeForwardTransitions);
    } else {
        STORM_LOG_THROW(this->matrix.getRowGroupSize(state) == 1, storm::exceptions::IllegalArgumentException,
                        "Invoked state elimination on a state with multiple choices. This is not supported.");
        this->eliminate(this->matrix.getRowGroupIndices()[state], state, removeForwardTransitions);
    }
}

template class StateEliminator<double>;

template class StateEliminator<storm::RationalNumber>;
template class StateEliminator<storm::RationalFunction>;
}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
