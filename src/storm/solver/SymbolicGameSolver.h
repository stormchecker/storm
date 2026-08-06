#pragma once

#include <set>
#include <vector>

#include "storm/solver/OptimizationDirection.h"

#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {

class Environment;

namespace solver {

/*!
 * An interface that represents an abstract symbolic game solver.
 */
template<storm::dd::DdType Type, typename ValueType = double>
class SymbolicGameSolver {
   public:
    /*!
     * Constructs a symbolic game solver with the given meta variable sets and pairs.
     *
     * @param A The matrix defining the coefficients of the game.
     * @param allRows A BDD characterizing all rows of the equation system.
     * @param illegalPlayer1Mask A BDD characterizing the illegal choices of player 1.
     * @param illegalPlayer2Mask A BDD characterizing the illegal choices of player 2.
     * @param rowMetaVariables The meta variables used to encode the rows of the matrix.
     * @param columnMetaVariables The meta variables used to encode the columns of the matrix.
     * @param rowColumnMetaVariablePairs The pairs of row meta variables and the corresponding column meta
     * variables.
     * @param player1Variables The meta variables used to encode the player 1 choices.
     * @param player2Variables The meta variables used to encode the player 2 choices.
     */
    SymbolicGameSolver(storm::dd::Add<Type, ValueType> const& A, storm::dd::Bdd<Type> const& allRows, storm::dd::Bdd<Type> const& illegalPlayer1Mask,
                       storm::dd::Bdd<Type> const& illegalPlayer2Mask, std::set<storm::expressions::Variable> const& rowMetaVariables,
                       std::set<storm::expressions::Variable> const& columnMetaVariables,
                       std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
                       std::set<storm::expressions::Variable> const& player1Variables, std::set<storm::expressions::Variable> const& player2Variables);

    virtual ~SymbolicGameSolver() = default;

    /*!
     * Solves the equation system defined by the game matrix. Note that the game matrix has to be given upon
     * construction time of the solver object.
     *
     * @param player1Goal Sets whether player 1 wants to minimize or maximize.
     * @param player2Goal Sets whether player 2 wants to minimize or maximize.
     * @param x The initial guess of the solution.
     * @param b The vector to add after matrix-vector multiplication.
     * @param basePlayer1Strategy If the vector x is not the zero vector and a strategy for player 1 is generated,
     * then this strategy can be used to generate a strategy that only differs from the given one if it has to.
     * @param basePlayer2Strategy If the vector x is not the zero vector and a strategy for player 1 is generated,
     * then this strategy can be used to generate a strategy that only differs from the given one if it has to.
     * @return The solution vector.
     */
    virtual storm::dd::Add<Type, ValueType> solveGame(Environment const& env, OptimizationDirection player1Goal, OptimizationDirection player2Goal,
                                                      storm::dd::Add<Type, ValueType> const& x, storm::dd::Add<Type, ValueType> const& b,
                                                      boost::optional<storm::dd::Bdd<Type>> const& basePlayer1Strategy = boost::none,
                                                      boost::optional<storm::dd::Bdd<Type>> const& basePlayer2Strategy = boost::none);

    // Setters that enable the generation of the players' strategies.
    void setGeneratePlayer1Strategy(bool value);
    void setGeneratePlayer2Strategy(bool value);
    void setGeneratePlayersStrategies(bool value);

    // Getters to retrieve the players' strategies. Only legal if they were generated.
    storm::dd::Bdd<Type> const& getPlayer1Strategy() const;
    storm::dd::Bdd<Type> const& getPlayer2Strategy() const;

   protected:
    // The matrix defining the coefficients of the linear equation system.
    storm::dd::Add<Type, ValueType> A;

    // A BDD characterizing all rows of the equation system.
    storm::dd::Bdd<Type> allRows;

    // An ADD that can be used to compensate for the illegal choices of player 1.
    storm::dd::Add<Type, ValueType> illegalPlayer1Mask;

    // An ADD that can be used to compensate for the illegal choices of player 2.
    storm::dd::Add<Type, ValueType> illegalPlayer2Mask;

    // The row variables.
    std::set<storm::expressions::Variable> rowMetaVariables;

    // The column variables.
    std::set<storm::expressions::Variable> columnMetaVariables;

    // The pairs of meta variables used for renaming.
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> rowColumnMetaVariablePairs;

    // The player 1 variables.
    std::set<storm::expressions::Variable> player1Variables;

    // The player 2 variables.
    std::set<storm::expressions::Variable> player2Variables;

    // A flag indicating whether a player 1 is to be generated.
    bool generatePlayer1Strategy;

    // A player 1 strategy if one was generated.
    boost::optional<storm::dd::Bdd<Type>> player1Strategy;

    // A flag indicating whether a player 2 is to be generated.
    bool generatePlayer2Strategy;

    // A player 1 strategy if one was generated.
    boost::optional<storm::dd::Bdd<Type>> player2Strategy;
};

template<storm::dd::DdType Type, typename ValueType>
class SymbolicGameSolverFactory {
   public:
    virtual ~SymbolicGameSolverFactory() = default;

    virtual std::unique_ptr<storm::solver::SymbolicGameSolver<Type, ValueType>> create(
        storm::dd::Add<Type, ValueType> const& A, storm::dd::Bdd<Type> const& allRows, storm::dd::Bdd<Type> const& illegalPlayer1Mask,
        storm::dd::Bdd<Type> const& illegalPlayer2Mask, std::set<storm::expressions::Variable> const& rowMetaVariables,
        std::set<storm::expressions::Variable> const& columnMetaVariables,
        std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
        std::set<storm::expressions::Variable> const& player1Variables, std::set<storm::expressions::Variable> const& player2Variables) const;
};

}  // namespace solver
}  // namespace storm
