#pragma once

#include "storm/solver/LinearEquationSolver.h"

#include "storm/solver/SolverSelectionOptions.h"
#include "storm/solver/multiplier/NativeMultiplier.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"

namespace storm {

class Environment;

namespace solver {

template<typename ValueType>
class TopologicalLinearEquationSolver : public LinearEquationSolver<ValueType> {
   public:
    TopologicalLinearEquationSolver();
    TopologicalLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A);
    TopologicalLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A);

    virtual void setMatrix(storm::storage::SparseMatrix<ValueType> const& A) override;
    virtual void setMatrix(storm::storage::SparseMatrix<ValueType>&& A) override;

    virtual LinearEquationSolverProblemFormat getEquationProblemFormat(storm::Environment const& env) const override;
    virtual LinearEquationSolverRequirements getRequirements(Environment const& env) const override;

    virtual void clearCache() const override;

   protected:
    virtual bool internalSolveEquations(storm::Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const& b) const override;

   private:
    virtual uint64_t getMatrixRowCount() const override;
    virtual uint64_t getMatrixColumnCount() const override;

    storm::Environment getEnvironmentForUnderlyingSolver(storm::Environment const& env, bool adaptPrecision = false) const;

    // Creates an SCC decomposition and sorts the SCCs according to a topological sort.
    void createSortedSccDecomposition(bool needLongestChainSize) const;

    // Solves the SCC with the given index
    // ... for the case that the SCC is trivial
    bool solveTrivialScc(uint64_t const& sccState, std::vector<ValueType>& globalX, std::vector<ValueType> const& globalB) const;
    // ... for the case that there is just one large SCC
    bool solveFullyConnectedEquationSystem(storm::Environment const& sccSolverEnvironment, std::vector<ValueType>& x, std::vector<ValueType> const& b) const;
    // ... for the remaining cases (1 < scc.size() < x.size())
    bool solveScc(storm::Environment const& sccSolverEnvironment, storm::storage::BitVector const& scc, std::vector<ValueType>& globalX,
                  std::vector<ValueType> const& globalB, std::optional<storm::storage::BitVector> const& globalRelevantValues) const;

    // If the solver takes posession of the matrix, we store the moved matrix in this member, so it gets deleted
    // when the solver is destructed.
    std::unique_ptr<storm::storage::SparseMatrix<ValueType>> localA;

    // A pointer to the original sparse matrix given to this solver. If the solver takes posession of the matrix
    // the pointer refers to localA.
    storm::storage::SparseMatrix<ValueType> const* A;

    // cached auxiliary data
    mutable std::unique_ptr<storm::storage::StronglyConnectedComponentDecomposition<ValueType>> sortedSccDecomposition;
    mutable boost::optional<uint64_t> longestSccChainSize;
    mutable std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> sccSolver;
};

template<typename ValueType>
class TopologicalLinearEquationSolverFactory : public LinearEquationSolverFactory<ValueType> {
   public:
    using LinearEquationSolverFactory<ValueType>::create;

    virtual std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> create(Environment const& env) const override;

    virtual std::unique_ptr<LinearEquationSolverFactory<ValueType>> clone() const override;
};
}  // namespace solver
}  // namespace storm
