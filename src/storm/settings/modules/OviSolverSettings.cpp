#include "OviSolverSettings.h"

#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

namespace storm {
namespace settings {
namespace modules {

const std::string OviSolverSettings::moduleName = "ovi";
const std::string OviSolverSettings::upperBoundGuessingFactorOptionName = "upper-bound-factor";

OviSolverSettings::OviSolverSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, upperBoundGuessingFactorOptionName, false, "Sets how optimistic the upper bound is guessed.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("factor", "The factor.")
                                         .setDefaultValueDouble(1e-6)
                                         .addValidatorDouble(ArgumentValidatorFactory::createDoubleGreaterValidator(0.0))
                                         .build())
                        .build());
}

bool OviSolverSettings::hasUpperBoundGuessingFactorBeenSet() const {
    return this->getOption(upperBoundGuessingFactorOptionName).getHasOptionBeenSet();
}

double OviSolverSettings::getUpperBoundGuessingFactor() const {
    return this->getOption(upperBoundGuessingFactorOptionName).getArgumentByName("factor").getValueAsDouble();
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
