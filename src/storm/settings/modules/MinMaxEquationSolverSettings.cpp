#include "storm/settings/modules/MinMaxEquationSolverSettings.h"

#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string MinMaxEquationSolverSettings::moduleName = "minmax";
const std::string solvingMethodOptionName = "method";
const std::string maximalIterationsOptionName = "maxiter";
const std::string maximalIterationsOptionShortName = "i";
const std::string precisionOptionName = "precision";
const std::string absoluteOptionName = "absolute";
const std::string valueIterationMultiplicationStyleOptionName = "vimult";
const std::string forceUniqueSolutionRequirementOptionName = "force-require-unique";
const std::string lpEqualityForUniqueActionsOptionName = "lp-eq-unique-actions";
const std::string lpUseNonTrivialBoundsOptionName = "lp-use-nontrivial-bounds";
const std::string lpOptimizeOnlyInitialStateOptionName = "lp-objective-type";

MinMaxEquationSolverSettings::MinMaxEquationSolverSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> minMaxSolvingTechniques = {"vi",          "value-iteration",
                                                        "pi",          "policy-iteration",
                                                        "lp",          "linear-programming",
                                                        "rs",          "ratsearch",
                                                        "ii",          "interval-iteration",
                                                        "svi",         "sound-value-iteration",
                                                        "ovi",         "optimistic-value-iteration",
                                                        "gvi",         "guessing-value-iteration",
                                                        "topological", "vi-to-pi",
                                                        "vi-to-lp",    "acyclic"};
    this->addOption(
        storm::settings::OptionBuilder(moduleName, solvingMethodOptionName, false, "Sets which min/max linear equation solving technique is preferred.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of a min/max linear equation solving technique.")
                             .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(minMaxSolvingTechniques))
                             .setDefaultValueString("topological")
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

    std::vector<std::string> multiplicationStyles = {"gaussseidel", "regular", "gs", "r"};
    this->addOption(storm::settings::OptionBuilder(moduleName, valueIterationMultiplicationStyleOptionName, false,
                                                   "Sets which method multiplication style to prefer for value iteration.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of a multiplication style.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(multiplicationStyles))
                                         .setDefaultValueString("gaussseidel")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, forceUniqueSolutionRequirementOptionName, false,
                                                   "Enforces end component collapsing for MinMax equation systems so that their solution becomes unique. May "
                                                   "simplify solving but causes some overhead.")
                        .setIsAdvanced()
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, lpEqualityForUniqueActionsOptionName, false,
                                                   "If set, enforce equality in the LP encoding for actions with a unique state.")
                        .setIsAdvanced()
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, lpUseNonTrivialBoundsOptionName, false, "If set, use nontrivial bounds in the LP encoding")
                        .setIsAdvanced()
                        .build());

    std::vector<std::string> optimizationObjectiveTypes = {"all", "onlyinitial"};
    this->addOption(
        storm::settings::OptionBuilder(moduleName, lpOptimizeOnlyInitialStateOptionName, false, "What objective to prefer")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("optimization-type", "What kind of optimization objective to prefer.")
                             .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(optimizationObjectiveTypes))
                             .setDefaultValueString("all")
                             .build())
            .build());
}

storm::solver::MinMaxMethod MinMaxEquationSolverSettings::getMinMaxEquationSolvingMethod() const {
    std::string minMaxEquationSolvingTechnique = this->getOption(solvingMethodOptionName).getArgumentByName("name").getValueAsString();
    if (minMaxEquationSolvingTechnique == "value-iteration" || minMaxEquationSolvingTechnique == "vi") {
        return storm::solver::MinMaxMethod::ValueIteration;
    } else if (minMaxEquationSolvingTechnique == "policy-iteration" || minMaxEquationSolvingTechnique == "pi") {
        return storm::solver::MinMaxMethod::PolicyIteration;
    } else if (minMaxEquationSolvingTechnique == "linear-programming" || minMaxEquationSolvingTechnique == "lp") {
        return storm::solver::MinMaxMethod::LinearProgramming;
    } else if (minMaxEquationSolvingTechnique == "ratsearch" || minMaxEquationSolvingTechnique == "rs") {
        return storm::solver::MinMaxMethod::RationalSearch;
    } else if (minMaxEquationSolvingTechnique == "interval-iteration" || minMaxEquationSolvingTechnique == "ii") {
        return storm::solver::MinMaxMethod::IntervalIteration;
    } else if (minMaxEquationSolvingTechnique == "sound-value-iteration" || minMaxEquationSolvingTechnique == "svi") {
        return storm::solver::MinMaxMethod::SoundValueIteration;
    } else if (minMaxEquationSolvingTechnique == "optimistic-value-iteration" || minMaxEquationSolvingTechnique == "ovi") {
        return storm::solver::MinMaxMethod::OptimisticValueIteration;
    } else if (minMaxEquationSolvingTechnique == "guessing-value-iteration" || minMaxEquationSolvingTechnique == "gvi") {
        return storm::solver::MinMaxMethod::GuessingValueIteration;
    } else if (minMaxEquationSolvingTechnique == "topological") {
        return storm::solver::MinMaxMethod::Topological;
    } else if (minMaxEquationSolvingTechnique == "vi-to-pi") {
        return storm::solver::MinMaxMethod::ViToPi;
    } else if (minMaxEquationSolvingTechnique == "vi-to-lp") {
        return storm::solver::MinMaxMethod::ViToLp;
    } else if (minMaxEquationSolvingTechnique == "acyclic") {
        return storm::solver::MinMaxMethod::Acyclic;
    }

    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException,
                    "Unknown min/max equation solving technique '" << minMaxEquationSolvingTechnique << "'.");
}

bool MinMaxEquationSolverSettings::isMinMaxEquationSolvingMethodSetFromDefaultValue() const {
    return !this->getOption(solvingMethodOptionName).getArgumentByName("name").getHasBeenSet() ||
           this->getOption(solvingMethodOptionName).getArgumentByName("name").wasSetFromDefaultValue();
}

bool MinMaxEquationSolverSettings::isMinMaxEquationSolvingMethodSet() const {
    return this->getOption(solvingMethodOptionName).getHasOptionBeenSet();
}

bool MinMaxEquationSolverSettings::isMaximalIterationCountSet() const {
    return this->getOption(maximalIterationsOptionName).getHasOptionBeenSet();
}

uint_fast64_t MinMaxEquationSolverSettings::getMaximalIterationCount() const {
    return this->getOption(maximalIterationsOptionName).getArgumentByName("count").getValueAsUnsignedInteger();
}

bool MinMaxEquationSolverSettings::isPrecisionSet() const {
    return this->getOption(precisionOptionName).getHasOptionBeenSet();
}

double MinMaxEquationSolverSettings::getPrecision() const {
    return this->getOption(precisionOptionName).getArgumentByName("value").getValueAsDouble();
}

bool MinMaxEquationSolverSettings::isConvergenceCriterionSet() const {
    return this->getOption(absoluteOptionName).getHasOptionBeenSet();
}

MinMaxEquationSolverSettings::ConvergenceCriterion MinMaxEquationSolverSettings::getConvergenceCriterion() const {
    return this->getOption(absoluteOptionName).getHasOptionBeenSet() ? MinMaxEquationSolverSettings::ConvergenceCriterion::Absolute
                                                                     : MinMaxEquationSolverSettings::ConvergenceCriterion::Relative;
}

storm::solver::MultiplicationStyle MinMaxEquationSolverSettings::getValueIterationMultiplicationStyle() const {
    std::string multiplicationStyleString = this->getOption(valueIterationMultiplicationStyleOptionName).getArgumentByName("name").getValueAsString();
    if (multiplicationStyleString == "gaussseidel" || multiplicationStyleString == "gs") {
        return storm::solver::MultiplicationStyle::GaussSeidel;
    } else if (multiplicationStyleString == "regular" || multiplicationStyleString == "r") {
        return storm::solver::MultiplicationStyle::Regular;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown multiplication style '" << multiplicationStyleString << "'.");
}

bool MinMaxEquationSolverSettings::isForceUniqueSolutionRequirementSet() const {
    return this->getOption(forceUniqueSolutionRequirementOptionName).getHasOptionBeenSet();
}

bool MinMaxEquationSolverSettings::getLpUseOnlyInitialStateAsObjective() const {
    return this->getOption(lpOptimizeOnlyInitialStateOptionName).getArgumentByName("optimization-type").getValueAsString() == "onlyinitial";
}

bool MinMaxEquationSolverSettings::getLpUseNonTrivialBounds() const {
    return this->getOption(lpUseNonTrivialBoundsOptionName).getHasOptionBeenSet();
}

bool MinMaxEquationSolverSettings::getLpUseEqualityForTrivialActions() const {
    return this->getOption(lpEqualityForUniqueActionsOptionName).getHasOptionBeenSet();
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
