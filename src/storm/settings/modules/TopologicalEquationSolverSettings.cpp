#include "storm/settings/modules/TopologicalEquationSolverSettings.h"

#include "storm/settings/modules/CoreSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"
#include "storm/solver/SolverSelectionOptions.h"

#include "storm/storage/dd/DdType.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/exceptions/InvalidOptionException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string TopologicalEquationSolverSettings::moduleName = "topological";
const std::string TopologicalEquationSolverSettings::underlyingEquationSolverOptionName = "eqsolver";
const std::string TopologicalEquationSolverSettings::underlyingMinMaxMethodOptionName = "minmax";
const std::string TopologicalEquationSolverSettings::extendRelevantValuesOptionName = "relevant-values";

#ifdef STORM_HAVE_GMM
const std::string defaultEqSolverString = "gmm++";
#else
const std::string defaultEqSolverString = "eigen";
#endif

TopologicalEquationSolverSettings::TopologicalEquationSolverSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> linearEquationSolver = {"gmm++", "native", "eigen", "elimination"};
    this->addOption(storm::settings::OptionBuilder(moduleName, underlyingEquationSolverOptionName, true,
                                                   "Sets which solver is considered for solving the underlying equation systems.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the used solver.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(linearEquationSolver))
                                         .setDefaultValueString(defaultEqSolverString)
                                         .build())
                        .build());
    std::vector<std::string> minMaxSolvingTechniques = {"vi",       "value-iteration",
                                                        "pi",       "policy-iteration",
                                                        "lp",       "linear-programming",
                                                        "rs",       "ratsearch",
                                                        "ii",       "interval-iteration",
                                                        "svi",      "sound-value-iteration",
                                                        "ovi",      "optimistic-value-iteration",
                                                        "gvi",      "guessing-value-iteration",
                                                        "vi-to-pi", "vi-to-lp"};
    this->addOption(storm::settings::OptionBuilder(moduleName, underlyingMinMaxMethodOptionName, true,
                                                   "Sets which minmax method is considered for solving the underlying minmax equation systems.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the used min max method.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(minMaxSolvingTechniques))
                                         .setDefaultValueString("value-iteration")
                                         .build())
                        .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, extendRelevantValuesOptionName, true, "Sets whether relevant values are set to the underlying solver.")
            .setIsAdvanced()
            .build());
}

bool TopologicalEquationSolverSettings::isUnderlyingEquationSolverTypeSet() const {
    return this->getOption(underlyingEquationSolverOptionName).getHasOptionBeenSet();
}

bool TopologicalEquationSolverSettings::isUnderlyingEquationSolverTypeSetFromDefaultValue() const {
    return !this->getOption(underlyingEquationSolverOptionName).getHasOptionBeenSet() ||
           this->getOption(underlyingEquationSolverOptionName).getArgumentByName("name").wasSetFromDefaultValue();
}

storm::solver::EquationSolverType TopologicalEquationSolverSettings::getUnderlyingEquationSolverType() const {
    std::string equationSolverName = this->getOption(underlyingEquationSolverOptionName).getArgumentByName("name").getValueAsString();
    if (equationSolverName == "gmm++") {
        return storm::solver::EquationSolverType::Gmmxx;
    } else if (equationSolverName == "native") {
        return storm::solver::EquationSolverType::Native;
    } else if (equationSolverName == "eigen") {
        return storm::solver::EquationSolverType::Eigen;
    } else if (equationSolverName == "elimination") {
        return storm::solver::EquationSolverType::Elimination;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown underlying equation solver '" << equationSolverName << "'.");
}

bool TopologicalEquationSolverSettings::isUnderlyingMinMaxMethodSet() const {
    return this->getOption(underlyingMinMaxMethodOptionName).getHasOptionBeenSet();
}

bool TopologicalEquationSolverSettings::isUnderlyingMinMaxMethodSetFromDefaultValue() const {
    return !this->getOption(underlyingMinMaxMethodOptionName).getHasOptionBeenSet() ||
           this->getOption(underlyingMinMaxMethodOptionName).getArgumentByName("name").wasSetFromDefaultValue();
}

storm::solver::MinMaxMethod TopologicalEquationSolverSettings::getUnderlyingMinMaxMethod() const {
    std::string minMaxEquationSolvingTechnique = this->getOption(underlyingMinMaxMethodOptionName).getArgumentByName("name").getValueAsString();
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
    } else if (minMaxEquationSolvingTechnique == "vi-to-pi") {
        return storm::solver::MinMaxMethod::ViToPi;
    } else if (minMaxEquationSolvingTechnique == "vi-to-lp") {
        return storm::solver::MinMaxMethod::ViToLp;
    }

    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown underlying equation solver '" << minMaxEquationSolvingTechnique << "'.");
}

bool TopologicalEquationSolverSettings::isExtendRelevantValues() const {
    return this->getOption(extendRelevantValuesOptionName).getHasOptionBeenSet();
}

bool TopologicalEquationSolverSettings::check() const {
    if (this->isUnderlyingEquationSolverTypeSet() && getUnderlyingEquationSolverType() == storm::solver::EquationSolverType::Topological) {
        STORM_LOG_WARN("Underlying solver type of the topological solver can not be the topological solver.");
        return false;
    }
    if (this->isUnderlyingMinMaxMethodSet() && getUnderlyingMinMaxMethod() == storm::solver::MinMaxMethod::Topological) {
        STORM_LOG_WARN("Underlying minmax method of the topological solver can not be topological.");
        return false;
    }
    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
