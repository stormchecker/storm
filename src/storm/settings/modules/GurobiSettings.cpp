#include "storm/settings/modules/GurobiSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/solver/GurobiLpSolver.h"
#include "storm/solver/SolverSelectionOptions.h"
namespace storm {
namespace settings {
namespace modules {

const std::string GurobiSettings::moduleName = "gurobi";
static const std::string methodOption = "method";
static const std::string integerToleranceOption = "inttol";
static const std::string threadsOption = "threads";
static const std::string outputOption = "output";
static const std::string mipFocusOption = "mipfocus";
static const std::string concurrentMipThreadsOption = "concurrentmip";

GurobiSettings::GurobiSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> methods;
    for (auto const& m : solver::getGurobiSolverMethods()) {
        methods.push_back(solver::toString(m));
    }

    this->addOption(OptionBuilder(moduleName, methodOption, true, "The method Gurobi should use.")
                        .setIsAdvanced()
                        .addArgument(ArgumentBuilder::createStringArgument("method", "the name of the method")
                                         .setDefaultValueString("auto")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(methods))
                                         .build())
                        .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, threadsOption, true, "The number of threads that may be used by Gurobi.")
            .setIsAdvanced()
            .addArgument(
                storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("count", "The number of threads.").setDefaultValueUnsignedInteger(1).build())
            .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, outputOption, true, "If set, the Gurobi output will be printed to the command line.")
                        .setIsAdvanced()
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, integerToleranceOption, true, "Sets Gurobi's precision for integer variables.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("value", "The precision to achieve.")
                                         .setDefaultValueDouble(1e-06)
                                         .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorExcluding(0.0, 1.0))
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, mipFocusOption, true, "The high level solution strategy used to solve MILPs.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("value", "The number of the strategy.")
                                         .setDefaultValueUnsignedInteger(0)
                                         .addValidatorUnsignedInteger(ArgumentValidatorFactory::createUnsignedRangeValidatorIncluding(0, 3))
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, concurrentMipThreadsOption, true,
                                                   "The number of MIP solvers Gurobi spawns in parallel. Shall not be larger then the number of threads.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("value", "The number of parallel solvers.")
                                         .setDefaultValueUnsignedInteger(1)
                                         .addValidatorUnsignedInteger(ArgumentValidatorFactory::createUnsignedGreaterEqualValidator(1))
                                         .build())
                        .build());
}

bool GurobiSettings::isIntegerToleranceSet() const {
    return this->getOption(integerToleranceOption).getHasOptionBeenSet();
}

double GurobiSettings::getIntegerTolerance() const {
    return this->getOption(integerToleranceOption).getArgumentByName("value").getValueAsDouble();
}

bool GurobiSettings::isNumberOfThreadsSet() const {
    return this->getOption(threadsOption).getHasOptionBeenSet();
}

uint64_t GurobiSettings::getNumberOfThreads() const {
    return this->getOption(threadsOption).getArgumentByName("count").getValueAsUnsignedInteger();
}

solver::GurobiSolverMethod GurobiSettings::getMethod() const {
    auto method = solver::gurobiSolverMethodFromString(this->getOption(methodOption).getArgumentByName("method").getValueAsString());
    if (method.has_value()) {
        return method.value();
    }
    STORM_LOG_ASSERT(false, "Unknown method name should not get through validator.");
    return solver::GurobiSolverMethod::AUTOMATIC;
}

bool GurobiSettings::isOutputSet() const {
    return this->getOption(outputOption).getHasOptionBeenSet();
}

uint64_t GurobiSettings::getMIPFocus() const {
    return this->getOption(mipFocusOption).getArgumentByName("value").getValueAsUnsignedInteger();
}

uint64_t GurobiSettings::getNumberOfConcurrentMipThreads() const {
    return this->getOption(concurrentMipThreadsOption).getArgumentByName("value").getValueAsUnsignedInteger();
}

bool GurobiSettings::check() const {
    if (isOutputSet() || isIntegerToleranceSet() || isNumberOfThreadsSet()) {
        STORM_LOG_WARN_COND(storm::settings::getModule<storm::settings::modules::CoreSettings>().getLpSolver() == storm::solver::LpSolverType::Gurobi,
                            "Gurobi is not selected as the preferred LP solver, so setting options for Gurobi might have no effect.");
    }
    STORM_LOG_WARN_COND(getNumberOfConcurrentMipThreads() <= getNumberOfThreads(),
                        "Requested more concurrent MILP solvers then the available threads for Gurobi.");
    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
