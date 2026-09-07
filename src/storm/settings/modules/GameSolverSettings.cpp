#include "GameSolverSettings.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string GameSolverSettings::moduleName = "game";
const std::string GameSolverSettings::solvingMethodOptionName = "method";
const std::string GameSolverSettings::maximalIterationsOptionName = "maxiter";
const std::string GameSolverSettings::maximalIterationsOptionShortName = "i";
const std::string GameSolverSettings::precisionOptionName = "precision";
const std::string GameSolverSettings::absoluteOptionName = "absolute";

GameSolverSettings::GameSolverSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> gameSolvingTechniques = {"vi", "value-iteration", "pi", "policy-iteration"};
    this->addOption(storm::settings::OptionBuilder(moduleName, solvingMethodOptionName, false, "Sets which game solving technique is preferred.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of a game solving technique.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(gameSolvingTechniques))
                                         .setDefaultValueString("vi")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, maximalIterationsOptionName, false,
                                                   "The maximal number of iterations to perform before iterative solving is aborted.")
                        .setShortName(maximalIterationsOptionShortName)
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("count", "The maximal iteration count.").build())
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
}

storm::solver::GameMethod GameSolverSettings::getGameSolvingMethod() const {
    std::string gameSolvingTechnique = this->getOption(solvingMethodOptionName).getArgumentByName("name").getValueAsString();
    if (gameSolvingTechnique == "value-iteration" || gameSolvingTechnique == "vi") {
        return storm::solver::GameMethod::ValueIteration;
    } else if (gameSolvingTechnique == "policy-iteration" || gameSolvingTechnique == "pi") {
        return storm::solver::GameMethod::PolicyIteration;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown game solving technique '" << gameSolvingTechnique << "'.");
}

bool GameSolverSettings::isGameSolvingMethodSet() const {
    return this->getOption(solvingMethodOptionName).getHasOptionBeenSet();
}

bool GameSolverSettings::isGameSolvingMethodSetFromDefaultValue() const {
    return !this->getOption(solvingMethodOptionName).getArgumentByName("name").getHasBeenSet() ||
           this->getOption(solvingMethodOptionName).getArgumentByName("name").wasSetFromDefaultValue();
}

bool GameSolverSettings::isMaximalIterationCountSet() const {
    return this->getOption(maximalIterationsOptionName).getHasOptionBeenSet();
}

uint_fast64_t GameSolverSettings::getMaximalIterationCount() const {
    return this->getOption(maximalIterationsOptionName).getArgumentByName("count").getValueAsUnsignedInteger();
}

bool GameSolverSettings::isPrecisionSet() const {
    return this->getOption(precisionOptionName).getHasOptionBeenSet();
}

double GameSolverSettings::getPrecision() const {
    return this->getOption(precisionOptionName).getArgumentByName("value").getValueAsDouble();
}

bool GameSolverSettings::isConvergenceCriterionSet() const {
    return this->getOption(absoluteOptionName).getHasOptionBeenSet();
}

GameSolverSettings::ConvergenceCriterion GameSolverSettings::getConvergenceCriterion() const {
    return this->getOption(absoluteOptionName).getHasOptionBeenSet() ? GameSolverSettings::ConvergenceCriterion::Absolute
                                                                     : GameSolverSettings::ConvergenceCriterion::Relative;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
