#include "storm-pars/settings/modules/ParametricSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string ParametricSettings::moduleName = "parametric";
const std::string modeOptionName = "mode";
const std::string exportResultOptionName = "resultfile";
const std::string transformContinuousOptionName = "transformcontinuous";
const std::string transformContinuousShortOptionName = "tc";
const std::string useMonotonicityName = "use-monotonicity";
const std::string bigStepEnabledName = "big-step";
const std::string linearToSimpleEnabledName = "linear-to-simple";

ParametricSettings::ParametricSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> modes = {"feasibility", "verification", "monotonicity", "sampling", "solutionfunction", "partitioning"};
    this->addOption(storm::settings::OptionBuilder(moduleName, modeOptionName, false, "What type of parametric analysis do you want to do?")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("mode", "What to do?")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(modes))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, exportResultOptionName, false, "A path to a file where the parametric result should be saved.")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("path", "the location.")
                                         .addValidatorString(ArgumentValidatorFactory::createWritableFileValidator())
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, transformContinuousOptionName, false,
                                                   "Sets whether to transform a continuous time input model to a discrete time model.")
                        .setShortName(transformContinuousShortOptionName)
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, useMonotonicityName, false, "If set, monotonicity will be used.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, bigStepEnabledName, false, "Enables big step transitions.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, linearToSimpleEnabledName, false,
                                                   "Converts linear (constant * parameter) transitions to simple (only constant or parameter) transitions.")
                        .build());
}

bool ParametricSettings::exportResultToFile() const {
    return this->getOption(exportResultOptionName).getHasOptionBeenSet();
}

std::string ParametricSettings::exportResultPath() const {
    return this->getOption(exportResultOptionName).getArgumentByName("path").getValueAsString();
}

bool ParametricSettings::transformContinuousModel() const {
    return this->getOption(transformContinuousOptionName).getHasOptionBeenSet();
}

bool ParametricSettings::isUseMonotonicitySet() const {
    return this->getOption(useMonotonicityName).getHasOptionBeenSet();
}

bool ParametricSettings::hasOperationModeBeenSet() const {
    return this->getOption(modeOptionName).getHasOptionBeenSet();
}

pars::utility::ParametricMode ParametricSettings::getOperationMode() const {
    auto mode = pars::utility::getParametricModeFromString(this->getOption(modeOptionName).getArgumentByName("mode").getValueAsString());
    STORM_LOG_THROW(mode, storm::exceptions::IllegalArgumentException, "Parametric mode is not properly implemented.");
    return *mode;
}

bool ParametricSettings::isBigStepEnabled() const {
    return this->getOption(bigStepEnabledName).getHasOptionBeenSet();
}

uint64_t ParametricSettings::getBigStepHorizon() const {
    return this->getOption(bigStepEnabledName).getArgumentByName("horizon").getValueAsUnsignedInteger();
}

bool ParametricSettings::isLinearToSimpleEnabled() const {
    return this->getOption(linearToSimpleEnabledName).getHasOptionBeenSet();
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
