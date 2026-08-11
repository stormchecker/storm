#include "storm/environment/solver/GlpkSolverEnvironment.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GlpkSettings.h"

namespace storm {

GlpkSolverEnvironment::GlpkSolverEnvironment() {
    auto const& glpkSettings = storm::settings::getModule<storm::settings::modules::GlpkSettings>();
    integerTolerance = glpkSettings.getIntegerTolerance();
    milpPresolverEnabled = glpkSettings.isMILPPresolverEnabled();
    output = glpkSettings.isOutputSet();
}

GlpkSolverEnvironment::~GlpkSolverEnvironment() {
    // Intentionally left empty.
}

double GlpkSolverEnvironment::getIntegerTolerance() const {
    return integerTolerance;
}

void GlpkSolverEnvironment::setIntegerTolerance(double value) {
    integerTolerance = value;
}

bool GlpkSolverEnvironment::isMILPPresolverEnabled() const {
    return milpPresolverEnabled;
}

void GlpkSolverEnvironment::setMILPPresolverEnabled(bool value) {
    milpPresolverEnabled = value;
}

bool GlpkSolverEnvironment::isOutputSet() const {
    return output;
}

void GlpkSolverEnvironment::setOutput(bool value) {
    output = value;
}

}  // namespace storm
