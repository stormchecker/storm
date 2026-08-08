#include "storm-pars/settings/modules/FeasibilitySettings.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

namespace storm::settings::modules {

const std::string FeasibilitySettings::moduleName = "feasibility";
const std::string methodOptionName = "method";
const std::string directionOptionName = "direction";
const std::string guaranteeOptionName = "guarantee";

FeasibilitySettings::FeasibilitySettings() : ModuleSettings(moduleName) {
    std::vector<std::string> methodChoice = {"gd", "pla"};
    this->addOption(OptionBuilder(moduleName, methodOptionName, true, "Which method to use")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("method", "The method")
                                         .addValidatorString(storm::settings::ArgumentValidatorFactory::createMultipleChoiceValidator(methodChoice))
                                         .build())
                        .build());
    std::vector<std::string> directions = {"min", "max"};
    this->addOption(OptionBuilder(moduleName, directionOptionName, false, "Which direction do the parameters optimize")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("direction", "The optimization direction")
                                         .addValidatorString(storm::settings::ArgumentValidatorFactory::createMultipleChoiceValidator(directions))
                                         .build())
                        .build());
    std::vector<std::string> precisiontype = {"rel", "abs"};
    this->addOption(OptionBuilder(moduleName, guaranteeOptionName, false, "Specifies the guarantee that must be provided. If not set, no guarantee is given.")
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("precision", "The desired precision")
                                         .setDefaultValueDouble(1.00)
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleRangeValidatorIncluding(0.0, 1.0))
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("precisiontype", "The desired precision type.")
                                         .setDefaultValueString("rel")
                                         .makeOptional()
                                         .addValidatorString(storm::settings::ArgumentValidatorFactory::createMultipleChoiceValidator(precisiontype))
                                         .build())
                        .build());
}

storm::pars::FeasibilityMethod FeasibilitySettings::getFeasibilityMethod() const {
    auto str = this->getOption(methodOptionName).getArgumentByName("method").getValueAsString();
    if (str == "gd") {
        return storm::pars::FeasibilityMethod::GD;
    } else {
        STORM_LOG_ASSERT(str == "pla", "Only remaining option is PLA.");
        return storm::pars::FeasibilityMethod::PLA;
    }
}

bool FeasibilitySettings::isParameterDirectionSet() const {
    return this->getOption(directionOptionName).getHasOptionBeenSet();
}

storm::solver::OptimizationDirection FeasibilitySettings::getParameterDirection() const {
    auto str = this->getOption(directionOptionName).getArgumentByName("direction").getValueAsString();
    if (str == "min") {
        return storm::solver::OptimizationDirection::Minimize;
    } else {
        STORM_LOG_ASSERT(str == "max", "If not min, then max.");
        return storm::solver::OptimizationDirection::Maximize;
    }
}

bool FeasibilitySettings::hasOptimalValueGuaranteeBeenSet() const {
    return this->getOption(guaranteeOptionName).getHasOptionBeenSet();
};

double FeasibilitySettings::getOptimalValueGuarantee() const {
    auto generalSettings = storm::settings::getModule<storm::settings::modules::GeneralSettings>();
    if (!generalSettings.isPrecisionSet() && generalSettings.isSoundSet()) {
        double prec = this->getOption(guaranteeOptionName).getArgumentByName("precision").getValueAsDouble() / 10;
        generalSettings.setPrecision(std::to_string(prec));
        STORM_LOG_WARN("Reset precision for solver to " << prec << " this is sufficient for guarantee value precision of " << (prec) * 10 << '\n');
    }
    return this->getOption(guaranteeOptionName).getArgumentByName("precision").getValueAsDouble();
}

bool FeasibilitySettings::isAbsolutePrecisionSet() const {
    auto str = this->getOption(guaranteeOptionName).getArgumentByName("precisiontype").getValueAsString();
    if (str == "abs") {
        return true;
    } else {
        STORM_LOG_ASSERT(str == "rel", "If not abs, then rel.");
        return false;
    }
}

}  // namespace storm::settings::modules