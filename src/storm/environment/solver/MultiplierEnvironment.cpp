#include "MultiplierEnvironment.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/MultiplierSettings.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {

MultiplierEnvironment::MultiplierEnvironment() {
    auto const& multiplierSettings = storm::settings::getModule<storm::settings::modules::MultiplierSettings>();
    type = multiplierSettings.getMultiplierType();
    typeSetFromDefault = multiplierSettings.isMultiplierTypeSetFromDefaultValue();
}

MultiplierEnvironment::~MultiplierEnvironment() {
    // Intentionally left empty
}

storm::solver::MultiplierType const& MultiplierEnvironment::getType() const {
    return type;
}

bool const& MultiplierEnvironment::isTypeSetFromDefault() const {
    return typeSetFromDefault;
}

void MultiplierEnvironment::setType(storm::solver::MultiplierType value, bool isSetFromDefault) {
    type = value;
    typeSetFromDefault = isSetFromDefault;
}

}  // namespace storm
