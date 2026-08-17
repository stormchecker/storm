#include "storm-gspn/settings/modules/GSPNSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"

#include "storm/exceptions/InvalidSettingsException.h"

namespace storm {
namespace settings {
namespace modules {
const std::string GSPNSettings::moduleName = "gspn";

const std::string GSPNSettings::gspnFileOptionName = "gspnfile";
const std::string GSPNSettings::gspnFileOptionShortName = "gspn";
const std::string GSPNSettings::capacitiesFileOptionName = "capacitiesfile";
const std::string GSPNSettings::capacitiesFileOptionShortName = "capacities";
const std::string GSPNSettings::capacityOptionName = "capacity";
const std::string GSPNSettings::constantsOptionName = "constants";
const std::string GSPNSettings::constantsOptionShortName = "const";

GSPNSettings::GSPNSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, gspnFileOptionName, false, "Parses the GSPN.")
                        .setShortName(gspnFileOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "path to file")
                                         .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, capacitiesFileOptionName, false, "Capacaties as invariants for places.")
                        .setShortName(capacitiesFileOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "path to file")
                                         .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, capacityOptionName, false, "Global capacity as invariants for all places.")
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("value", "capacity")
                                         .addValidatorUnsignedInteger(ArgumentValidatorFactory::createUnsignedGreaterValidator(0))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, constantsOptionName, false, "Specifies the constant replacements to use.")
                        .setShortName(constantsOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument(
                                         "values", "A comma separated list of constants and their value, e.g. a=1,b=2,c=3.")
                                         .setDefaultValueString("")
                                         .build())
                        .build());
}

bool GSPNSettings::isGspnFileSet() const {
    return this->getOption(gspnFileOptionName).getHasOptionBeenSet();
}

std::string GSPNSettings::getGspnFilename() const {
    return this->getOption(gspnFileOptionName).getArgumentByName("filename").getValueAsString();
}

bool GSPNSettings::isCapacitiesFileSet() const {
    return this->getOption(capacitiesFileOptionName).getHasOptionBeenSet();
}

std::string GSPNSettings::getCapacitiesFilename() const {
    return this->getOption(capacitiesFileOptionName).getArgumentByName("filename").getValueAsString();
}

bool GSPNSettings::isCapacitySet() const {
    return this->getOption(capacityOptionName).getHasOptionBeenSet();
}

uint64_t GSPNSettings::getCapacity() const {
    return this->getOption(capacityOptionName).getArgumentByName("value").getValueAsUnsignedInteger();
}

bool GSPNSettings::isConstantsSet() const {
    return this->getOption(constantsOptionName).getHasOptionBeenSet();
}

std::string GSPNSettings::getConstantDefinitionString() const {
    return this->getOption(constantsOptionName).getArgumentByName("values").getValueAsString();
}

void GSPNSettings::finalize() {}

bool GSPNSettings::check() const {
    if (!isGspnFileSet()) {
        if (isCapacitiesFileSet()) {
            return false;
        }
    }

    if (isCapacitiesFileSet() && isCapacitySet()) {
        STORM_LOG_ERROR("Conflicting settings: Capacity file AND capacity was set.");
        return false;
    }
    return true;
}
}  // namespace modules
}  // namespace settings
}  // namespace storm
