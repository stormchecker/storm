#include "storm-pomdp-cli/settings/modules/ToParametricSettings.h"

#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingMemento.h"
#include "storm/settings/SettingsManager.h"

#include "storm/exceptions/InvalidArgumentException.h"

namespace storm {
namespace settings {
namespace modules {

const std::string ToParametricSettings::moduleName = "toparametric";
const std::string mecReductionOption = "mecreduction";
const std::string fscmode = "fscmode";
const std::vector<std::string> fscModes = {"standard", "simple-linear", "simple-linear-inverse"};
const std::string transformBinaryOption = "transformbinary";
const std::string transformSimpleOption = "transformsimple";
const std::string allowSimplificationOption = "simplify-pmc";

ToParametricSettings::ToParametricSettings() : ModuleSettings(moduleName) {
    this->addOption(
        storm::settings::OptionBuilder(moduleName, mecReductionOption, false, "Reduces the model size by analyzing maximal end components").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, fscmode, false, "Sets the way the pMC is obtained")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("type", "type name")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(fscModes))
                                         .setDefaultValueString("standard")
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, transformBinaryOption, false, "Transforms the pomdp to a binary pomdp.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, transformSimpleOption, false, "Transforms the pomdp to a binary and simple pomdp.").build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, allowSimplificationOption, false, "After obtaining a pMC, should further simplifications be applied?.")
            .build());
}

bool ToParametricSettings::isMecReductionSet() const {
    return this->getOption(mecReductionOption).getHasOptionBeenSet();
}

std::string ToParametricSettings::getFscApplicationTypeString() const {
    return this->getOption(fscmode).getArgumentByName("type").getValueAsString();
}

bool ToParametricSettings::isTransformBinarySet() const {
    return this->getOption(transformBinaryOption).getHasOptionBeenSet();
}

bool ToParametricSettings::isTransformSimpleSet() const {
    return this->getOption(transformSimpleOption).getHasOptionBeenSet();
}

bool ToParametricSettings::allowPostSimplifications() const {
    return this->getOption(allowSimplificationOption).getHasOptionBeenSet();
}

void ToParametricSettings::finalize() {}

bool ToParametricSettings::check() const {
    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
