#pragma once

#include "storm/solver/LinearEquationSolver.h"

namespace storm {
namespace solver {

/*!
 * A class that uses gaussian elimination to implement the LinearEquationSolver interface.
 */
template<typename ValueType>
class EliminationLinearEquationSolver : public LinearEquationSolver<ValueType> {
   public:
    EliminationLinearEquationSolver();
    EliminationLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A);
    EliminationLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A);

    virtual void setMatrix(storm::storage::SparseMatrix<ValueType> const& A) override;
    virtual void setMatrix(storm::storage::SparseMatrix<ValueType>&& A) override;

    virtual LinearEquationSolverProblemFormat getEquationProblemFormat(Environment const& env) const override;

   protected:
    virtual bool internalSolveEquations(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const& b) const override;

   private:
    virtual uint64_t getMatrixRowCount() const override;
    virtual uint64_t getMatrixColumnCount() const override;

    // If the solver takes posession of the matrix, we store the moved matrix in this member, so it gets deleted
    // when the solver is destructed.
    std::unique_ptr<storm::storage::SparseMatrix<ValueType>> localA;

    // A pointer to the original sparse matrix given to this solver. If the solver takes posession of the matrix
    // the pointer refers to localA.
    storm::storage::SparseMatrix<ValueType> const* A;
};

template<typename ValueType>
class EliminationLinearEquationSolverFactory : public LinearEquationSolverFactory<ValueType> {
   public:
    using LinearEquationSolverFactory<ValueType>::create;

    virtual std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> create(Environment const& env) const override;

    virtual std::unique_ptr<LinearEquationSolverFactory<ValueType>> clone() const override;
};
}  // namespace solver
}  // namespace storm
