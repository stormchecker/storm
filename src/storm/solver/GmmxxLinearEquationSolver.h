#pragma once

#include <ostream>

#include "storm/adapters/gmm.h"

#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {
namespace solver {

/*!
 * A class that uses the gmm++ library to implement the LinearEquationSolver interface.
 */
template<typename ValueType>
class GmmxxLinearEquationSolver : public LinearEquationSolver<ValueType> {
   public:
    GmmxxLinearEquationSolver();
    GmmxxLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A);
    GmmxxLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A);

    virtual void setMatrix(storm::storage::SparseMatrix<ValueType> const& A) override;
    virtual void setMatrix(storm::storage::SparseMatrix<ValueType>&& A) override;

    virtual LinearEquationSolverProblemFormat getEquationProblemFormat(Environment const& env) const override;

    virtual void clearCache() const override;

   protected:
    virtual bool internalSolveEquations(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const& b) const override;

   private:
    GmmxxLinearEquationSolverMethod getMethod(Environment const& env) const;

    virtual uint64_t getMatrixRowCount() const override;
    virtual uint64_t getMatrixColumnCount() const override;
#ifdef STORM_HAVE_GMM
    // The matrix in gmm++ format.
    std::unique_ptr<gmm::csr_matrix<ValueType>> gmmxxA;

    // cached data obtained during solving
    mutable std::unique_ptr<gmm::ilu_precond<gmm::csr_matrix<ValueType>>> iluPreconditioner;
    mutable std::unique_ptr<gmm::diagonal_precond<gmm::csr_matrix<ValueType>>> diagonalPreconditioner;
#endif
};

template<typename ValueType>
class GmmxxLinearEquationSolverFactory : public LinearEquationSolverFactory<ValueType> {
   public:
    using LinearEquationSolverFactory<ValueType>::create;

    virtual std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> create(Environment const& env) const override;

    virtual std::unique_ptr<LinearEquationSolverFactory<ValueType>> clone() const override;
};

}  // namespace solver
}  // namespace storm
