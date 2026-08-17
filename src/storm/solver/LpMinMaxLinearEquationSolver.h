#pragma once

#include "storm/solver/LpSolver.h"
#include "storm/solver/StandardMinMaxLinearEquationSolver.h"
#include "storm/utility/solver.h"

namespace storm {

class Environment;

namespace solver {

/*!
 * Solves a MinMaxLinearEquationSystem using a linear programming solver.
 * @see doi.org/10.1007/978-3-031-30823-9_24 for a description of the algorithm as implemented here.
 * @tparam ValueType
 */
template<typename ValueType>
class LpMinMaxLinearEquationSolver : public StandardMinMaxLinearEquationSolver<ValueType> {
   public:
    LpMinMaxLinearEquationSolver(std::unique_ptr<storm::utility::solver::LpSolverFactory<ValueType>>&& lpSolverFactory);
    LpMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A,
                                 std::unique_ptr<storm::utility::solver::LpSolverFactory<ValueType>>&& lpSolverFactory);
    LpMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A,
                                 std::unique_ptr<storm::utility::solver::LpSolverFactory<ValueType>>&& lpSolverFactory);

    virtual bool internalSolveEquations(Environment const& env, OptimizationDirection dir, std::vector<ValueType>& x,
                                        std::vector<ValueType> const& b) const override;

    virtual void clearCache() const override;

    virtual MinMaxLinearEquationSolverRequirements getRequirements(Environment const& env,
                                                                   boost::optional<storm::solver::OptimizationDirection> const& direction = boost::none,
                                                                   bool const& hasInitialScheduler = false) const override;

   private:
    bool solveEquationsViToLp(Environment const& env, OptimizationDirection dir, std::vector<ValueType>& x, std::vector<ValueType> const& b) const;
    bool solveEquationsLp(Environment const& env, OptimizationDirection dir, std::vector<ValueType>& x, std::vector<ValueType> const& b,
                          std::vector<ValueType> const* lowerBounds = nullptr, std::vector<ValueType> const* upperBounds = nullptr) const;

    std::unique_ptr<storm::utility::solver::LpSolverFactory<ValueType>> lpSolverFactory;
};

}  // namespace solver
}  // namespace storm
