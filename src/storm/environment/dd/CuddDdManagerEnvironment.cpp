#include "storm/environment/dd/CuddDdManagerEnvironment.h"

#include "storm/utility/macros.h"

namespace storm {

CuddDdManagerEnvironment::CuddDdManagerEnvironment()
    : constantPrecision(1e-15), maximalMemory(4096), reorderingEnabled(false), reorderingTechnique(storm::dd::CuddReorderingTechnique::GroupSift) {
    // Intentionally left empty.
}

CuddDdManagerEnvironment::~CuddDdManagerEnvironment() {
    // Intentionally left empty
}

double CuddDdManagerEnvironment::getConstantPrecision() const {
    return constantPrecision;
}

void CuddDdManagerEnvironment::setConstantPrecision(double value) {
    constantPrecision = value;
}

uint64_t CuddDdManagerEnvironment::getMaximalMemory() const {
    return maximalMemory;
}

void CuddDdManagerEnvironment::setMaximalMemory(uint64_t value) {
    maximalMemory = value;
}

bool CuddDdManagerEnvironment::isReorderingEnabled() const {
    return reorderingEnabled;
}

void CuddDdManagerEnvironment::setReorderingEnabled(bool value) {
    reorderingEnabled = value;
}

storm::dd::CuddReorderingTechnique CuddDdManagerEnvironment::getReorderingTechnique() const {
    return reorderingTechnique;
}

void CuddDdManagerEnvironment::setReorderingTechnique(storm::dd::CuddReorderingTechnique value) {
    reorderingTechnique = value;
}

}  // namespace storm
