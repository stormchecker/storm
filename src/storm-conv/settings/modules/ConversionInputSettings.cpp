#include "ConversionInputSettings.h"

#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/parser/CSVParser.h"
#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"

namespace storm {
namespace settings {
namespace modules {

const std::string ConversionInputSettings::moduleName = "input";
const std::string ConversionInputSettings::propertyOptionName = "prop";
const std::string ConversionInputSettings::propertyOptionShortName = "prop";
const std::string ConversionInputSettings::constantsOptionName = "constants";
const std::string ConversionInputSettings::constantsOptionShortName = "const";
const std::string ConversionInputSettings::prismInputOptionName = "prism";
const std::string ConversionInputSettings::prismCompatibilityOptionName = "prismcompat";
const std::string ConversionInputSettings::prismCompatibilityOptionShortName = "pc";
const std::string ConversionInputSettings::janiInputOptionName = "jani";
const std::string ConversionInputSettings::janiPropertyOptionName = "janiproperty";
const std::string ConversionInputSettings::janiPropertyOptionShortName = "jprop";

ConversionInputSettings::ConversionInputSettings() : ModuleSettings(moduleName) {
    // General
    this->addOption(
        storm::settings::OptionBuilder(moduleName, propertyOptionName, false, "Specifies the properties to be checked on the model.")
            .setShortName(propertyOptionShortName)
            .addArgument(
                storm::settings::ArgumentBuilder::createStringArgument("property or filename", "The formula or the file containing the formulas.").build())
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filter", "The names of the properties to check.")
                             .setDefaultValueString("all")
                             .makeOptional()
                             .build())
            .build());
    this->addOption(
        storm::settings::OptionBuilder(
            moduleName, constantsOptionName, false,
            "Specifies the constant replacements to use in symbolic models. Note that this requires the model to be given as an symbolic model (e.g., via --" +
                prismInputOptionName + ").")
            .setShortName(constantsOptionShortName)
            .addArgument(
                storm::settings::ArgumentBuilder::createStringArgument("values", "A comma separated list of constants and their value, e.g. a=1,b=2,c=3.")
                    .setDefaultValueString("")
                    .build())
            .build());

    // Prism related
    this->addOption(
        storm::settings::OptionBuilder(moduleName, prismInputOptionName, false, "Parses the model given in the PRISM format.")
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the file from which to read the PRISM input.")
                             .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                             .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, prismCompatibilityOptionName, false,
                                                   "Enables PRISM compatibility. This may be necessary to process some PRISM models.")
                        .setShortName(prismCompatibilityOptionShortName)
                        .build());

    // Jani related
    this->addOption(
        storm::settings::OptionBuilder(moduleName, janiInputOptionName, false, "Parses the model given in the JANI format.")
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the file from which to read the JANI input.")
                             .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                             .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, janiPropertyOptionName, false,
                                                   "Specifies the properties from the jani model (given by --" + janiInputOptionName + ") to be checked.")
                        .setShortName(janiPropertyOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("values", "A comma separated list of properties to be checked")
                                         .setDefaultValueString("")
                                         .makeOptional()
                                         .build())
                        .build());
}

bool ConversionInputSettings::isPrismInputSet() const {
    return this->getOption(prismInputOptionName).getHasOptionBeenSet();
}

std::string ConversionInputSettings::getPrismInputFilename() const {
    return this->getOption(prismInputOptionName).getArgumentByName("filename").getValueAsString();
}

bool ConversionInputSettings::isJaniInputSet() const {
    return this->getOption(janiInputOptionName).getHasOptionBeenSet();
}

std::string ConversionInputSettings::getJaniInputFilename() const {
    return this->getOption(janiInputOptionName).getArgumentByName("filename").getValueAsString();
}

bool ConversionInputSettings::isPrismCompatibilityEnabled() const {
    return this->getOption(prismCompatibilityOptionName).getHasOptionBeenSet();
}

bool ConversionInputSettings::isConstantsSet() const {
    return this->getOption(constantsOptionName).getHasOptionBeenSet();
}

std::string ConversionInputSettings::getConstantDefinitionString() const {
    return this->getOption(constantsOptionName).getArgumentByName("values").getValueAsString();
}

bool ConversionInputSettings::isPropertyInputSet() const {
    return this->getOption(propertyOptionName).getHasOptionBeenSet();
}

std::string ConversionInputSettings::getPropertyInput() const {
    return this->getOption(propertyOptionName).getArgumentByName("property or filename").getValueAsString();
}

std::string ConversionInputSettings::getPropertyInputFilter() const {
    return this->getOption(propertyOptionName).getArgumentByName("filter").getValueAsString();
}

bool ConversionInputSettings::isJaniPropertiesSet() const {
    return this->getOption(janiPropertyOptionName).getHasOptionBeenSet();
}

bool ConversionInputSettings::areJaniPropertiesSelected() const {
    return this->getOption(janiPropertyOptionName).getHasOptionBeenSet() &&
           (this->getOption(janiPropertyOptionName).getArgumentByName("values").getValueAsString() != "");
}

std::vector<std::string> ConversionInputSettings::getSelectedJaniProperties() const {
    return storm::parser::parseCommaSeperatedValues(this->getOption(janiPropertyOptionName).getArgumentByName("values").getValueAsString());
}

void ConversionInputSettings::finalize() {
    // Intentionally left empty.
}

bool ConversionInputSettings::check() const {
    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
