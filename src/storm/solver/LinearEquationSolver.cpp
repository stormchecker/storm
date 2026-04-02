#include "storm/solver/LinearEquationSolver.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/exceptions/InvalidEnvironmentException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/solver/AcyclicLinearEquationSolver.h"
#include "storm/solver/EigenLinearEquationSolver.h"
#include "storm/solver/EliminationLinearEquationSolver.h"
#include "storm/solver/GmmxxLinearEquationSolver.h"
#include "storm/solver/NativeLinearEquationSolver.h"
#include "storm/solver/SolverSelectionOptions.h"
#include "storm/solver/TopologicalLinearEquationSolver.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace solver {

template<typename ValueType>
LinearEquationSolver<ValueType>::LinearEquationSolver() : cachingEnabled(false) {
    // Intentionally left empty.
}

template<typename ValueType>
bool LinearEquationSolver<ValueType>::solveEquations(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const& b) const {
    return this->internalSolveEquations(env, x, b);
}

template<typename ValueType>
LinearEquationSolverRequirements LinearEquationSolver<ValueType>::getRequirements(Environment const&) const {
    return LinearEquationSolverRequirements();
}

template<typename ValueType>
void LinearEquationSolver<ValueType>::setCachingEnabled(bool value) const {
    if (cachingEnabled && !value) {
        // caching will be turned off. Hence we clear the cache at this point
        clearCache();
    }
    cachingEnabled = value;
}

template<typename ValueType>
bool LinearEquationSolver<ValueType>::isCachingEnabled() const {
    return cachingEnabled;
}

template<typename ValueType>
void LinearEquationSolver<ValueType>::clearCache() const {
    cachedRowVector.reset();
}

template<typename ValueType>
std::unique_ptr<LinearEquationSolver<ValueType>> LinearEquationSolverFactory<ValueType>::create(Environment const& env,
                                                                                                storm::storage::SparseMatrix<ValueType> const& matrix) const {
    std::unique_ptr<LinearEquationSolver<ValueType>> solver = this->create(env);
    solver->setMatrix(matrix);
    return solver;
}

template<typename ValueType>
std::unique_ptr<LinearEquationSolver<ValueType>> LinearEquationSolverFactory<ValueType>::create(Environment const& env,
                                                                                                storm::storage::SparseMatrix<ValueType>&& matrix) const {
    std::unique_ptr<LinearEquationSolver<ValueType>> solver = this->create(env);
    solver->setMatrix(std::move(matrix));
    return solver;
}

template<typename ValueType>
LinearEquationSolverProblemFormat LinearEquationSolverFactory<ValueType>::getEquationProblemFormat(Environment const& env) const {
    return this->create(env)->getEquationProblemFormat(env);
}

template<typename ValueType>
LinearEquationSolverRequirements LinearEquationSolverFactory<ValueType>::getRequirements(Environment const& env) const {
    return this->create(env)->getRequirements(env);
}

template<typename ValueType>
GeneralLinearEquationSolverFactory<ValueType>::GeneralLinearEquationSolverFactory() {
    // Intentionally left empty.
}

template<>
std::unique_ptr<LinearEquationSolver<storm::RationalNumber>> GeneralLinearEquationSolverFactory<storm::RationalNumber>::create(Environment const& env) const {
    EquationSolverType type = env.solver().getLinearEquationSolverType();

    // Adjust the solver type if it is not supported by this value type
    if (type != EquationSolverType::Eigen && type != EquationSolverType::Topological && type != EquationSolverType::Acyclic &&
        (env.solver().isLinearEquationSolverTypeSetFromDefaultValue() || type == EquationSolverType::Gmmxx)) {
        STORM_LOG_INFO("Selecting '" + toString(EquationSolverType::Eigen) + "' as the linear equation solver since the previously selected one ("
                       << toString(type) << ") does not support exact computations.");
        type = EquationSolverType::Eigen;
    }

    switch (type) {
        case EquationSolverType::Native:
            return std::make_unique<NativeLinearEquationSolver<storm::RationalNumber>>();
        case EquationSolverType::Eigen:
            return std::make_unique<EigenLinearEquationSolver<storm::RationalNumber>>();
        case EquationSolverType::Elimination:
            return std::make_unique<EliminationLinearEquationSolver<storm::RationalNumber>>();
        case EquationSolverType::Topological:
            return std::make_unique<TopologicalLinearEquationSolver<storm::RationalNumber>>();
        case EquationSolverType::Acyclic:
            return std::make_unique<AcyclicLinearEquationSolver<storm::RationalNumber>>();
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidEnvironmentException, "Unknown solver type.");
            return nullptr;
    }
}

template<>
std::unique_ptr<LinearEquationSolver<storm::RationalFunction>> GeneralLinearEquationSolverFactory<storm::RationalFunction>::create(
    Environment const& env) const {
    EquationSolverType type = env.solver().getLinearEquationSolverType();

    // Adjust the solver type if it is not supported by this value type
    if (type == EquationSolverType::Gmmxx || type == EquationSolverType::Native) {
        if (env.solver().isLinearEquationSolverTypeSetFromDefaultValue()) {
            STORM_LOG_INFO("Selecting '" + toString(EquationSolverType::Eigen) + "' as the linear equation solver since the previously selected one ("
                           << toString(type) << ") does not support parametric computations.");
        } else {
            // Be more verbose if the user set the solver explicitly
            STORM_LOG_WARN("The selected linear equation solver (" << toString(type) << ") does not support parametric computations. Falling back to "
                                                                   << toString(EquationSolverType::Eigen) << ".");
        }
        type = EquationSolverType::Eigen;
    }

    switch (type) {
        case EquationSolverType::Eigen:
            return std::make_unique<EigenLinearEquationSolver<storm::RationalFunction>>();
        case EquationSolverType::Elimination:
            return std::make_unique<EliminationLinearEquationSolver<storm::RationalFunction>>();
        case EquationSolverType::Topological:
            return std::make_unique<TopologicalLinearEquationSolver<storm::RationalFunction>>();
        case EquationSolverType::Acyclic:
            return std::make_unique<AcyclicLinearEquationSolver<storm::RationalFunction>>();
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidEnvironmentException, "Unknown solver type.");
            return nullptr;
    }
}

template<typename ValueType>
std::unique_ptr<LinearEquationSolver<ValueType>> GeneralLinearEquationSolverFactory<ValueType>::create(Environment const& env) const {
    EquationSolverType type = env.solver().getLinearEquationSolverType();

    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We have not implemented interval-based linear equation solvers");
    }

    // Adjust the solver type if none was specified and we want sound/exact computations
    if (env.solver().isForceExact() && type != EquationSolverType::Native && type != EquationSolverType::Eigen && type != EquationSolverType::Elimination &&
        type != EquationSolverType::Topological && type != EquationSolverType::Acyclic) {
        if (env.solver().isLinearEquationSolverTypeSetFromDefaultValue()) {
            type = EquationSolverType::Eigen;
            STORM_LOG_INFO(
                "Selecting '" + toString(type) +
                "' as the linear equation solver to guarantee exact results. If you want to override this, please explicitly specify a different solver.");
        } else {
            STORM_LOG_WARN("The selected solver does not yield exact results.");
        }
    } else if (env.solver().isForceSoundness() && type != EquationSolverType::Native && type != EquationSolverType::Eigen &&
               type != EquationSolverType::Elimination && type != EquationSolverType::Topological && type != EquationSolverType::Acyclic) {
        if (env.solver().isLinearEquationSolverTypeSetFromDefaultValue()) {
            type = EquationSolverType::Native;
            STORM_LOG_INFO(
                "Selecting '" + toString(type) +
                "' as the linear equation solver to guarantee sound results. If you want to override this, please explicitly specify a different solver.");
        } else {
            STORM_LOG_WARN("The selected solver does not yield sound results.");
        }
    }

    switch (type) {
        case EquationSolverType::Gmmxx:
            return std::make_unique<GmmxxLinearEquationSolver<ValueType>>();
        case EquationSolverType::Native:
            return std::make_unique<NativeLinearEquationSolver<ValueType>>();
        case EquationSolverType::Eigen:
            return std::make_unique<EigenLinearEquationSolver<ValueType>>();
        case EquationSolverType::Elimination:
            return std::make_unique<EliminationLinearEquationSolver<ValueType>>();
        case EquationSolverType::Topological:
            return std::make_unique<TopologicalLinearEquationSolver<ValueType>>();
        case EquationSolverType::Acyclic:
            return std::make_unique<AcyclicLinearEquationSolver<ValueType>>();
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidEnvironmentException, "Unknown solver type.");
            return nullptr;
    }
}

template<typename ValueType>
std::unique_ptr<LinearEquationSolverFactory<ValueType>> GeneralLinearEquationSolverFactory<ValueType>::clone() const {
    return std::make_unique<GeneralLinearEquationSolverFactory<ValueType>>(*this);
}

template class LinearEquationSolver<double>;
template class LinearEquationSolverFactory<double>;
template class GeneralLinearEquationSolverFactory<double>;

template class LinearEquationSolver<storm::RationalNumber>;
template class LinearEquationSolverFactory<storm::RationalNumber>;
template class GeneralLinearEquationSolverFactory<storm::RationalNumber>;

template class LinearEquationSolver<storm::RationalFunction>;
template class LinearEquationSolverFactory<storm::RationalFunction>;
template class GeneralLinearEquationSolverFactory<storm::RationalFunction>;

}  // namespace solver
}  // namespace storm
