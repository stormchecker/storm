#include "storm/environment/solver/MinMaxSolverEnvironment.h"

#include <limits>

#include "storm/environment/solver/MinMaxLpSolverEnvironment.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {

MinMaxSolverEnvironment::MinMaxSolverEnvironment()
    : minMaxMethod(storm::solver::MinMaxMethod::Topological),
      methodSetFromDefault(true),
      maxIterationCount(std::numeric_limits<uint64_t>::max()),
      precision(storm::utility::convertNumber<storm::RationalNumber>(1e-06)),
      considerRelativeTerminationCriterion(true),
      multiplicationStyle(storm::solver::MultiplicationStyle::GaussSeidel),
      forceRequireUnique(false) {
    // Intentionally left empty.
}

MinMaxSolverEnvironment::~MinMaxSolverEnvironment() {
    // Intentionally left empty
}

MinMaxLpSolverEnvironment& MinMaxSolverEnvironment::lp() {
    return lpEnvironment.get();
}

MinMaxLpSolverEnvironment const& MinMaxSolverEnvironment::lp() const {
    return lpEnvironment.get();
}

storm::solver::MinMaxMethod const& MinMaxSolverEnvironment::getMethod() const {
    return minMaxMethod;
}

bool const& MinMaxSolverEnvironment::isMethodSetFromDefault() const {
    return methodSetFromDefault;
}

void MinMaxSolverEnvironment::setMethod(storm::solver::MinMaxMethod value, bool isSetFromDefault) {
    methodSetFromDefault = isSetFromDefault;
    minMaxMethod = value;
}

uint64_t const& MinMaxSolverEnvironment::getMaximalNumberOfIterations() const {
    return maxIterationCount;
}

void MinMaxSolverEnvironment::setMaximalNumberOfIterations(uint64_t value) {
    maxIterationCount = value;
}

storm::RationalNumber const& MinMaxSolverEnvironment::getPrecision() const {
    return precision;
}

void MinMaxSolverEnvironment::setPrecision(storm::RationalNumber value) {
    precision = value;
}

bool const& MinMaxSolverEnvironment::getRelativeTerminationCriterion() const {
    return considerRelativeTerminationCriterion;
}

void MinMaxSolverEnvironment::setRelativeTerminationCriterion(bool value) {
    considerRelativeTerminationCriterion = value;
}

storm::solver::MultiplicationStyle const& MinMaxSolverEnvironment::getMultiplicationStyle() const {
    return multiplicationStyle;
}

void MinMaxSolverEnvironment::setMultiplicationStyle(storm::solver::MultiplicationStyle value) {
    multiplicationStyle = value;
}

bool MinMaxSolverEnvironment::isForceRequireUnique() const {
    return forceRequireUnique;
}

void MinMaxSolverEnvironment::setForceRequireUnique(bool value) {
    forceRequireUnique = value;
}

}  // namespace storm
