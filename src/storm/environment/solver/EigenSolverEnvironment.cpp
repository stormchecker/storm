#include "storm/environment/solver/EigenSolverEnvironment.h"

#include "storm/numbers/constants.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/EigenEquationSolverSettings.h"
#include "storm/utility/macros.h"

namespace storm {

EigenSolverEnvironment::EigenSolverEnvironment() {
    auto const& eigenSettings = storm::settings::getModule<storm::settings::modules::EigenEquationSolverSettings>();

    method = eigenSettings.getLinearEquationSystemMethod();
    methodSetFromDefault = eigenSettings.isLinearEquationSystemMethodSetFromDefault();
    preconditioner = eigenSettings.getPreconditioningMethod();
    restartThreshold = eigenSettings.getRestartIterationCount();
    if (eigenSettings.isMaximalIterationCountSet()) {
        maxIterationCount = eigenSettings.getMaximalIterationCount();
    } else {
        maxIterationCount = std::numeric_limits<uint_fast64_t>::max();
    }
    precision = storm::numbers::convert<storm::RationalNumber>(eigenSettings.getPrecision());
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

void EigenSolverEnvironment::setMethod(storm::solver::EigenLinearEquationSolverMethod value) {
    methodSetFromDefault = false;
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
