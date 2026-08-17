#include "storm/storage/SparseMatrixOperations.h"

#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm::storage {
std::set<storm::RationalFunctionVariable> getVariables(SparseMatrix<storm::RationalFunction> const& matrix) {
    std::set<storm::RationalFunctionVariable> result;
    for (auto const& entry : matrix) {
        entry.getValue().gatherVariables(result);
    }
    return result;
}

}  // namespace storm::storage