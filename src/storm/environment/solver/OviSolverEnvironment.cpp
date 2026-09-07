#include "OviSolverEnvironment.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/OviSolverSettings.h"
#include "storm/utility/constants.h"

namespace storm {

OviSolverEnvironment::OviSolverEnvironment() {
    auto const& oviSettings = storm::settings::getModule<storm::settings::modules::OviSolverSettings>();
    if (oviSettings.hasUpperBoundGuessingFactorBeenSet()) {
        upperBoundGuessingFactor = storm::utility::convertNumber<storm::RationalNumber>(oviSettings.getUpperBoundGuessingFactor());
    }
}

std::optional<storm::RationalNumber> const& OviSolverEnvironment::getUpperBoundGuessingFactor() const {
    return upperBoundGuessingFactor;
}

}  // namespace storm
