#include "ConversionGeneralSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"

namespace storm {
namespace settings {
namespace modules {

const std::string ConversionGeneralSettings::moduleName = "general";
const std::string ConversionGeneralSettings::helpOptionName = "help";
const std::string ConversionGeneralSettings::helpOptionShortName = "h";
const std::string ConversionGeneralSettings::versionOptionName = "version";
const std::string ConversionGeneralSettings::verboseOptionName = "verbose";
const std::string ConversionGeneralSettings::verboseOptionShortName = "v";
const std::string ConversionGeneralSettings::debugOptionName = "debug";
const std::string ConversionGeneralSettings::traceOptionName = "trace";
const std::string ConversionGeneralSettings::configOptionName = "config";
const std::string ConversionGeneralSettings::configOptionShortName = "c";

ConversionGeneralSettings::ConversionGeneralSettings() : ModuleSettings(moduleName) {
    this->addOption(
        storm::settings::OptionBuilder(moduleName, helpOptionName, false, "Shows available options, arguments and descriptions.")
            .setShortName(helpOptionShortName)
            .addArgument(
                storm::settings::ArgumentBuilder::createStringArgument(
                    "filter",
                    "'frequent' for frequently used options, 'all' for the complete help, or a regular expression to show help for all matching entities.")
                    .setDefaultValueString("frequent")
                    .makeOptional()
                    .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, versionOptionName, false, "Prints the version information.").build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, verboseOptionName, false, "Enables more verbose output.").setShortName(verboseOptionShortName).build());
    this->addOption(storm::settings::OptionBuilder(moduleName, debugOptionName, false, "Enables verbose and debug output.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, traceOptionName, false, "Enables verbose and debug and trace output.").build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, configOptionName, false,
                                       "If given, this file will be read and parsed for additional configuration settings.")
            .setShortName(configOptionShortName)
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the file from which to read the configuration.")
                             .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                             .build())
            .build());
}

bool ConversionGeneralSettings::isHelpSet() const {
    return this->getOption(helpOptionName).getHasOptionBeenSet();
}

bool ConversionGeneralSettings::isVersionSet() const {
    return this->getOption(versionOptionName).getHasOptionBeenSet();
}

std::string ConversionGeneralSettings::getHelpFilterExpression() const {
    return this->getOption(helpOptionName).getArgumentByName("filter").getValueAsString();
}

bool ConversionGeneralSettings::isVerboseSet() const {
    return this->getOption(verboseOptionName).getHasOptionBeenSet();
}

bool ConversionGeneralSettings::isDebugOutputSet() const {
    return this->getOption(debugOptionName).getHasOptionBeenSet();
}

bool ConversionGeneralSettings::isTraceOutputSet() const {
    return this->getOption(traceOptionName).getHasOptionBeenSet();
}

bool ConversionGeneralSettings::isConfigSet() const {
    return this->getOption(configOptionName).getHasOptionBeenSet();
}

std::string ConversionGeneralSettings::getConfigFilename() const {
    return this->getOption(configOptionName).getArgumentByName("filename").getValueAsString();
}

void ConversionGeneralSettings::finalize() {
    // Intentionally left empty.
}

bool ConversionGeneralSettings::check() const {
    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
