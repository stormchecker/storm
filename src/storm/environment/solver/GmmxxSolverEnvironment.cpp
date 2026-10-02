#include "storm/environment/solver/GmmxxSolverEnvironment.h"

#include <limits>

#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {

GmmxxSolverEnvironment::GmmxxSolverEnvironment()
    : method(storm::solver::GmmxxLinearEquationSolverMethod::Gmres),
      preconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu),
      restartThreshold(50),
      maxIterationCount(std::numeric_limits<uint64_t>::max()),
      precision(storm::utility::convertNumber<storm::RationalNumber>(1e-06)) {
    // Intentionally left empty.
}

GmmxxSolverEnvironment::~GmmxxSolverEnvironment() {
    // Intentionally left empty
}

storm::solver::GmmxxLinearEquationSolverMethod const& GmmxxSolverEnvironment::getMethod() const {
    return method;
}

void GmmxxSolverEnvironment::setMethod(storm::solver::GmmxxLinearEquationSolverMethod value) {
    method = value;
}

storm::solver::GmmxxLinearEquationSolverPreconditioner const& GmmxxSolverEnvironment::getPreconditioner() const {
    return preconditioner;
}

void GmmxxSolverEnvironment::setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner value) {
    preconditioner = value;
}

uint64_t const& GmmxxSolverEnvironment::getRestartThreshold() const {
    return restartThreshold;
}

void GmmxxSolverEnvironment::setRestartThreshold(uint64_t value) {
    restartThreshold = value;
}

uint64_t const& GmmxxSolverEnvironment::getMaximalNumberOfIterations() const {
    return maxIterationCount;
}

void GmmxxSolverEnvironment::setMaximalNumberOfIterations(uint64_t value) {
    maxIterationCount = value;
}

storm::RationalNumber const& GmmxxSolverEnvironment::getPrecision() const {
    return precision;
}

void GmmxxSolverEnvironment::setPrecision(storm::RationalNumber value) {
    precision = value;
}
}  // namespace storm
