#include "storm/environment/solver/GlpkSolverEnvironment.h"

namespace storm {

GlpkSolverEnvironment::GlpkSolverEnvironment() : integerTolerance(1e-06), milpPresolverEnabled(true), output(false) {
    // Intentionally left empty.
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
