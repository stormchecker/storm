#pragma once

#include <variant>
#include "storm/solver/MultiplicationStyle.h"
#include "storm/solver/OptimizationDirection.h"

#include "storm/utility/NumberTraits.h"

#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/StandardMinMaxLinearEquationSolver.h"

#include "storm/solver/helper/ValueIterationOperator.h"

#include "storm/solver/SolverStatus.h"

namespace storm {

class Environment;

namespace solver {

template<typename ValueType, typename SolutionType = ValueType>
class IterativeMinMaxLinearEquationSolver : public StandardMinMaxLinearEquationSolver<ValueType, SolutionType> {
   public:
    IterativeMinMaxLinearEquationSolver(std::unique_ptr<LinearEquationSolverFactory<SolutionType>>&& linearEquationSolverFactory);
    IterativeMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A,
                                        std::unique_ptr<LinearEquationSolverFactory<SolutionType>>&& linearEquationSolverFactory);
    IterativeMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A,
                                        std::unique_ptr<LinearEquationSolverFactory<SolutionType>>&& linearEquationSolverFactory);

    virtual bool internalSolveEquations(Environment const& env, OptimizationDirection dir, std::vector<SolutionType>& x,
                                        std::vector<ValueType> const& b) const override;

    virtual void clearCache() const override;

    virtual MinMaxLinearEquationSolverRequirements getRequirements(Environment const& env,
                                                                   boost::optional<storm::solver::OptimizationDirection> const& direction = boost::none,
                                                                   bool const& hasInitialScheduler = false) const override;

   private:
    MinMaxMethod getMethod(Environment const& env, bool isExactMode) const;

    bool solveInducedEquationSystem(Environment const& env, std::unique_ptr<LinearEquationSolver<SolutionType>>& linearEquationSolver,
                                    std::vector<uint64_t> const& scheduler, std::vector<SolutionType>& x, std::vector<ValueType>& subB,
                                    std::vector<ValueType> const& originalB, OptimizationDirection dir) const;
    bool solveEquationsPolicyIteration(Environment const& env, OptimizationDirection dir, std::vector<SolutionType>& x, std::vector<ValueType> const& b) const;
    bool performPolicyIteration(Environment const& env, OptimizationDirection dir, std::vector<SolutionType>& x, std::vector<ValueType> const& b,
                                std::vector<storm::storage::sparse::state_type>&& initialPolicy) const;
    bool valueImproved(OptimizationDirection dir, ValueType const& value1, ValueType const& value2) const;

    bool solveEquationsValueIteration(Environment const& env, OptimizationDirection dir, std::vector<SolutionType>& x, std::vector<ValueType> const& b) const;
    bool solveEquationsOptimisticValueIteration(Environment const& env, OptimizationDirection dir, std::vector<SolutionType>& x,
                                                std::vector<ValueType> const& b) const;
    bool solveEquationsGuessingValueIteration(Environment const& env, OptimizationDirection dir, std::vector<SolutionType>& x,
                                              std::vector<ValueType> const& b) const;
    bool solveEquationsIntervalIteration(Environment const& env, OptimizationDirection dir, std::vector<SolutionType>& x,
                                         std::vector<ValueType> const& b) const;
    bool solveEquationsSoundValueIteration(Environment const& env, OptimizationDirection dir, std::vector<SolutionType>& x,
                                           std::vector<ValueType> const& b) const;
    bool solveEquationsViToPi(Environment const& env, OptimizationDirection dir, std::vector<SolutionType>& x, std::vector<ValueType> const& b) const;

    bool solveEquationsRationalSearch(Environment const& env, OptimizationDirection dir, std::vector<SolutionType>& x, std::vector<ValueType> const& b) const;

    void setUpViOperator() const;
    void extractScheduler(std::vector<SolutionType>& x, std::vector<ValueType> const& b, OptimizationDirection const& dir,
                          UncertaintyResolutionMode uncertaintyResolutionMode, bool updateX = true) const;

    /// The factory used to obtain linear equation solvers.
    std::unique_ptr<LinearEquationSolverFactory<SolutionType>> linearEquationSolverFactory;

    // possibly cached data

    // two different VI operators, one for trivialrowgrouping, one without
    mutable std::shared_ptr<storm::solver::helper::ValueIterationOperator<ValueType, true, SolutionType>> viOperatorTriv;
    mutable std::shared_ptr<storm::solver::helper::ValueIterationOperator<ValueType, false, SolutionType>> viOperatorNontriv;

    mutable std::unique_ptr<std::vector<ValueType>> auxiliaryRowGroupVector;  // A.rowGroupCount() entries
};

}  // namespace solver
}  // namespace storm
