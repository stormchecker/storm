#include "storm/environment/solver/OviSolverEnvironment.h"

#include <optional>

namespace storm {

OviSolverEnvironment::OviSolverEnvironment() : upperBoundGuessingFactor(std::nullopt) {
    // Intentionally left empty.
}

std::optional<storm::RationalNumber> const& OviSolverEnvironment::getUpperBoundGuessingFactor() const {
    return upperBoundGuessingFactor;
}

void OviSolverEnvironment::setUpperBoundGuessingFactor(storm::RationalNumber value) {
    upperBoundGuessingFactor = value;
}

void OviSolverEnvironment::unsetUpperBoundGuessingFactor() {
    upperBoundGuessingFactor = std::nullopt;
}

}  // namespace storm
