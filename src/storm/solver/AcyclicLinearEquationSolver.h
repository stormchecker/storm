#pragma once

#include <optional>

#include <memory>
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/multiplier/Multiplier.h"

namespace storm {

class Environment;

namespace solver {

/*!
 * This solver can be used on equation systems that are known to be acyclic.
 * It is optimized for solving many instances of the equation system with the same underlying matrix.
 */
template<typename ValueType>
class AcyclicLinearEquationSolver : public LinearEquationSolver<ValueType> {
   public:
    AcyclicLinearEquationSolver();
    AcyclicLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A);
    AcyclicLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A);

    virtual void setMatrix(storm::storage::SparseMatrix<ValueType> const& A) override;
    virtual void setMatrix(storm::storage::SparseMatrix<ValueType>&& A) override;

    virtual ~AcyclicLinearEquationSolver() {}

    virtual void clearCache() const override;

    virtual LinearEquationSolverProblemFormat getEquationProblemFormat(storm::Environment const& env) const override;
    virtual LinearEquationSolverRequirements getRequirements(Environment const& env) const override;

   protected:
    virtual bool internalSolveEquations(storm::Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const& b) const override;

   private:
    virtual uint64_t getMatrixRowCount() const override;
    virtual uint64_t getMatrixColumnCount() const override;

    // If the solver takes posession of the matrix, we store the moved matrix in this member, so it gets deleted
    // when the solver is destructed.
    std::unique_ptr<storm::storage::SparseMatrix<ValueType>> localA;
    // A pointer to the original sparse matrix given to this solver. If the solver takes posession of the matrix
    // the pointer refers to orderedMatrix.
    storm::storage::SparseMatrix<ValueType> const* A;

    // cached multiplier either with original matrix or ordered matrix
    mutable std::unique_ptr<storm::solver::Multiplier<ValueType>> multiplier;
    // cached matrix for the multiplier (only if different from original matrix)
    mutable boost::optional<storm::storage::SparseMatrix<ValueType>> orderedMatrix;
    // cached row group ordering (only if not identity)
    mutable boost::optional<std::vector<uint64_t>> rowOrdering;  // A.rowGroupCount() entries
    // can be used if the entries in 'b' need to be reordered
    mutable boost::optional<std::vector<ValueType>> auxiliaryRowVector;  // A.rowCount() entries
    // can be used if the entries in 'x' need to be reordered
    mutable boost::optional<std::vector<ValueType>> auxiliaryRowVector2;  // A.rowCount() entries
    // contains factors applied to scale the entries of the 'b' vector
    mutable std::vector<std::pair<uint64_t, std::optional<ValueType>>> bFactors;
};
}  // namespace solver
}  // namespace storm
