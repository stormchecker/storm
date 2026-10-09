#pragma once

#include <optional>

#include "storm/solver/StandardMinMaxLinearEquationSolver.h"

#include "storm/solver/SolverSelectionOptions.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/storage/SubmatrixBuilder.h"

namespace storm {

class Environment;

namespace solver {

template<typename ValueType, typename SolutionType = ValueType>
class TopologicalMinMaxLinearEquationSolver : public StandardMinMaxLinearEquationSolver<ValueType, SolutionType> {
   public:
    TopologicalMinMaxLinearEquationSolver();
    TopologicalMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A);
    TopologicalMinMaxLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A);

    virtual ~TopologicalMinMaxLinearEquationSolver() {}

    virtual void clearCache() const override;

    virtual MinMaxLinearEquationSolverRequirements getRequirements(Environment const& env,
                                                                   boost::optional<storm::solver::OptimizationDirection> const& direction = boost::none,
                                                                   bool const& hasInitialScheduler = false) const override;

   protected:
    virtual bool internalSolveEquations(storm::Environment const& env, OptimizationDirection d, std::vector<SolutionType>& x,
                                        std::vector<ValueType> const& b) const override;

   private:
    storm::Environment getEnvironmentForUnderlyingSolver(storm::Environment const& env, bool adaptPrecision = false) const;

    // Creates an SCC decomposition and sorts the SCCs according to a topological sort.
    void createSortedSccDecomposition(bool needLongestChainSize) const;

    // Reports what the SCC-wise solve established about the solution: the values themselves if every SCC was
    // solved exactly, otherwise the precision they were solved with, if every SCC was solved that accurately.
    void trySetSolutionBounds(Environment const& env, std::vector<SolutionType> const& x, bool allSccsExact, bool allSccsPrecise) const;

    // Retrieves whether the bounds the SCC solver reported for the SCC it just solved show that it achieved the
    // precision the given environment asked of it. Adding up the errors of the SCCs relies on that.
    bool lastSccMetPrecision(Environment const& sccSolverEnvironment) const;

    // Solves the SCC with the given index
    // ... for the case that the SCC is trivial
    bool solveTrivialScc(uint64_t const& sccState, OptimizationDirection d, std::vector<SolutionType>& globalX, std::vector<ValueType> const& globalB) const;
    // ... for the case that there is just one large SCC
    bool solveFullyConnectedEquationSystem(storm::Environment const& sccSolverEnvironment, OptimizationDirection d, std::vector<SolutionType>& x,
                                           std::vector<ValueType> const& b) const;
    // ... for the remaining cases (1 < scc.size() < x.size())
    bool solveScc(storm::Environment const& sccSolverEnvironment, OptimizationDirection d, storm::storage::StronglyConnectedComponent const& scc,
                  storm::storage::BitVector const& sccAsBitVector, std::vector<SolutionType>& globalX, std::vector<ValueType> const& globalB,
                  std::optional<storm::storage::BitVector> const& globalRelevantValues) const;

    // cached auxiliary data
    mutable std::unique_ptr<storm::storage::StronglyConnectedComponentDecomposition<ValueType>> sortedSccDecomposition;
    mutable boost::optional<uint64_t> longestSccChainSize;
    mutable std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> sccSolver;
    // Builds the submatrices of all SCCs (reuses its internal lookup table between SCCs)
    mutable std::unique_ptr<storm::storage::SubmatrixBuilder<ValueType>> sccSubmatrixBuilder;
    mutable std::unique_ptr<std::vector<ValueType>> auxiliaryRowGroupVector;  // A.rowGroupCount() entries
};
}  // namespace solver
}  // namespace storm
