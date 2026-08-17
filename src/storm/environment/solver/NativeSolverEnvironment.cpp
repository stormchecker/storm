#include "storm/environment/solver/NativeSolverEnvironment.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/NativeEquationSolverSettings.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {

NativeSolverEnvironment::NativeSolverEnvironment() {
    auto const& nativeSettings = storm::settings::getModule<storm::settings::modules::NativeEquationSolverSettings>();

    method = nativeSettings.getLinearEquationSystemMethod();
    methodSetFromDefault = nativeSettings.isLinearEquationSystemTechniqueSetFromDefaultValue();
    if (nativeSettings.isMaximalIterationCountSet()) {
        maxIterationCount = nativeSettings.getMaximalIterationCount();
    } else {
        maxIterationCount = std::numeric_limits<uint_fast64_t>::max();
    }
    precision = storm::utility::convertNumber<storm::RationalNumber>(nativeSettings.getPrecision());
    considerRelativeTerminationCriterion =
        nativeSettings.getConvergenceCriterion() == storm::settings::modules::NativeEquationSolverSettings::ConvergenceCriterion::Relative;
    STORM_LOG_ASSERT(considerRelativeTerminationCriterion ||
                         nativeSettings.getConvergenceCriterion() == storm::settings::modules::NativeEquationSolverSettings::ConvergenceCriterion::Absolute,
                     "Unknown convergence criterion.");
    powerMethodMultiplicationStyle = nativeSettings.getPowerMethodMultiplicationStyle();
    sorOmega = storm::utility::convertNumber<storm::RationalNumber>(nativeSettings.getOmega());
    symmetricUpdates = nativeSettings.isForceIntervalIterationSymmetricUpdatesSet();
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

void NativeSolverEnvironment::setMethod(storm::solver::NativeLinearEquationSolverMethod value) {
    methodSetFromDefault = false;
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
