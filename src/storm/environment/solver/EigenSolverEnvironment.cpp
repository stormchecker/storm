#include "storm/environment/solver/EigenSolverEnvironment.h"

#include <limits>

#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {

EigenSolverEnvironment::EigenSolverEnvironment()
    : method(storm::solver::EigenLinearEquationSolverMethod::Gmres),
      methodSetFromDefault(true),
      preconditioner(storm::solver::EigenLinearEquationSolverPreconditioner::Ilu),
      restartThreshold(50),
      maxIterationCount(std::numeric_limits<uint64_t>::max()),
      precision(storm::utility::convertNumber<storm::RationalNumber>(1e-06)) {
    // Intentionally left empty.
}

EigenSolverEnvironment::~EigenSolverEnvironment() {
    // Intentionally left empty
}

storm::solver::EigenLinearEquationSolverMethod const& EigenSolverEnvironment::getMethod() const {
    return method;
}

bool EigenSolverEnvironment::isMethodSetFromDefault() const {
    return methodSetFromDefault;
}

void EigenSolverEnvironment::setMethod(storm::solver::EigenLinearEquationSolverMethod value, bool isSetFromDefault) {
    methodSetFromDefault = isSetFromDefault;
    method = value;
}

storm::solver::EigenLinearEquationSolverPreconditioner const& EigenSolverEnvironment::getPreconditioner() const {
    return preconditioner;
}

void EigenSolverEnvironment::setPreconditioner(storm::solver::EigenLinearEquationSolverPreconditioner value) {
    preconditioner = value;
}

uint64_t const& EigenSolverEnvironment::getRestartThreshold() const {
    return restartThreshold;
}

void EigenSolverEnvironment::setRestartThreshold(uint64_t value) {
    restartThreshold = value;
}

uint64_t const& EigenSolverEnvironment::getMaximalNumberOfIterations() const {
    return maxIterationCount;
}

void EigenSolverEnvironment::setMaximalNumberOfIterations(uint64_t value) {
    maxIterationCount = value;
}

storm::RationalNumber const& EigenSolverEnvironment::getPrecision() const {
    return precision;
}

void EigenSolverEnvironment::setPrecision(storm::RationalNumber value) {
    precision = value;
}
}  // namespace storm
