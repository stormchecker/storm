#include "SymbolicGameSolver.h"

#include "storm/environment/solver/GameSolverEnvironment.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

template<storm::dd::DdType Type, typename ValueType>
SymbolicGameSolver<Type, ValueType>::SymbolicGameSolver(
    storm::dd::Add<Type, ValueType> const& A, storm::dd::Bdd<Type> const& allRows, storm::dd::Bdd<Type> const& illegalPlayer1Mask,
    storm::dd::Bdd<Type> const& illegalPlayer2Mask, std::set<storm::expressions::Variable> const& rowMetaVariables,
    std::set<storm::expressions::Variable> const& columnMetaVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
    std::set<storm::expressions::Variable> const& player1Variables, std::set<storm::expressions::Variable> const& player2Variables)
    : A(A),
      allRows(allRows),
      illegalPlayer1Mask(
          illegalPlayer1Mask.ite(A.getDdManager().getConstant(storm::utility::infinity<ValueType>()), A.getDdManager().template getAddZero<ValueType>())),
      illegalPlayer2Mask(
          illegalPlayer2Mask.ite(A.getDdManager().getConstant(storm::utility::infinity<ValueType>()), A.getDdManager().template getAddZero<ValueType>())),
      rowMetaVariables(rowMetaVariables),
      columnMetaVariables(columnMetaVariables),
      rowColumnMetaVariablePairs(rowColumnMetaVariablePairs),
      player1Variables(player1Variables),
      player2Variables(player2Variables),
      generatePlayer1Strategy(false),
      generatePlayer2Strategy(false) {
    // Get the default settings
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Add<Type, ValueType> SymbolicGameSolver<Type, ValueType>::solveGame(Environment const& env, OptimizationDirection player1Goal,
                                                                               OptimizationDirection player2Goal, storm::dd::Add<Type, ValueType> const& x,
                                                                               storm::dd::Add<Type, ValueType> const& b,
                                                                               boost::optional<storm::dd::Bdd<Type>> const& basePlayer1Strategy,
                                                                               boost::optional<storm::dd::Bdd<Type>> const& basePlayer2Strategy) {
    STORM_LOG_WARN_COND(env.solver().game().getMethod() == GameMethod::ValueIteration,
                        "Switching game method to Value iteration since the selected method is not supported by this solver.");

    // Set up the environment.
    ValueType precision = storm::utility::convertNumber<ValueType>(env.solver().game().getPrecision());
    bool relative = env.solver().game().getRelativeTerminationCriterion();
    uint64_t maxIter = env.solver().game().getMaximalNumberOfIterations();
    storm::dd::Add<Type, ValueType> xCopy = x;
    uint_fast64_t iterations = 0;
    bool converged = false;

    // Prepare some data storage in case we need to generate strategies.
    if (generatePlayer1Strategy) {
        if (basePlayer1Strategy) {
            player1Strategy = basePlayer1Strategy.get();
        } else {
            player1Strategy = A.getDdManager().getBddZero();
        }
    }
    boost::optional<storm::dd::Add<Type, ValueType>> previousPlayer2Values;
    if (generatePlayer2Strategy) {
        if (basePlayer2Strategy && player2Goal == storm::OptimizationDirection::Maximize) {
            player2Strategy = basePlayer2Strategy.get();

            // If we are required to generate a player 2 strategy based on another one that is not the zero strategy,
            // we need to determine the values, because only then we can update the strategy only if necessary.
            previousPlayer2Values = (player2Strategy.get().template toAdd<ValueType>() *
                                     (this->A.multiplyMatrix(x.swapVariables(this->rowColumnMetaVariablePairs), this->columnMetaVariables) + b))
                                        .sumAbstract(this->player2Variables);
        } else {
            player2Strategy = A.getDdManager().getBddZero();
            previousPlayer2Values = A.getDdManager().template getAddZero<ValueType>();
        }
    }

    do {
        // Compute tmp = A * x + b.
        storm::dd::Add<Type, ValueType> xCopyAsColumn = xCopy.swapVariables(this->rowColumnMetaVariablePairs);
        storm::dd::Add<Type, ValueType> tmp = this->A.multiplyMatrix(xCopyAsColumn, this->columnMetaVariables);
        tmp += b;

        // Now abstract from player 2 and player 1 variables.
        if (player2Goal == storm::OptimizationDirection::Maximize) {
            storm::dd::Add<Type, ValueType> newValues = tmp.maxAbstract(this->player2Variables);

            if (generatePlayer2Strategy) {
                // Update only the choices that strictly improved the value.
                storm::dd::Bdd<Type> maxChoices = tmp.maxAbstractRepresentative(this->player2Variables);
                player2Strategy.get() = newValues.greater(previousPlayer2Values.get()).ite(maxChoices, player2Strategy.get());
                previousPlayer2Values = newValues;
            }

            tmp = newValues;
        } else {
            tmp = (tmp + illegalPlayer2Mask);
            storm::dd::Add<Type, ValueType> newValues = tmp.minAbstract(this->player2Variables);

            if (generatePlayer2Strategy) {
                player2Strategy = tmp.minAbstractRepresentative(this->player2Variables);
            }

            tmp = newValues;
        }

        if (player1Goal == storm::OptimizationDirection::Maximize) {
            storm::dd::Add<Type, ValueType> newValues = tmp.maxAbstract(this->player1Variables);

            if (generatePlayer1Strategy) {
                // Update only the choices that strictly improved the value.
                storm::dd::Bdd<Type> maxChoices = tmp.maxAbstractRepresentative(this->player1Variables);
                player1Strategy = newValues.greater(xCopy).ite(maxChoices, player1Strategy.get());
            }

            tmp = newValues;
        } else {
            tmp = (tmp + illegalPlayer1Mask);
            storm::dd::Add<Type, ValueType> newValues = tmp.minAbstract(this->player1Variables);

            if (generatePlayer1Strategy) {
                player1Strategy = tmp.minAbstractRepresentative(this->player1Variables);
            }

            tmp = newValues;
        }

        // Now check if the process already converged within our precision.
        converged = xCopy.equalModuloPrecision(tmp, precision, relative);

        // If the method did not converge yet, we prepare the x vector for the next iteration.
        if (!converged) {
            xCopy = tmp;
        }

        ++iterations;
    } while (!converged && iterations < maxIter);
    STORM_LOG_INFO("Numerically solving the game took " << iterations << " iterations.");

    return xCopy;
}

template<storm::dd::DdType Type, typename ValueType>
void SymbolicGameSolver<Type, ValueType>::setGeneratePlayer1Strategy(bool value) {
    generatePlayer1Strategy = value;
}

template<storm::dd::DdType Type, typename ValueType>
void SymbolicGameSolver<Type, ValueType>::setGeneratePlayer2Strategy(bool value) {
    generatePlayer2Strategy = value;
}

template<storm::dd::DdType Type, typename ValueType>
void SymbolicGameSolver<Type, ValueType>::setGeneratePlayersStrategies(bool value) {
    setGeneratePlayer1Strategy(value);
    setGeneratePlayer2Strategy(value);
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& SymbolicGameSolver<Type, ValueType>::getPlayer1Strategy() const {
    STORM_LOG_THROW(player1Strategy, storm::exceptions::IllegalFunctionCallException, "Cannot retrieve player 1 strategy because none was generated.");
    return player1Strategy.get();
}

template<storm::dd::DdType Type, typename ValueType>
storm::dd::Bdd<Type> const& SymbolicGameSolver<Type, ValueType>::getPlayer2Strategy() const {
    STORM_LOG_THROW(player2Strategy, storm::exceptions::IllegalFunctionCallException, "Cannot retrieve player 2 strategy because none was generated.");
    return player2Strategy.get();
}

template<storm::dd::DdType Type, typename ValueType>
std::unique_ptr<storm::solver::SymbolicGameSolver<Type, ValueType>> SymbolicGameSolverFactory<Type, ValueType>::create(
    storm::dd::Add<Type, ValueType> const& A, storm::dd::Bdd<Type> const& allRows, storm::dd::Bdd<Type> const& illegalPlayer1Mask,
    storm::dd::Bdd<Type> const& illegalPlayer2Mask, std::set<storm::expressions::Variable> const& rowMetaVariables,
    std::set<storm::expressions::Variable> const& columnMetaVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs,
    std::set<storm::expressions::Variable> const& player1Variables, std::set<storm::expressions::Variable> const& player2Variables) const {
    return std::unique_ptr<storm::solver::SymbolicGameSolver<Type, ValueType>>(
        new storm::solver::SymbolicGameSolver<Type, ValueType>(A, allRows, illegalPlayer1Mask, illegalPlayer2Mask, rowMetaVariables, columnMetaVariables,
                                                               rowColumnMetaVariablePairs, player1Variables, player2Variables));
}

template class SymbolicGameSolver<storm::dd::DdType::CUDD, double>;
template class SymbolicGameSolver<storm::dd::DdType::Sylvan, double>;
template class SymbolicGameSolver<storm::dd::DdType::Sylvan, storm::RationalNumber>;

template class SymbolicGameSolverFactory<storm::dd::DdType::CUDD, double>;
template class SymbolicGameSolverFactory<storm::dd::DdType::Sylvan, double>;
template class SymbolicGameSolverFactory<storm::dd::DdType::Sylvan, storm::RationalNumber>;

}  // namespace solver
}  // namespace storm
