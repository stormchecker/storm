#pragma once

#include <optional>

#include <memory>

#include "storm/solver/StandardMinMaxLinearEquationSolver.h"
#include "storm/solver/multiplier/Multiplier.h"

namespace storm {

class Environment;

namespace solver {

/*!
 * This solver can be used on equation systems that are known to be acyclic.
 * It is optimized for solving many instances of the equation system with the same underlying matrix.
 */
template<typename ValueType>
class AcyclicMinMaxLinearEquationSolver : public StandardMinMaxLinearEquationSolver<ValueType> {
   public:
    AcyclicMinMaxLinearEquationSolver();
    AcyclicMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A);
    AcyclicMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A);

    virtual ~AcyclicMinMaxLinearEquationSolver() {}

    virtual void clearCache() const override;

    virtual MinMaxLinearEquationSolverRequirements getRequirements(Environment const& env,
                                                                   boost::optional<storm::solver::OptimizationDirection> const& direction = boost::none,
                                                                   bool const& hasInitialScheduler = false) const override;

   protected:
    virtual bool internalSolveEquations(storm::Environment const& env, OptimizationDirection d, std::vector<ValueType>& x,
                                        std::vector<ValueType> const& b) const override;

   private:
    // cached multiplier either with original matrix or ordered matrix
    mutable std::unique_ptr<storm::solver::Multiplier<ValueType>> multiplier;
    // cached matrix for the multiplier (only if different from original matrix)
    mutable boost::optional<storm::storage::SparseMatrix<ValueType>> orderedMatrix;
    // cached row group ordering (only if not identity)
    mutable boost::optional<std::vector<uint64_t>> rowGroupOrdering;  // A.rowGroupCount() entries
    // can be used if the entries in 'b' need to be reordered
    mutable boost::optional<std::vector<ValueType>> auxiliaryRowVector;  // A.rowCount() entries
    // can be used if the entries in 'x' need to be reordered
    mutable boost::optional<std::vector<ValueType>> auxiliaryRowGroupVector;  // A.rowGroupCount() entries
    // can be used if the performed scheduler choices need to be reordered
    mutable boost::optional<std::vector<uint64_t>> auxiliaryRowGroupIndexVector;  // A.rowGroupCount() entries
    // contains factors applied to scale the entries of the 'b' vector
    mutable std::vector<std::pair<uint64_t, std::optional<ValueType>>> bFactors;
};
}  // namespace solver
}  // namespace storm
