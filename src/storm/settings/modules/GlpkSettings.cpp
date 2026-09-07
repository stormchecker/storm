#include "GlpkSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {
namespace settings {
namespace modules {

const std::string GlpkSettings::moduleName = "glpk";
const std::string GlpkSettings::integerToleranceOption = "inttol";
const std::string GlpkSettings::outputOptionName = "output";
const std::string GlpkSettings::noMilpPresolverOptionName = "nomilppresolver";

GlpkSettings::GlpkSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, outputOptionName, true, "If set, the glpk output will be printed to the command line.")
                        .setIsAdvanced()
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, noMilpPresolverOptionName, true, "Disables glpk's built-in MILP presolver.").setIsAdvanced().build());
    this->addOption(storm::settings::OptionBuilder(moduleName, integerToleranceOption, true, "Sets glpk's precision for integer variables.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("value", "The precision to achieve.")
                                         .setDefaultValueDouble(1e-06)
                                         .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorExcluding(0.0, 1.0))
                                         .build())
                        .build());
}

bool GlpkSettings::isOutputSet() const {
    return this->getOption(outputOptionName).getHasOptionBeenSet();
}

bool GlpkSettings::isMILPPresolverEnabled() const {
    return !this->getOption(noMilpPresolverOptionName).getHasOptionBeenSet();
}

bool GlpkSettings::isIntegerToleranceSet() const {
    return this->getOption(integerToleranceOption).getHasOptionBeenSet();
}

double GlpkSettings::getIntegerTolerance() const {
    return this->getOption(integerToleranceOption).getArgumentByName("value").getValueAsDouble();
}

bool GlpkSettings::check() const {
    if (isOutputSet() || isIntegerToleranceSet()) {
        STORM_LOG_WARN_COND(storm::settings::getModule<storm::settings::modules::CoreSettings>().getLpSolver() == storm::solver::LpSolverType::Glpk,
                            "glpk is not selected as the preferred LP solver, so setting options for glpk might have no effect.");
    }

    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
