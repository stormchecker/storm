#include "storm/settings/modules/CuddSettings.h"

#include "storm/settings/SettingsManager.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

namespace storm {
namespace settings {
namespace modules {

const std::string CuddSettings::moduleName = "cudd";
const std::string CuddSettings::precisionOptionName = "precision";
const std::string CuddSettings::maximalMemoryOptionName = "maxmem";

CuddSettings::CuddSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, precisionOptionName, true, "Sets the precision used by Cudd.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(
                                         "value", "The precision up to which to constants are considered to be different.")
                                         .setDefaultValueDouble(1e-15)
                                         .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorIncluding(0.0, 1.0))
                                         .build())
                        .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, maximalMemoryOptionName, true, "Sets the upper bound of memory available to Cudd in MB.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("value", "The memory available to Cudd (0 means unlimited).")
                             .setDefaultValueUnsignedInteger(4096)
                             .build())
            .build());
}

double CuddSettings::getConstantPrecision() const {
    return this->getOption(precisionOptionName).getArgumentByName("value").getValueAsDouble();
}

uint_fast64_t CuddSettings::getMaximalMemory() const {
    return this->getOption(maximalMemoryOptionName).getArgumentByName("value").getValueAsUnsignedInteger();
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
