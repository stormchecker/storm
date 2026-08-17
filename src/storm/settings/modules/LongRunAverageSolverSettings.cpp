#include "storm/settings/modules/LongRunAverageSolverSettings.h"

#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string LongRunAverageSolverSettings::moduleName = "lra";
const std::string LongRunAverageSolverSettings::detLraMethodOptionName = "detmethod";
const std::string LongRunAverageSolverSettings::nondetLraMethodOptionName = "nondetmethod";
const std::string LongRunAverageSolverSettings::maximalIterationsOptionName = "maxiter";
const std::string LongRunAverageSolverSettings::maximalIterationsOptionShortName = "i";
const std::string LongRunAverageSolverSettings::precisionOptionName = "precision";
const std::string LongRunAverageSolverSettings::absoluteOptionName = "absolute";
const std::string LongRunAverageSolverSettings::aperiodicFactorOptionName = "aperiodicfactor";

LongRunAverageSolverSettings::LongRunAverageSolverSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> detLraMethods = {"gb", "gain-bias-equations", "distr", "lra-distribution-equations", "vi", "value-iteration"};
    this->addOption(storm::settings::OptionBuilder(moduleName, detLraMethodOptionName, true,
                                                   "Sets which method is preferred for computing long run averages on deterministic models.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of a long run average computation method.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(detLraMethods))
                                         .setDefaultValueString("gb")
                                         .build())
                        .build());

    std::vector<std::string> nondetLraMethods = {"vi", "value-iteration", "linear-programming", "lp"};
    this->addOption(storm::settings::OptionBuilder(moduleName, nondetLraMethodOptionName, true,
                                                   "Sets which method is preferred for computing long run averages on models with nondeterminism.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of a long run average computation method.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(nondetLraMethods))
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

    this->addOption(storm::settings::OptionBuilder(moduleName, aperiodicFactorOptionName, true,
                                                   "If required by the selected method (e.g. vi), this factor controls how the system is made aperiodic")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("value", "The factor.")
                                         .setDefaultValueDouble(0.125)
                                         .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorExcluding(0.0, 1.0))
                                         .build())
                        .build());
}

storm::solver::LraMethod LongRunAverageSolverSettings::getDetLraMethod() const {
    std::string lraMethodString = this->getOption(detLraMethodOptionName).getArgumentByName("name").getValueAsString();
    if (lraMethodString == "gain-bias-equations" || lraMethodString == "gb") {
        return storm::solver::LraMethod::GainBiasEquations;
    }
    if (lraMethodString == "lra-distribution-equations" || lraMethodString == "distr") {
        return storm::solver::LraMethod::LraDistributionEquations;
    }
    if (lraMethodString == "value-iteration" || lraMethodString == "vi") {
        return storm::solver::LraMethod::ValueIteration;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException,
                    "Unknown lra solving technique for deterministic models:'" << lraMethodString << "'.");
}

bool LongRunAverageSolverSettings::isDetLraMethodSetFromDefaultValue() const {
    return !this->getOption(detLraMethodOptionName).getArgumentByName("name").getHasBeenSet() ||
           this->getOption(detLraMethodOptionName).getArgumentByName("name").wasSetFromDefaultValue();
}

storm::solver::LraMethod LongRunAverageSolverSettings::getNondetLraMethod() const {
    std::string lraMethodString = this->getOption(nondetLraMethodOptionName).getArgumentByName("name").getValueAsString();
    if (lraMethodString == "value-iteration" || lraMethodString == "vi") {
        return storm::solver::LraMethod::ValueIteration;
    } else if (lraMethodString == "linear-programming" || lraMethodString == "lp") {
        return storm::solver::LraMethod::LinearProgramming;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException,
                    "Unknown lra solving technique for nondeterministic models:'" << lraMethodString << "'.");
}

bool LongRunAverageSolverSettings::isNondetLraMethodSetFromDefaultValue() const {
    return !this->getOption(nondetLraMethodOptionName).getArgumentByName("name").getHasBeenSet() ||
           this->getOption(nondetLraMethodOptionName).getArgumentByName("name").wasSetFromDefaultValue();
}

bool LongRunAverageSolverSettings::isMaximalIterationCountSet() const {
    return this->getOption(maximalIterationsOptionName).getHasOptionBeenSet();
}

uint_fast64_t LongRunAverageSolverSettings::getMaximalIterationCount() const {
    return this->getOption(maximalIterationsOptionName).getArgumentByName("count").getValueAsUnsignedInteger();
}

bool LongRunAverageSolverSettings::isPrecisionSet() const {
    return this->getOption(precisionOptionName).getHasOptionBeenSet();
}

double LongRunAverageSolverSettings::getPrecision() const {
    return this->getOption(precisionOptionName).getArgumentByName("value").getValueAsDouble();
}

bool LongRunAverageSolverSettings::isRelativePrecision() const {
    return !this->getOption(absoluteOptionName).getHasOptionBeenSet();
}

double LongRunAverageSolverSettings::getAperiodicFactor() const {
    return this->getOption(aperiodicFactorOptionName).getArgumentByName("value").getValueAsDouble();
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
