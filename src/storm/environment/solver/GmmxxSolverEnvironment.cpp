#include "storm/environment/solver/GmmxxSolverEnvironment.h"

#include "storm/numbers/constants.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GmmxxEquationSolverSettings.h"
#include "storm/utility/macros.h"

namespace storm {

GmmxxSolverEnvironment::GmmxxSolverEnvironment() {
    auto const& gmmxxSettings = storm::settings::getModule<storm::settings::modules::GmmxxEquationSolverSettings>();

    method = gmmxxSettings.getLinearEquationSystemMethod();
    preconditioner = gmmxxSettings.getPreconditioningMethod();
    restartThreshold = gmmxxSettings.getRestartIterationCount();
    if (gmmxxSettings.isMaximalIterationCountSet()) {
        maxIterationCount = gmmxxSettings.getMaximalIterationCount();
    } else {
        maxIterationCount = std::numeric_limits<uint_fast64_t>::max();
    }
    precision = storm::numbers::convert<storm::RationalNumber>(gmmxxSettings.getPrecision());
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
