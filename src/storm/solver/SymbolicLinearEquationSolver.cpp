#include "storm/solver/SymbolicLinearEquationSolver.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/exceptions/InvalidEnvironmentException.h"
#include "storm/solver/SolverSelectionOptions.h"
#include "storm/solver/SymbolicEliminationLinearEquationSolver.h"
#include "storm/solver/SymbolicNativeLinearEquationSolver.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

template<storm::dd::DdType DdType, typename ValueType>
SymbolicLinearEquationSolver<DdType, ValueType>::SymbolicLinearEquationSolver() {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
SymbolicLinearEquationSolver<DdType, ValueType>::SymbolicLinearEquationSolver(
    storm::dd::Add<DdType, ValueType> const& A, storm::dd::Bdd<DdType> const& allRows, std::set<storm::expressions::Variable> const& rowMetaVariables,
    std::set<storm::expressions::Variable> const& columnMetaVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs)
    : SymbolicLinearEquationSolver(allRows, rowMetaVariables, columnMetaVariables, rowColumnMetaVariablePairs) {
    this->setMatrix(A);
}

template<storm::dd::DdType DdType, typename ValueType>
SymbolicLinearEquationSolver<DdType, ValueType>::SymbolicLinearEquationSolver(
    storm::dd::Bdd<DdType> const& allRows, std::set<storm::expressions::Variable> const& rowMetaVariables,
    std::set<storm::expressions::Variable> const& columnMetaVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs)
    : SymbolicEquationSolver<DdType, ValueType>(allRows),
      rowMetaVariables(rowMetaVariables),
      columnMetaVariables(columnMetaVariables),
      rowColumnMetaVariablePairs(rowColumnMetaVariablePairs) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> SymbolicLinearEquationSolver<DdType, ValueType>::multiply(storm::dd::Add<DdType, ValueType> const& x,
                                                                                            storm::dd::Add<DdType, ValueType> const* b, uint_fast64_t n) const {
    storm::dd::Add<DdType, ValueType> xCopy = x;

    // Perform matrix-vector multiplication while the bound is met.
    for (uint_fast64_t i = 0; i < n; ++i) {
        xCopy = xCopy.swapVariables(this->rowColumnMetaVariablePairs);
        xCopy = this->A.multiplyMatrix(xCopy, this->columnMetaVariables);
        if (b != nullptr) {
            xCopy += *b;
        }
    }

    return xCopy;
}

template<storm::dd::DdType DdType, typename ValueType>
LinearEquationSolverProblemFormat SymbolicLinearEquationSolver<DdType, ValueType>::getEquationProblemFormat(Environment const& env) const {
    return LinearEquationSolverProblemFormat::EquationSystem;
}

template<storm::dd::DdType DdType, typename ValueType>
LinearEquationSolverRequirements SymbolicLinearEquationSolver<DdType, ValueType>::getRequirements(Environment const& env) const {
    // Return empty requirements by default.
    return LinearEquationSolverRequirements();
}

template<storm::dd::DdType DdType, typename ValueType>
void SymbolicLinearEquationSolver<DdType, ValueType>::setMatrix(storm::dd::Add<DdType, ValueType> const& newA) {
    this->A = newA;
}

template<storm::dd::DdType DdType, typename ValueType>
void SymbolicLinearEquationSolver<DdType, ValueType>::setData(
    storm::dd::Bdd<DdType> const& allRows, std::set<storm::expressions::Variable> const& rowMetaVariables,
    std::set<storm::expressions::Variable> const& columnMetaVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs) {
    this->setAllRows(allRows);
    this->rowMetaVariables = rowMetaVariables;
    this->columnMetaVariables = columnMetaVariables;
    this->rowColumnMetaVariablePairs = rowColumnMetaVariablePairs;
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>> SymbolicLinearEquationSolverFactory<DdType, ValueType>::create(
    Environment const& env, storm::dd::Add<DdType, ValueType> const& A, storm::dd::Bdd<DdType> const& allRows,
    std::set<storm::expressions::Variable> const& rowMetaVariables, std::set<storm::expressions::Variable> const& columnMetaVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs) const {
    std::unique_ptr<storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>> solver =
        this->create(env, allRows, rowMetaVariables, columnMetaVariables, rowColumnMetaVariablePairs);
    solver->setMatrix(A);
    return solver;
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>> SymbolicLinearEquationSolverFactory<DdType, ValueType>::create(
    Environment const& env, storm::dd::Bdd<DdType> const& allRows, std::set<storm::expressions::Variable> const& rowMetaVariables,
    std::set<storm::expressions::Variable> const& columnMetaVariables,
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs) const {
    std::unique_ptr<storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>> solver = this->create(env);
    solver->setData(allRows, rowMetaVariables, columnMetaVariables, rowColumnMetaVariablePairs);
    return solver;
}

template<storm::dd::DdType DdType, typename ValueType>
LinearEquationSolverProblemFormat SymbolicLinearEquationSolverFactory<DdType, ValueType>::getEquationProblemFormat(Environment const& env) const {
    return this->create(env)->getEquationProblemFormat(env);
}

template<storm::dd::DdType DdType, typename ValueType>
LinearEquationSolverRequirements SymbolicLinearEquationSolverFactory<DdType, ValueType>::getRequirements(Environment const& env) const {
    return this->create(env)->getRequirements(env);
}

template<>
std::unique_ptr<storm::solver::SymbolicLinearEquationSolver<storm::dd::DdType::Sylvan, storm::RationalFunction>>
GeneralSymbolicLinearEquationSolverFactory<storm::dd::DdType::Sylvan, storm::RationalFunction>::create(Environment const& env) const {
    EquationSolverType type = env.solver().getLinearEquationSolverType();

    // Adjust the solver type if it is not supported in the Dd engine with rational functions
    if (type != EquationSolverType::Elimination) {
        type = EquationSolverType::Elimination;
        STORM_LOG_INFO("The selected equation solver is not available in the parametric dd engine. Falling back to " << toString(type) << " solver.");
    }

    switch (type) {
        case EquationSolverType::Elimination:
            return std::make_unique<SymbolicEliminationLinearEquationSolver<storm::dd::DdType::Sylvan, storm::RationalFunction>>();
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidEnvironmentException, "Unknown solver type.");
            return nullptr;
    }
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<storm::solver::SymbolicLinearEquationSolver<DdType, ValueType>> GeneralSymbolicLinearEquationSolverFactory<DdType, ValueType>::create(
    Environment const& env) const {
    EquationSolverType type = env.solver().getLinearEquationSolverType();

    // Adjust the solver type if it is not supported in the Dd engine
    if (type != EquationSolverType::Native && type != EquationSolverType::Elimination) {
        type = EquationSolverType::Native;
        STORM_LOG_INFO("The selected equation solver is not available in the dd engine. Falling back to " << toString(type) << " solver.");
    }

    switch (type) {
        case EquationSolverType::Native:
            return std::make_unique<SymbolicNativeLinearEquationSolver<DdType, ValueType>>();
        case EquationSolverType::Elimination:
            return std::make_unique<SymbolicEliminationLinearEquationSolver<DdType, ValueType>>();
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidEnvironmentException, "Unknown solver type.");
            return nullptr;
    }
}

template class SymbolicLinearEquationSolver<storm::dd::DdType::CUDD, double>;
template class SymbolicLinearEquationSolver<storm::dd::DdType::CUDD, storm::RationalNumber>;

template class SymbolicLinearEquationSolver<storm::dd::DdType::Sylvan, double>;
template class SymbolicLinearEquationSolver<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class SymbolicLinearEquationSolver<storm::dd::DdType::Sylvan, storm::RationalFunction>;

template class SymbolicLinearEquationSolverFactory<storm::dd::DdType::CUDD, double>;
template class SymbolicLinearEquationSolverFactory<storm::dd::DdType::CUDD, storm::RationalNumber>;
template class SymbolicLinearEquationSolverFactory<storm::dd::DdType::Sylvan, double>;
template class SymbolicLinearEquationSolverFactory<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class SymbolicLinearEquationSolverFactory<storm::dd::DdType::Sylvan, storm::RationalFunction>;

template class GeneralSymbolicLinearEquationSolverFactory<storm::dd::DdType::CUDD, double>;
template class GeneralSymbolicLinearEquationSolverFactory<storm::dd::DdType::CUDD, storm::RationalNumber>;
template class GeneralSymbolicLinearEquationSolverFactory<storm::dd::DdType::Sylvan, double>;
template class GeneralSymbolicLinearEquationSolverFactory<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class GeneralSymbolicLinearEquationSolverFactory<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace solver
}  // namespace storm
