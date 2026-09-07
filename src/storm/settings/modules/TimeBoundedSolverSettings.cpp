#include "TimeBoundedSolverSettings.h"

#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string TimeBoundedSolverSettings::moduleName = "timebounded";

const std::string TimeBoundedSolverSettings::maMethodOptionName = "mamethod";
const std::string TimeBoundedSolverSettings::precisionOptionName = "precision";
const std::string TimeBoundedSolverSettings::absoluteOptionName = "absolute";
const std::string TimeBoundedSolverSettings::unifPlusKappaOptionName = "kappa";

TimeBoundedSolverSettings::TimeBoundedSolverSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> maMethods = {"imca", "unifplus"};
    this->addOption(storm::settings::OptionBuilder(moduleName, maMethodOptionName, false, "The method to use to solve bounded reachability queries on MAs.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the method to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(maMethods))
                                         .setDefaultValueString("unifplus")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, precisionOptionName, false, "The precision used for detecting convergence of iterative methods.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("value", "The precision to achieve.")
                                         .setDefaultValueDouble(1e-06)
                                         .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorExcluding(0.0, 1.0))
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, absoluteOptionName, false,
                                                   "Sets whether the relative or the absolute error is considered for detecting convergence.")
                        .setIsAdvanced()
                        .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, unifPlusKappaOptionName, false, "Controls which amount of the approximation error is due to truncation.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("kappa", "The factor")
                             .setDefaultValueDouble(0.05)
                             .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorExcluding(0.0, 1.0))
                             .build())
            .build());
}

bool TimeBoundedSolverSettings::isPrecisionSet() const {
    return this->getOption(precisionOptionName).getHasOptionBeenSet();
}

double TimeBoundedSolverSettings::getPrecision() const {
    return this->getOption(precisionOptionName).getArgumentByName("value").getValueAsDouble();
}

bool TimeBoundedSolverSettings::isRelativePrecision() const {
    return !this->getOption(absoluteOptionName).getHasOptionBeenSet();
}

storm::solver::MaBoundedReachabilityMethod TimeBoundedSolverSettings::getMaMethod() const {
    std::string techniqueAsString = this->getOption(maMethodOptionName).getArgumentByName("name").getValueAsString();
    if (techniqueAsString == "imca") {
        return storm::solver::MaBoundedReachabilityMethod::Imca;
    }
    return storm::solver::MaBoundedReachabilityMethod::UnifPlus;
}

bool TimeBoundedSolverSettings::isMaMethodSetFromDefaultValue() const {
    return !this->getOption(maMethodOptionName).getArgumentByName("name").getHasBeenSet() ||
           this->getOption(maMethodOptionName).getArgumentByName("name").wasSetFromDefaultValue();
}

double TimeBoundedSolverSettings::getUnifPlusKappa() const {
    return this->getOption(unifPlusKappaOptionName).getArgumentByName("kappa").getValueAsDouble();
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
