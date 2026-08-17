#include "storm/environment/solver/GameSolverEnvironment.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GameSolverSettings.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {

GameSolverEnvironment::GameSolverEnvironment() {
    auto const& gameSettings = storm::settings::getModule<storm::settings::modules::GameSolverSettings>();

    gameMethod = gameSettings.getGameSolvingMethod();
    methodSetFromDefault = gameSettings.isGameSolvingMethodSetFromDefaultValue();
    if (gameSettings.isMaximalIterationCountSet()) {
        maxIterationCount = gameSettings.getMaximalIterationCount();
    } else {
        maxIterationCount = std::numeric_limits<uint_fast64_t>::max();
    }
    precision = storm::utility::convertNumber<storm::RationalNumber>(gameSettings.getPrecision());
    considerRelativeTerminationCriterion =
        gameSettings.getConvergenceCriterion() == storm::settings::modules::GameSolverSettings::ConvergenceCriterion::Relative;
    STORM_LOG_ASSERT(considerRelativeTerminationCriterion ||
                         gameSettings.getConvergenceCriterion() == storm::settings::modules::GameSolverSettings::ConvergenceCriterion::Absolute,
                     "Unknown convergence criterion.");
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

void GameSolverEnvironment::setMethod(storm::solver::GameMethod value) {
    methodSetFromDefault = false;
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
