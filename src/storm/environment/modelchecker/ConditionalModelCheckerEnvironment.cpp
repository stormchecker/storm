#include "storm/environment/modelchecker/ConditionalModelCheckerEnvironment.h"

#include "storm/adapters/RationalNumberForward.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/ConditionalSettings.h"
#include "storm/utility/constants.h"

namespace storm {

ConditionalModelCheckerEnvironment::ConditionalModelCheckerEnvironment() {
    auto const& mcSettings = storm::settings::getModule<storm::settings::modules::ConditionalSettings>();
    algorithm = mcSettings.getConditionalAlgorithmSetting();
    precision = storm::utility::convertNumber<storm::RationalNumber>(mcSettings.getConditionalPrecision());
    relative = !mcSettings.isConditionalPrecisionAbsolute();
    precisionSetFromDefault = mcSettings.isConditionalPrecisionSetFromDefaultValue();
}

ConditionalModelCheckerEnvironment::~ConditionalModelCheckerEnvironment() {
    // Intentionally left empty
}

ConditionalAlgorithmSetting ConditionalModelCheckerEnvironment::getAlgorithm() const {
    return algorithm;
}

void ConditionalModelCheckerEnvironment::setAlgorithm(ConditionalAlgorithmSetting value) {
    algorithm = value;
}

storm::RationalNumber ConditionalModelCheckerEnvironment::getPrecision() const {
    return precision;
}

void ConditionalModelCheckerEnvironment::setPrecision(storm::RationalNumber const& value, bool setFromDefault) {
    precision = value;
    precisionSetFromDefault = setFromDefault;
}

bool ConditionalModelCheckerEnvironment::isPrecisionSetFromDefault() const {
    return precisionSetFromDefault;
}

bool ConditionalModelCheckerEnvironment::isRelativePrecision() const {
    return relative;
}

void ConditionalModelCheckerEnvironment::setRelativePrecision(bool value) {
    relative = value;
}

}  // namespace storm
