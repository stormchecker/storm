#include "storm/environment/solver/NativeSolverEnvironment.h"

#include <limits>

#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {

NativeSolverEnvironment::NativeSolverEnvironment()
    : method(storm::solver::NativeLinearEquationSolverMethod::Jacobi),
      methodSetFromDefault(true),
      maxIterationCount(std::numeric_limits<uint64_t>::max()),
      precision(storm::utility::convertNumber<storm::RationalNumber>(1e-06)),
      considerRelativeTerminationCriterion(true),
      powerMethodMultiplicationStyle(storm::solver::MultiplicationStyle::GaussSeidel),
      sorOmega(storm::utility::convertNumber<storm::RationalNumber>(0.9)),
      symmetricUpdates(false) {
    // Intentionally left empty.
}

NativeSolverEnvironment::~NativeSolverEnvironment() {
    // Intentionally left empty
}

storm::solver::NativeLinearEquationSolverMethod const& NativeSolverEnvironment::getMethod() const {
    return method;
}

bool const& NativeSolverEnvironment::isMethodSetFromDefault() const {
    return methodSetFromDefault;
}

void NativeSolverEnvironment::setMethod(storm::solver::NativeLinearEquationSolverMethod value, bool isSetFromDefault) {
    methodSetFromDefault = isSetFromDefault;
    method = value;
}

uint64_t const& NativeSolverEnvironment::getMaximalNumberOfIterations() const {
    return maxIterationCount;
}

void NativeSolverEnvironment::setMaximalNumberOfIterations(uint64_t value) {
    maxIterationCount = value;
}

storm::RationalNumber const& NativeSolverEnvironment::getPrecision() const {
    return precision;
}

void NativeSolverEnvironment::setPrecision(storm::RationalNumber value) {
    precision = value;
}

bool const& NativeSolverEnvironment::getRelativeTerminationCriterion() const {
    return considerRelativeTerminationCriterion;
}

void NativeSolverEnvironment::setRelativeTerminationCriterion(bool value) {
    considerRelativeTerminationCriterion = value;
}

storm::solver::MultiplicationStyle const& NativeSolverEnvironment::getPowerMethodMultiplicationStyle() const {
    return powerMethodMultiplicationStyle;
}

void NativeSolverEnvironment::setPowerMethodMultiplicationStyle(storm::solver::MultiplicationStyle value) {
    powerMethodMultiplicationStyle = value;
}

storm::RationalNumber const& NativeSolverEnvironment::getSorOmega() const {
    return sorOmega;
}

void NativeSolverEnvironment::setSorOmega(storm::RationalNumber const& value) {
    sorOmega = value;
}

bool NativeSolverEnvironment::isSymmetricUpdatesSet() const {
    return symmetricUpdates;
}

void NativeSolverEnvironment::setSymmetricUpdates(bool value) {
    symmetricUpdates = value;
}

}  // namespace storm
