#include "storm/environment/solver/OviSolverEnvironment.h"

#include "storm/numbers/constants.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/OviSolverSettings.h"

namespace storm {

OviSolverEnvironment::OviSolverEnvironment() {
    auto const& oviSettings = storm::settings::getModule<storm::settings::modules::OviSolverSettings>();
    if (oviSettings.hasUpperBoundGuessingFactorBeenSet()) {
        upperBoundGuessingFactor = storm::numbers::convert<storm::RationalNumber>(oviSettings.getUpperBoundGuessingFactor());
    }
}

std::optional<storm::RationalNumber> const& OviSolverEnvironment::getUpperBoundGuessingFactor() const {
    return upperBoundGuessingFactor;
}

}  // namespace storm
