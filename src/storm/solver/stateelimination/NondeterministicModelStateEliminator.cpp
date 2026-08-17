#include "storm/solver/stateelimination/NondeterministicModelStateEliminator.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {
namespace stateelimination {

template<typename ValueType>
NondeterministicModelStateEliminator<ValueType>::NondeterministicModelStateEliminator(storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                                                                                      storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions,
                                                                                      std::vector<ValueType>& rowValues)
    : StateEliminator<ValueType>(transitionMatrix, backwardTransitions), rowValues(rowValues) {
    STORM_LOG_THROW(
        transitionMatrix.getRowCount() == backwardTransitions.getColumnCount() && transitionMatrix.getColumnCount() == backwardTransitions.getRowCount(),
        storm::exceptions::InvalidArgumentException, "Invalid matrix dimensions of forward/backwards transition matrices.");
    STORM_LOG_THROW(rowValues.size() == transitionMatrix.getRowCount(), storm::exceptions::InvalidArgumentException, "Invalid size of row value vector.");
    // Intentionally left empty
}

template<typename ValueType>
void NondeterministicModelStateEliminator<ValueType>::updateValue(storm::storage::sparse::state_type const& row, ValueType const& loopProbability) {
    rowValues[row] = storm::utility::simplify((ValueType)(loopProbability * rowValues[row]));
}

template<typename ValueType>
void NondeterministicModelStateEliminator<ValueType>::updatePredecessor(storm::storage::sparse::state_type const& predecessorRow, ValueType const& probability,
                                                                        storm::storage::sparse::state_type const& row) {
    rowValues[predecessorRow] =
        storm::utility::simplify((ValueType)(rowValues[predecessorRow] + storm::utility::simplify((ValueType)(probability * rowValues[row]))));
}

template class NondeterministicModelStateEliminator<double>;

template class NondeterministicModelStateEliminator<storm::RationalNumber>;
template class NondeterministicModelStateEliminator<storm::RationalFunction>;
}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
