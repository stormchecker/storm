#include "storm/settings/modules/GeneralSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"
#include "storm/solver/SolverSelectionOptions.h"

#include "storm/storage/dd/DdType.h"

#include "storm/exceptions/InvalidSettingsException.h"

namespace storm {
namespace settings {
namespace modules {

const std::string GeneralSettings::moduleName = "general";
const std::string GeneralSettings::helpOptionName = "help";
const std::string GeneralSettings::helpOptionShortName = "h";
const std::string GeneralSettings::versionOptionName = "version";
const std::string GeneralSettings::verboseOptionName = "verbose";
const std::string GeneralSettings::verboseOptionShortName = "v";
const std::string GeneralSettings::showProgressOptionName = "progress";
const std::string GeneralSettings::precisionOptionName = "precision";
const std::string GeneralSettings::precisionOptionShortName = "eps";
const std::string GeneralSettings::configOptionName = "config";
const std::string GeneralSettings::configOptionShortName = "c";
const std::string GeneralSettings::bisimulationOptionName = "bisimulation";
const std::string GeneralSettings::bisimulationOptionShortName = "bisim";
const std::string GeneralSettings::parametricOptionName = "parametric";
const std::string GeneralSettings::exactOptionName = "exact";
const std::string GeneralSettings::soundOptionName = "sound";

GeneralSettings::GeneralSettings() : ModuleSettings(moduleName) {
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
    this->addOption(storm::settings::OptionBuilder(moduleName, showProgressOptionName, false,
                                                   "Sets when additional information (if available) about the progress is printed.")
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument(
                                         "delay", "The delay to wait (in seconds) between emitting information (0 means never print progress).")
                                         .setDefaultValueUnsignedInteger(5)
                                         .makeOptional()
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, precisionOptionName, false, "The internally used precision.")
                        .setShortName(precisionOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("value", "The precision to use.")
                                         .setDefaultValueDouble(1e-06)
                                         .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorExcluding(0.0, 1.0))
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, configOptionName, false,
                                       "If given, this file will be read and parsed for additional configuration settings.")
            .setShortName(configOptionShortName)
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the file from which to read the configuration.")
                             .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                             .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, bisimulationOptionName, false, "Sets whether to perform bisimulation minimization.")
                        .setShortName(bisimulationOptionShortName)
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, parametricOptionName, false, "Sets whether to enable parametric model checking.").setIsAdvanced().build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, exactOptionName, false, "Sets whether to enable exact model checking.")
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("valuetype", "The kind of datatype used to represent numeric values")
                             .setDefaultValueString("rationals")
                             .makeOptional()
                             .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator({"rationals", "floats"}))
                             .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, soundOptionName, false, "Sets whether to force sound model checking.").build());
}

bool GeneralSettings::isHelpSet() const {
    return this->getOption(helpOptionName).getHasOptionBeenSet();
}

bool GeneralSettings::isVersionSet() const {
    return this->getOption(versionOptionName).getHasOptionBeenSet();
}

std::string GeneralSettings::getHelpFilterExpression() const {
    return this->getOption(helpOptionName).getArgumentByName("filter").getValueAsString();
}

bool GeneralSettings::isVerboseSet() const {
    return this->getOption(verboseOptionName).getHasOptionBeenSet();
}

bool GeneralSettings::isShowProgressSet() const {
    return this->getOption(showProgressOptionName).getHasOptionBeenSet();
}

uint64_t GeneralSettings::getShowProgressDelay() const {
    return this->getOption(showProgressOptionName).getArgumentByName("delay").getValueAsUnsignedInteger();
}

bool GeneralSettings::isPrecisionSet() const {
    return this->getOption(precisionOptionName).getHasOptionBeenSet();
}

void GeneralSettings::setPrecision(std::string precision) {
    this->getOption(precisionOptionName).getArgumentByName("value").setFromStringValue(precision);
}
double GeneralSettings::getPrecision() const {
    return this->getOption(precisionOptionName).getArgumentByName("value").getValueAsDouble();
}

bool GeneralSettings::isConfigSet() const {
    return this->getOption(configOptionName).getHasOptionBeenSet();
}

std::string GeneralSettings::getConfigFilename() const {
    return this->getOption(configOptionName).getArgumentByName("filename").getValueAsString();
}

bool GeneralSettings::isBisimulationSet() const {
    return this->getOption(bisimulationOptionName).getHasOptionBeenSet();
}

bool GeneralSettings::isParametricSet() const {
    return this->getOption(parametricOptionName).getHasOptionBeenSet();
}

bool GeneralSettings::isExactSet() const {
    return this->getOption(exactOptionName).getHasOptionBeenSet() &&
           this->getOption(exactOptionName).getArgumentByName("valuetype").getValueAsString() == "rationals";
}

bool GeneralSettings::isExactFinitePrecisionSet() const {
    return this->getOption(exactOptionName).getHasOptionBeenSet() &&
           this->getOption(exactOptionName).getArgumentByName("valuetype").getValueAsString() == "floats";
}

bool GeneralSettings::isSoundSet() const {
    return this->getOption(soundOptionName).getHasOptionBeenSet();
}

void GeneralSettings::finalize() {
    // Intentionally left empty.
}

bool GeneralSettings::check() const {
    STORM_LOG_WARN_COND(!this->getOption(precisionOptionName).getHasOptionBeenSetWithModulePrefix(),
                        "Setting the precision option with module prefix does not effect all solvers. Consider setting --"
                            << precisionOptionName << " instead of --" << moduleName << ":" << precisionOptionName << ".");
    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
