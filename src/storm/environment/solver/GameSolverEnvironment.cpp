#include "storm/environment/solver/GameSolverEnvironment.h"

#include <limits>

#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {

GameSolverEnvironment::GameSolverEnvironment()
    : gameMethod(storm::solver::GameMethod::ValueIteration),
      methodSetFromDefault(true),
      maxIterationCount(std::numeric_limits<uint64_t>::max()),
      precision(storm::utility::convertNumber<storm::RationalNumber>(1e-06)),
      considerRelativeTerminationCriterion(true) {
    // Intentionally left empty.
}

GameSolverEnvironment::~GameSolverEnvironment() {
    // Intentionally left empty
}

storm::solver::GameMethod const& GameSolverEnvironment::getMethod() const {
    return gameMethod;
}

bool const& GameSolverEnvironment::isMethodSetFromDefault() const {
    return methodSetFromDefault;
}

void GameSolverEnvironment::setMethod(storm::solver::GameMethod value, bool isSetFromDefault) {
    methodSetFromDefault = isSetFromDefault;
    gameMethod = value;
}

uint64_t const& GameSolverEnvironment::getMaximalNumberOfIterations() const {
    return maxIterationCount;
}

void GameSolverEnvironment::setMaximalNumberOfIterations(uint64_t value) {
    maxIterationCount = value;
}

storm::RationalNumber const& GameSolverEnvironment::getPrecision() const {
    return precision;
}

void GameSolverEnvironment::setPrecision(storm::RationalNumber value) {
    precision = value;
}

bool const& GameSolverEnvironment::getRelativeTerminationCriterion() const {
    return considerRelativeTerminationCriterion;
}

void GameSolverEnvironment::setRelativeTerminationCriterion(bool value) {
    considerRelativeTerminationCriterion = value;
}

}  // namespace storm
