#include "SolverRequirement.h"

#include <vector>

#include "storm/utility/vector.h"

namespace storm {
namespace solver {
SolverRequirement::SolverRequirement() : enabled(false), critical(false) {
    // Intentionally left empty
}

SolverRequirement::operator bool() const {
    return enabled;
}

void SolverRequirement::enable(bool critical) {
    this->enabled = true;
    this->critical = critical;
}

void SolverRequirement::clear() {
    enabled = false;
    critical = false;
}

bool SolverRequirement::isCritical() const {
    return this->critical;
}

}  // namespace solver
}  // namespace storm
