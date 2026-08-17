#include "GmmxxLinearEquationSolver.h"

#include <utility>

#include "storm/adapters/GmmxxAdapter.h"
#include "storm/environment/solver/GmmxxSolverEnvironment.h"
#include "storm/exceptions/AbortException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/constants.h"
#include "storm/utility/vector.h"

namespace storm {
namespace solver {

template<typename ValueType>
GmmxxLinearEquationSolver<ValueType>::GmmxxLinearEquationSolver() {
    // Intentionally left empty.
}

template<typename ValueType>
GmmxxLinearEquationSolver<ValueType>::GmmxxLinearEquationSolver(storm::storage::SparseMatrix<ValueType> const& A) {
    this->setMatrix(A);
}

template<typename ValueType>
GmmxxLinearEquationSolver<ValueType>::GmmxxLinearEquationSolver(storm::storage::SparseMatrix<ValueType>&& A) {
    this->setMatrix(std::move(A));
}

template<typename ValueType>
void GmmxxLinearEquationSolver<ValueType>::setMatrix(storm::storage::SparseMatrix<ValueType> const& A) {
#ifdef STORM_HAVE_GMM
    gmmxxA = storm::adapters::GmmxxAdapter<ValueType>::toGmmxxSparseMatrix(A);
    clearCache();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GMM. Yet, a method was called that requires this support.");
#endif
}

template<typename ValueType>
void GmmxxLinearEquationSolver<ValueType>::setMatrix(storm::storage::SparseMatrix<ValueType>&& A) {
#ifdef STORM_HAVE_GMM
    gmmxxA = storm::adapters::GmmxxAdapter<ValueType>::toGmmxxSparseMatrix(A);
    clearCache();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GMM. Yet, a method was called that requires this support.");
#endif
}

template<typename ValueType>
GmmxxLinearEquationSolverMethod GmmxxLinearEquationSolver<ValueType>::getMethod(Environment const& env) const {
    STORM_LOG_ERROR_COND(!env.solver().isForceSoundness(), "This linear equation solver does not support sound computations. Using unsound methods now...");
    STORM_LOG_ERROR_COND(!env.solver().isForceExact(), "This linear equation solver does not support exact computations. Using unsound methods now...");
    return env.solver().gmmxx().getMethod();
}

template<typename ValueType>
bool GmmxxLinearEquationSolver<ValueType>::internalSolveEquations(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const& b) const {
#ifdef STORM_HAVE_GMM
    auto method = getMethod(env);
    auto preconditioner = env.solver().gmmxx().getPreconditioner();

    STORM_LOG_INFO("Solving linear equation system (" << x.size() << " rows) with Gmmxx linear equation solver with method '" << toString(method)
                                                      << "' and preconditioner '" << toString(preconditioner) << "'.");

    if (method == GmmxxLinearEquationSolverMethod::Bicgstab || method == GmmxxLinearEquationSolverMethod::Qmr ||
        method == GmmxxLinearEquationSolverMethod::Gmres) {
        // Make sure that the requested preconditioner is available
        if (preconditioner == GmmxxLinearEquationSolverPreconditioner::Ilu && !iluPreconditioner) {
            iluPreconditioner = std::make_unique<gmm::ilu_precond<gmm::csr_matrix<ValueType>>>(*gmmxxA);
        } else if (preconditioner == GmmxxLinearEquationSolverPreconditioner::Diagonal) {
            diagonalPreconditioner = std::make_unique<gmm::diagonal_precond<gmm::csr_matrix<ValueType>>>(*gmmxxA);
        }

        // Prepare an iteration object that determines the accuracy and the maximum number of iterations.
        gmm::size_type maxIter = std::numeric_limits<gmm::size_type>::max();
        if (env.solver().gmmxx().getMaximalNumberOfIterations() < static_cast<uint64_t>(maxIter)) {
            maxIter = env.solver().gmmxx().getMaximalNumberOfIterations();
        }
        gmm::iteration iter(storm::utility::convertNumber<ValueType>(env.solver().gmmxx().getPrecision()), 0, maxIter);
        iter.set_callback([](const gmm::iteration& iteration) -> void {
            STORM_LOG_THROW(!storm::utility::resources::isTerminate(), storm::exceptions::AbortException,
                            "Gmm++ (externally) aborted after " << iteration.get_iteration() << " iterations.");
        });

        // We use throwing an exception to abort within Gmm++
        try {
            // Invoke gmm with the corresponding settings
            if (method == GmmxxLinearEquationSolverMethod::Bicgstab) {
                if (preconditioner == GmmxxLinearEquationSolverPreconditioner::Ilu) {
                    gmm::bicgstab(*gmmxxA, x, b, *iluPreconditioner, iter);
                } else if (preconditioner == GmmxxLinearEquationSolverPreconditioner::Diagonal) {
                    gmm::bicgstab(*gmmxxA, x, b, *diagonalPreconditioner, iter);
                } else if (preconditioner == GmmxxLinearEquationSolverPreconditioner::None) {
                    gmm::bicgstab(*gmmxxA, x, b, gmm::identity_matrix(), iter);
                }
            } else if (method == GmmxxLinearEquationSolverMethod::Qmr) {
                if (preconditioner == GmmxxLinearEquationSolverPreconditioner::Ilu) {
                    gmm::qmr(*gmmxxA, x, b, *iluPreconditioner, iter);
                } else if (preconditioner == GmmxxLinearEquationSolverPreconditioner::Diagonal) {
                    gmm::qmr(*gmmxxA, x, b, *diagonalPreconditioner, iter);
                } else if (preconditioner == GmmxxLinearEquationSolverPreconditioner::None) {
                    gmm::qmr(*gmmxxA, x, b, gmm::identity_matrix(), iter);
                }
            } else if (method == GmmxxLinearEquationSolverMethod::Gmres) {
                if (preconditioner == GmmxxLinearEquationSolverPreconditioner::Ilu) {
                    gmm::gmres(*gmmxxA, x, b, *iluPreconditioner, env.solver().gmmxx().getRestartThreshold(), iter);
                } else if (preconditioner == GmmxxLinearEquationSolverPreconditioner::Diagonal) {
                    gmm::gmres(*gmmxxA, x, b, *diagonalPreconditioner, env.solver().gmmxx().getRestartThreshold(), iter);
                } else if (preconditioner == GmmxxLinearEquationSolverPreconditioner::None) {
                    gmm::gmres(*gmmxxA, x, b, gmm::identity_matrix(), env.solver().gmmxx().getRestartThreshold(), iter);
                }
            }
        } catch (storm::exceptions::AbortException const&) {
            // Do nothing
        }

        if (!this->isCachingEnabled()) {
            clearCache();
        }

        // Make sure that all results conform to the bounds.
        storm::utility::vector::clip(x, this->lowerBound, this->upperBound);

        // Check if the solver converged and issue a warning otherwise.
        if (iter.converged()) {
            STORM_LOG_INFO("Iterative solver converged after " << iter.get_iteration() << " iteration(s).");
            return true;
        } else {
            STORM_LOG_WARN("Iterative solver did not converge within " << iter.get_iteration() << " iteration(s).");
            return false;
        }
    }

    STORM_LOG_ERROR("Selected method is not available");
    return false;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GMM. Yet, a method was called that requires this support.");
#endif
}

template<typename ValueType>
LinearEquationSolverProblemFormat GmmxxLinearEquationSolver<ValueType>::getEquationProblemFormat(Environment const& env) const {
    return LinearEquationSolverProblemFormat::EquationSystem;
}

template<typename ValueType>
void GmmxxLinearEquationSolver<ValueType>::clearCache() const {
#ifdef STORM_HAVE_GMM
    iluPreconditioner.reset();
    diagonalPreconditioner.reset();
    LinearEquationSolver<ValueType>::clearCache();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GMM. Yet, a method was called that requires this support.");
#endif
}

template<typename ValueType>
uint64_t GmmxxLinearEquationSolver<ValueType>::getMatrixRowCount() const {
#ifdef STORM_HAVE_GMM
    return gmmxxA->nr;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GMM. Yet, a method was called that requires this support.");
#endif
}

template<typename ValueType>
uint64_t GmmxxLinearEquationSolver<ValueType>::getMatrixColumnCount() const {
#ifdef STORM_HAVE_GMM
    return gmmxxA->nc;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of storm was compiled without support for GMM. Yet, a method was called that requires this support.");
#endif
}

template<typename ValueType>
std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> GmmxxLinearEquationSolverFactory<ValueType>::create(Environment const& env) const {
    return std::make_unique<storm::solver::GmmxxLinearEquationSolver<ValueType>>();
}

template<typename ValueType>
std::unique_ptr<LinearEquationSolverFactory<ValueType>> GmmxxLinearEquationSolverFactory<ValueType>::clone() const {
    return std::make_unique<GmmxxLinearEquationSolverFactory<ValueType>>(*this);
}

// Explicitly instantiate the solver.
template class GmmxxLinearEquationSolver<double>;
template class GmmxxLinearEquationSolverFactory<double>;

}  // namespace solver
}  // namespace storm
