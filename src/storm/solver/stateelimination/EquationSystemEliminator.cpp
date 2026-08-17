#include "storm/solver/stateelimination/EquationSystemEliminator.h"

namespace storm {
namespace solver {
namespace stateelimination {

template<typename ValueType>
EquationSystemEliminator<ValueType>::EquationSystemEliminator(storm::storage::FlexibleSparseMatrix<ValueType>& matrix,
                                                              storm::storage::FlexibleSparseMatrix<ValueType>& transposedMatrix)
    : EliminatorBase<ValueType, ScalingMode::Divide>(matrix, transposedMatrix) {
    // Intentionally left empty.
}

template class EquationSystemEliminator<double>;
template class EquationSystemEliminator<storm::RationalNumber>;
template class EquationSystemEliminator<storm::RationalFunction>;

}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
