#include "storm/settings/modules/NativeEquationSolverSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/solver/SolverSelectionOptions.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string NativeEquationSolverSettings::moduleName = "native";
const std::string NativeEquationSolverSettings::techniqueOptionName = "method";
const std::string NativeEquationSolverSettings::omegaOptionName = "soromega";
const std::string NativeEquationSolverSettings::maximalIterationsOptionName = "maxiter";
const std::string NativeEquationSolverSettings::maximalIterationsOptionShortName = "i";
const std::string NativeEquationSolverSettings::precisionOptionName = "precision";
const std::string NativeEquationSolverSettings::absoluteOptionName = "absolute";
const std::string NativeEquationSolverSettings::powerMethodMultiplicationStyleOptionName = "powmult";
const std::string NativeEquationSolverSettings::intervalIterationSymmetricUpdatesOptionName = "symmetricupdates";

NativeEquationSolverSettings::NativeEquationSolverSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> methods = {"jacobi", "gaussseidel",
                                        "sor",    "walkerchae",
                                        "power",  "sound-value-iteration",
                                        "svi",    "optimistic-value-iteration",
                                        "ovi",    "guessing-value-iteration",
                                        "gvi",    "interval-iteration",
                                        "ii",     "ratsearch"};
    this->addOption(storm::settings::OptionBuilder(moduleName, techniqueOptionName, true,
                                                   "The method to be used for solving linear equation systems with the native engine.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the method to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(methods))
                                         .setDefaultValueString("jacobi")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, maximalIterationsOptionName, false,
                                                   "The maximal number of iterations to perform before iterative solving is aborted.")
                        .setIsAdvanced()
                        .setShortName(maximalIterationsOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("count", "The maximal iteration count.").build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, precisionOptionName, false, "The precision used for detecting convergence of iterative methods.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("value", "The precision to achieve.")
                                         .setDefaultValueDouble(1e-06)
                                         .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorExcluding(0.0, 1.0))
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, omegaOptionName, false, "The omega used for SOR.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("value", "The value of the SOR parameter.")
                                         .setDefaultValueDouble(0.9)
                                         .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorExcluding(0.0, 1.0))
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, absoluteOptionName, false,
                                                   "Sets whether the relative or the absolute error is considered for detecting convergence.")
                        .setIsAdvanced()
                        .build());

    std::vector<std::string> multiplicationStyles = {"gaussseidel", "regular", "gs", "r"};
    this->addOption(storm::settings::OptionBuilder(moduleName, powerMethodMultiplicationStyleOptionName, false,
                                                   "Sets which method multiplication style to prefer for the power method.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of a multiplication style.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(multiplicationStyles))
                                         .setDefaultValueString("gaussseidel")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, intervalIterationSymmetricUpdatesOptionName, false,
                                                   "If set, interval iteration performs an update on both, lower and upper bound in each iteration")
                        .setIsAdvanced()
                        .build());
}

bool NativeEquationSolverSettings::isLinearEquationSystemTechniqueSet() const {
    return this->getOption(techniqueOptionName).getHasOptionBeenSet();
}

bool NativeEquationSolverSettings::isLinearEquationSystemTechniqueSetFromDefaultValue() const {
    return !this->getOption(techniqueOptionName).getHasOptionBeenSet() ||
           this->getOption(techniqueOptionName).getArgumentByName("name").wasSetFromDefaultValue();
}

storm::solver::NativeLinearEquationSolverMethod NativeEquationSolverSettings::getLinearEquationSystemMethod() const {
    std::string linearEquationSystemTechniqueAsString = this->getOption(techniqueOptionName).getArgumentByName("name").getValueAsString();
    if (linearEquationSystemTechniqueAsString == "jacobi") {
        return storm::solver::NativeLinearEquationSolverMethod::Jacobi;
    } else if (linearEquationSystemTechniqueAsString == "gaussseidel") {
        return storm::solver::NativeLinearEquationSolverMethod::GaussSeidel;
    } else if (linearEquationSystemTechniqueAsString == "sor") {
        return storm::solver::NativeLinearEquationSolverMethod::SOR;
    } else if (linearEquationSystemTechniqueAsString == "walkerchae") {
        return storm::solver::NativeLinearEquationSolverMethod::WalkerChae;
    } else if (linearEquationSystemTechniqueAsString == "power") {
        return storm::solver::NativeLinearEquationSolverMethod::Power;
    } else if (linearEquationSystemTechniqueAsString == "sound-value-iteration" || linearEquationSystemTechniqueAsString == "svi") {
        return storm::solver::NativeLinearEquationSolverMethod::SoundValueIteration;
    } else if (linearEquationSystemTechniqueAsString == "optimistic-value-iteration" || linearEquationSystemTechniqueAsString == "ovi") {
        return storm::solver::NativeLinearEquationSolverMethod::OptimisticValueIteration;
    } else if (linearEquationSystemTechniqueAsString == "guessing-value-iteration" || linearEquationSystemTechniqueAsString == "gvi") {
        return storm::solver::NativeLinearEquationSolverMethod::GuessingValueIteration;
    } else if (linearEquationSystemTechniqueAsString == "interval-iteration" || linearEquationSystemTechniqueAsString == "ii") {
        return storm::solver::NativeLinearEquationSolverMethod::IntervalIteration;
    } else if (linearEquationSystemTechniqueAsString == "ratsearch") {
        return storm::solver::NativeLinearEquationSolverMethod::RationalSearch;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException,
                    "Unknown solution technique '" << linearEquationSystemTechniqueAsString << "' selected.");
}

bool NativeEquationSolverSettings::isMaximalIterationCountSet() const {
    return this->getOption(maximalIterationsOptionName).getHasOptionBeenSet();
}

uint_fast64_t NativeEquationSolverSettings::getMaximalIterationCount() const {
    return this->getOption(maximalIterationsOptionName).getArgumentByName("count").getValueAsUnsignedInteger();
}

bool NativeEquationSolverSettings::isPrecisionSet() const {
    return this->getOption(precisionOptionName).getHasOptionBeenSet();
}

double NativeEquationSolverSettings::getPrecision() const {
    return this->getOption(precisionOptionName).getArgumentByName("value").getValueAsDouble();
}

double NativeEquationSolverSettings::getOmega() const {
    return this->getOption(omegaOptionName).getArgumentByName("value").getValueAsDouble();
}

bool NativeEquationSolverSettings::isConvergenceCriterionSet() const {
    return this->getOption(absoluteOptionName).getHasOptionBeenSet();
}

NativeEquationSolverSettings::ConvergenceCriterion NativeEquationSolverSettings::getConvergenceCriterion() const {
    return this->getOption(absoluteOptionName).getHasOptionBeenSet() ? NativeEquationSolverSettings::ConvergenceCriterion::Absolute
                                                                     : NativeEquationSolverSettings::ConvergenceCriterion::Relative;
}

storm::solver::MultiplicationStyle NativeEquationSolverSettings::getPowerMethodMultiplicationStyle() const {
    std::string multiplicationStyleString = this->getOption(powerMethodMultiplicationStyleOptionName).getArgumentByName("name").getValueAsString();
    if (multiplicationStyleString == "gaussseidel" || multiplicationStyleString == "gs") {
        return storm::solver::MultiplicationStyle::GaussSeidel;
    } else if (multiplicationStyleString == "regular" || multiplicationStyleString == "r") {
        return storm::solver::MultiplicationStyle::Regular;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown multiplication style '" << multiplicationStyleString << "'.");
}

bool NativeEquationSolverSettings::isForceIntervalIterationSymmetricUpdatesSet() const {
    return this->getOption(intervalIterationSymmetricUpdatesOptionName).getHasOptionBeenSet();
}

bool NativeEquationSolverSettings::check() const {
    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
