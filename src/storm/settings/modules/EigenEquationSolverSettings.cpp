#include "EigenEquationSolverSettings.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/solver/SolverSelectionOptions.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string EigenEquationSolverSettings::moduleName = "eigen";
const std::string EigenEquationSolverSettings::techniqueOptionName = "method";
const std::string EigenEquationSolverSettings::preconditionOptionName = "precond";
const std::string EigenEquationSolverSettings::maximalIterationsOptionName = "maxiter";
const std::string EigenEquationSolverSettings::maximalIterationsOptionShortName = "i";
const std::string EigenEquationSolverSettings::precisionOptionName = "precision";
const std::string EigenEquationSolverSettings::restartOptionName = "restart";

EigenEquationSolverSettings::EigenEquationSolverSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> methods = {"sparselu", "bicgstab", "dgmres", "gmres"};
    this->addOption(storm::settings::OptionBuilder(moduleName, techniqueOptionName, true,
                                                   "The method to be used for solving linear equation systems with the eigen solver.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the method to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(methods))
                                         .setDefaultValueString("gmres")
                                         .build())
                        .build());

    // Register available preconditioners.
    std::vector<std::string> preconditioner = {"ilu", "diagonal", "none"};
    this->addOption(
        storm::settings::OptionBuilder(moduleName, preconditionOptionName, true, "The preconditioning technique used for solving linear equation systems.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the preconditioning method.")
                             .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(preconditioner))
                             .setDefaultValueString("ilu")
                             .build())
            .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, restartOptionName, true, "The number of iteration until restarted methods are actually restarted.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("count", "The number of iterations.")
                             .setDefaultValueUnsignedInteger(50)
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
}

bool EigenEquationSolverSettings::isLinearEquationSystemMethodSet() const {
    return this->getOption(techniqueOptionName).getHasOptionBeenSet();
}

bool EigenEquationSolverSettings::isLinearEquationSystemMethodSetFromDefault() const {
    return !this->getOption(techniqueOptionName).getHasOptionBeenSet() ||
           this->getOption(techniqueOptionName).getArgumentByName("name").wasSetFromDefaultValue();
}

storm::solver::EigenLinearEquationSolverMethod EigenEquationSolverSettings::getLinearEquationSystemMethod() const {
    std::string linearEquationSystemTechniqueAsString = this->getOption(techniqueOptionName).getArgumentByName("name").getValueAsString();
    if (linearEquationSystemTechniqueAsString == "sparselu") {
        return storm::solver::EigenLinearEquationSolverMethod::SparseLU;
    } else if (linearEquationSystemTechniqueAsString == "bicgstab") {
        return storm::solver::EigenLinearEquationSolverMethod::Bicgstab;
    } else if (linearEquationSystemTechniqueAsString == "dgmres") {
        return storm::solver::EigenLinearEquationSolverMethod::DGmres;
    } else if (linearEquationSystemTechniqueAsString == "gmres") {
        return storm::solver::EigenLinearEquationSolverMethod::Gmres;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException,
                    "Unknown solution technique '" << linearEquationSystemTechniqueAsString << "' selected.");
}

bool EigenEquationSolverSettings::isPreconditioningMethodSet() const {
    return this->getOption(preconditionOptionName).getHasOptionBeenSet();
}

storm::solver::EigenLinearEquationSolverPreconditioner EigenEquationSolverSettings::getPreconditioningMethod() const {
    std::string PreconditioningMethodAsString = this->getOption(preconditionOptionName).getArgumentByName("name").getValueAsString();
    if (PreconditioningMethodAsString == "ilu") {
        return storm::solver::EigenLinearEquationSolverPreconditioner::Ilu;
    } else if (PreconditioningMethodAsString == "diagonal") {
        return storm::solver::EigenLinearEquationSolverPreconditioner::Diagonal;
    } else if (PreconditioningMethodAsString == "none") {
        return storm::solver::EigenLinearEquationSolverPreconditioner::None;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException,
                    "Unknown preconditioning technique '" << PreconditioningMethodAsString << "' selected.");
}

bool EigenEquationSolverSettings::isRestartIterationCountSet() const {
    return this->getOption(restartOptionName).getHasOptionBeenSet();
}

uint_fast64_t EigenEquationSolverSettings::getRestartIterationCount() const {
    return this->getOption(restartOptionName).getArgumentByName("count").getValueAsUnsignedInteger();
}

bool EigenEquationSolverSettings::isMaximalIterationCountSet() const {
    return this->getOption(maximalIterationsOptionName).getHasOptionBeenSet();
}

uint_fast64_t EigenEquationSolverSettings::getMaximalIterationCount() const {
    return this->getOption(maximalIterationsOptionName).getArgumentByName("count").getValueAsUnsignedInteger();
}

bool EigenEquationSolverSettings::isPrecisionSet() const {
    return this->getOption(precisionOptionName).getHasOptionBeenSet();
}

double EigenEquationSolverSettings::getPrecision() const {
    return this->getOption(precisionOptionName).getArgumentByName("value").getValueAsDouble();
}

bool EigenEquationSolverSettings::check() const {
    // This list does not include the precision, because this option is shared with other modules.
    bool optionsSet = isLinearEquationSystemMethodSet() || isPreconditioningMethodSet() || isMaximalIterationCountSet();

    STORM_LOG_WARN_COND(
        storm::settings::getModule<storm::settings::modules::CoreSettings>().getEquationSolver() == storm::solver::EquationSolverType::Eigen || !optionsSet,
        "Eigen is not selected as the preferred equation solver, so setting options for eigen might have no effect.");

    return true;
}

std::ostream& operator<<(std::ostream& out, EigenEquationSolverSettings::LinearEquationMethod const& method) {
    switch (method) {
        case EigenEquationSolverSettings::LinearEquationMethod::BiCGSTAB:
            out << "bicgstab";
            break;
        case EigenEquationSolverSettings::LinearEquationMethod::GMRES:
            out << "gmres";
            break;
        case EigenEquationSolverSettings::LinearEquationMethod::DGMRES:
            out << "dgmres";
            break;
        case EigenEquationSolverSettings::LinearEquationMethod::SparseLU:
            out << "sparselu";
            break;
    }
    return out;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
