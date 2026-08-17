#include "storm/solver/StandardMinMaxLinearEquationSolver.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/solver/EigenLinearEquationSolver.h"
#include "storm/solver/TopologicalLinearEquationSolver.h"
#include "storm/utility/vector.h"

namespace storm::solver {

template<typename ValueType, typename SolutionType>
StandardMinMaxLinearEquationSolver<ValueType, SolutionType>::StandardMinMaxLinearEquationSolver() : A(nullptr) {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
StandardMinMaxLinearEquationSolver<ValueType, SolutionType>::StandardMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A)
    : localA(nullptr), A(&A) {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
StandardMinMaxLinearEquationSolver<ValueType, SolutionType>::StandardMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A)
    : localA(std::make_unique<storm::storage::SparseMatrix<ValueType>>(std::move(A))), A(localA.get()) {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
void StandardMinMaxLinearEquationSolver<ValueType, SolutionType>::setMatrix(storm::storage::SparseMatrix<ValueType> const& matrix) {
    this->localA = nullptr;
    this->A = &matrix;
    this->clearCache();
}

template<typename ValueType, typename SolutionType>
void StandardMinMaxLinearEquationSolver<ValueType, SolutionType>::setMatrix(storm::storage::SparseMatrix<ValueType>&& matrix) {
    this->localA = std::make_unique<storm::storage::SparseMatrix<ValueType>>(std::move(matrix));
    this->A = this->localA.get();
    this->clearCache();
}

template class StandardMinMaxLinearEquationSolver<double>;
template class StandardMinMaxLinearEquationSolver<storm::RationalNumber>;
template class StandardMinMaxLinearEquationSolver<storm::Interval, double>;
template class StandardMinMaxLinearEquationSolver<storm::RationalInterval, storm::RationalNumber>;

}  // namespace storm::solver
