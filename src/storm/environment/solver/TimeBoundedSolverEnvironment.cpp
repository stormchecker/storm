#include "storm/environment/solver/TimeBoundedSolverEnvironment.h"

#include "storm/numbers/constants.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/TimeBoundedSolverSettings.h"
#include "storm/utility/macros.h"

namespace storm {

TimeBoundedSolverEnvironment::TimeBoundedSolverEnvironment() {
    auto const& tbSettings = storm::settings::getModule<storm::settings::modules::TimeBoundedSolverSettings>();
    maMethod = tbSettings.getMaMethod();
    maMethodSetFromDefault = tbSettings.isMaMethodSetFromDefaultValue();
    precision = storm::numbers::convert<storm::RationalNumber>(tbSettings.getPrecision());
    relative = tbSettings.isRelativePrecision();
    unifPlusKappa = storm::numbers::convert<storm::RationalNumber>(tbSettings.getUnifPlusKappa());
}

TimeBoundedSolverEnvironment::~TimeBoundedSolverEnvironment() {
    // Intentionally left empty
}

storm::solver::MaBoundedReachabilityMethod const& TimeBoundedSolverEnvironment::getMaMethod() const {
    return maMethod;
}

bool const& TimeBoundedSolverEnvironment::isMaMethodSetFromDefault() const {
    return maMethodSetFromDefault;
}

void TimeBoundedSolverEnvironment::setMaMethod(storm::solver::MaBoundedReachabilityMethod value, bool isSetFromDefault) {
    maMethod = value;
    maMethodSetFromDefault = isSetFromDefault;
}

storm::RationalNumber const& TimeBoundedSolverEnvironment::getPrecision() const {
    return precision;
}

void TimeBoundedSolverEnvironment::setPrecision(storm::RationalNumber value) {
    precision = value;
}

bool const& TimeBoundedSolverEnvironment::getRelativeTerminationCriterion() const {
    return relative;
}

void TimeBoundedSolverEnvironment::setRelativeTerminationCriterion(bool value) {
    relative = value;
}

storm::RationalNumber const& TimeBoundedSolverEnvironment::getUnifPlusKappa() const {
    return unifPlusKappa;
}

void TimeBoundedSolverEnvironment::setUnifPlusKappa(storm::RationalNumber value) {
    unifPlusKappa = value;
}

}  // namespace storm
