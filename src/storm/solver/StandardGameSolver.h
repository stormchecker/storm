#pragma once

#include "SolverSelectionOptions.h"
#include "storm/solver/GameSolver.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/SolverStatus.h"
#include "storm/solver/multiplier/Multiplier.h"

namespace storm {
namespace solver {

template<typename ValueType>
class StandardGameSolver : public GameSolver<ValueType> {
   public:
    // Constructors for when the first player is represented using a matrix.
    StandardGameSolver(storm::storage::SparseMatrix<storm::storage::sparse::state_type> const& player1Matrix,
                       storm::storage::SparseMatrix<ValueType> const& player2Matrix,
                       std::unique_ptr<LinearEquationSolverFactory<ValueType>>&& linearEquationSolverFactory);
    StandardGameSolver(storm::storage::SparseMatrix<storm::storage::sparse::state_type>&& player1Matrix,
                       storm::storage::SparseMatrix<ValueType>&& player2Matrix,
                       std::unique_ptr<LinearEquationSolverFactory<ValueType>>&& linearEquationSolverFactory);

    // Constructor for when the first player is represented by a grouping of the player 2 states (row groups).
    StandardGameSolver(std::vector<uint64_t> const& player1Groups, storm::storage::SparseMatrix<ValueType> const& player2Matrix,
                       std::unique_ptr<LinearEquationSolverFactory<ValueType>>&& linearEquationSolverFactory);
    StandardGameSolver(std::vector<uint64_t>&& player1Groups, storm::storage::SparseMatrix<ValueType>&& player2Matrix,
                       std::unique_ptr<LinearEquationSolverFactory<ValueType>>&& linearEquationSolverFactory);

    virtual bool solveGame(Environment const& env, OptimizationDirection player1Dir, OptimizationDirection player2Dir, std::vector<ValueType>& x,
                           std::vector<ValueType> const& b, std::vector<uint64_t>* player1Choices = nullptr,
                           std::vector<uint64_t>* player2Choices = nullptr) const override;
    virtual void repeatedMultiply(Environment const& env, OptimizationDirection player1Dir, OptimizationDirection player2Dir, std::vector<ValueType>& x,
                                  std::vector<ValueType> const* b, uint_fast64_t n = 1) const override;

    virtual void clearCache() const override;

   private:
    GameMethod getMethod(Environment const& env, bool isExactMode) const;

    bool solveGamePolicyIteration(Environment const& env, OptimizationDirection player1Dir, OptimizationDirection player2Dir, std::vector<ValueType>& x,
                                  std::vector<ValueType> const& b, std::vector<uint64_t>* player1Choices = nullptr,
                                  std::vector<uint64_t>* player2Choices = nullptr) const;
    bool solveGameValueIteration(Environment const& env, OptimizationDirection player1Dir, OptimizationDirection player2Dir, std::vector<ValueType>& x,
                                 std::vector<ValueType> const& b, std::vector<uint64_t>* player1Choices = nullptr,
                                 std::vector<uint64_t>* player2Choices = nullptr) const;

    // Computes p2Matrix * x + b, reduces the result w.r.t. player 2 choices, and then reduces the result w.r.t. player 1 choices.
    void multiplyAndReduce(Environment const& env, OptimizationDirection player1Dir, OptimizationDirection player2Dir, std::vector<ValueType>& x,
                           std::vector<ValueType> const* b, storm::solver::Multiplier<ValueType> const& multiplier,
                           std::vector<ValueType>& player2ReducedResult, std::vector<ValueType>& player1ReducedResult,
                           std::vector<uint64_t>* player1SchedulerChoices = nullptr, std::vector<uint64_t>* player2SchedulerChoices = nullptr) const;

    // Solves the equation system given by the two choice selections
    void getInducedMatrixVector(std::vector<ValueType>& x, std::vector<ValueType> const& b, std::vector<uint_fast64_t> const& player1Choices,
                                std::vector<uint_fast64_t> const& player2Choices, storm::storage::SparseMatrix<ValueType>& inducedMatrix,
                                std::vector<ValueType>& inducedVector) const;

    // Extracts the choices of the different players for the given solution x.
    // Returns true iff the newly extracted choices yield "better" values then the given choices for one of the players.
    bool extractChoices(Environment const& env, OptimizationDirection player1Dir, OptimizationDirection player2Dir, std::vector<ValueType> const& x,
                        std::vector<ValueType> const& b, std::vector<ValueType>& player2ChoiceValues, std::vector<uint_fast64_t>& player1Choices,
                        std::vector<uint_fast64_t>& player2Choices) const;

    bool valueImproved(OptimizationDirection dir, storm::utility::ConstantsComparator<ValueType> const& comparator, ValueType const& value1,
                       ValueType const& value2) const;

    bool player1RepresentedByMatrix() const;
    storm::storage::SparseMatrix<storm::storage::sparse::state_type> const& getPlayer1Matrix() const;
    std::vector<uint64_t> const& getPlayer1Grouping() const;
    uint64_t getNumberOfPlayer1States() const;
    uint64_t getNumberOfPlayer2States() const;

    // possibly cached data
    mutable std::unique_ptr<storm::solver::Multiplier<ValueType>> multiplierPlayer2Matrix;
    mutable std::unique_ptr<std::vector<ValueType>> auxiliaryP2RowVector;       // player2Matrix.rowCount() entries
    mutable std::unique_ptr<std::vector<ValueType>> auxiliaryP2RowGroupVector;  // player2Matrix.rowGroupCount() entries
    mutable std::unique_ptr<std::vector<ValueType>> auxiliaryP1RowGroupVector;  // player1Matrix.rowGroupCount() entries

    /// The factory used to obtain linear equation solvers.
    std::unique_ptr<LinearEquationSolverFactory<ValueType>> linearEquationSolverFactory;

    // If the solver takes posession of the matrix, we store the moved matrix in this member, so it gets deleted
    // when the solver is destructed.
    std::unique_ptr<std::vector<uint64_t>> localPlayer1Grouping;
    std::unique_ptr<storm::storage::SparseMatrix<storm::storage::sparse::state_type>> localPlayer1Matrix;
    std::unique_ptr<storm::storage::SparseMatrix<ValueType>> localPlayer2Matrix;

    // A reference to the original sparse matrix given to this solver. If the solver takes posession of the matrix
    // the reference refers to localA.
    std::vector<uint64_t> const* player1Grouping;
    storm::storage::SparseMatrix<storm::storage::sparse::state_type> const* player1Matrix;
    storm::storage::SparseMatrix<ValueType> const& player2Matrix;
};
}  // namespace solver
}  // namespace storm
