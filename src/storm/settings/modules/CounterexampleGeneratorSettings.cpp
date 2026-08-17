#include "storm/settings/modules/CounterexampleGeneratorSettings.h"

#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/IOSettings.h"

namespace storm {
namespace settings {
namespace modules {

const std::string CounterexampleGeneratorSettings::moduleName = "counterexample";
const std::string CounterexampleGeneratorSettings::counterexampleOptionName = "counterexample";
const std::string CounterexampleGeneratorSettings::counterexampleOptionShortName = "cex";
const std::string CounterexampleGeneratorSettings::counterexampleTypeOptionName = "cextype";
const std::string CounterexampleGeneratorSettings::shortestPathMaxKOptionName = "shortestpath-maxk";
const std::string CounterexampleGeneratorSettings::minimalCommandMethodOptionName = "mincmdmethod";
const std::string CounterexampleGeneratorSettings::encodeReachabilityOptionName = "encreach";
const std::string CounterexampleGeneratorSettings::schedulerCutsOptionName = "schedcuts";
const std::string CounterexampleGeneratorSettings::noDynamicConstraintsOptionName = "nodyn";

CounterexampleGeneratorSettings::CounterexampleGeneratorSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, counterexampleOptionName, false,
                                                   "Generates a counterexample for the given PRCTL formulas if not satisfied by the model.")
                        .setShortName(counterexampleOptionShortName)
                        .build());
    std::vector<std::string> cextype = {"mincmd", "shortestpath"};
    this->addOption(storm::settings::OptionBuilder(moduleName, counterexampleTypeOptionName, false,
                                                   "Generates a counterexample of the given type if the given PRCTL formula is not satisfied by the model.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("type", "The type of the counterexample to compute.")
                                         .setDefaultValueString("mincmd")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(cextype))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, shortestPathMaxKOptionName, false, "Maximal number K of shortest paths to generate.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument(
                                         "maxk", "Upper bound on number of generated paths. Default value is 10.")
                                         .setDefaultValueUnsignedInteger(10)
                                         .build())
                        .build());
    std::vector<std::string> method = {"maxsat", "milp"};
    this->addOption(storm::settings::OptionBuilder(moduleName, minimalCommandMethodOptionName, true,
                                                   "Sets which method is used to derive the counterexample in terms of a minimal command/edge set.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("method", "The name of the method to use.")
                                         .setDefaultValueString("maxsat")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(method))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, encodeReachabilityOptionName, true,
                                                   "Sets whether to encode reachability for MAXSAT-based counterexample generation.")
                        .setIsAdvanced()
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, schedulerCutsOptionName, true,
                                                   "Sets whether to add the scheduler cuts for MILP-based counterexample generation.")
                        .setIsAdvanced()
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, noDynamicConstraintsOptionName, true,
                                                   "Disables the generation of dynamic constraints in the MAXSAT-based counterexample generation.")
                        .setIsAdvanced()
                        .build());
}

bool CounterexampleGeneratorSettings::isCounterexampleSet() const {
    return this->getOption(counterexampleOptionName).getHasOptionBeenSet();
}

bool CounterexampleGeneratorSettings::isCounterexampleTypeSet() const {
    return this->getOption(counterexampleTypeOptionName).getHasOptionBeenSet();
}

bool CounterexampleGeneratorSettings::isMinimalCommandSetGenerationSet() const {
    return this->getOption(counterexampleTypeOptionName).getArgumentByName("type").getValueAsString() == "mincmd";
}

bool CounterexampleGeneratorSettings::isShortestPathGenerationSet() const {
    return this->getOption(counterexampleTypeOptionName).getArgumentByName("type").getValueAsString() == "shortestpath";
}

size_t CounterexampleGeneratorSettings::getShortestPathMaxK() const {
    return this->getOption(shortestPathMaxKOptionName).getArgumentByName("maxk").getValueAsUnsignedInteger();
}

bool CounterexampleGeneratorSettings::isUseMilpBasedMinimalCommandSetGenerationSet() const {
    return this->getOption(minimalCommandMethodOptionName).getArgumentByName("method").getValueAsString() == "milp";
}

bool CounterexampleGeneratorSettings::isUseMaxSatBasedMinimalCommandSetGenerationSet() const {
    return this->getOption(minimalCommandMethodOptionName).getArgumentByName("method").getValueAsString() == "maxsat";
}

bool CounterexampleGeneratorSettings::isEncodeReachabilitySet() const {
    return this->getOption(encodeReachabilityOptionName).getHasOptionBeenSet();
}

bool CounterexampleGeneratorSettings::isUseSchedulerCutsSet() const {
    return this->getOption(schedulerCutsOptionName).getHasOptionBeenSet();
}

bool CounterexampleGeneratorSettings::isUseDynamicConstraintsSet() const {
    return !this->getOption(noDynamicConstraintsOptionName).getHasOptionBeenSet();
}

bool CounterexampleGeneratorSettings::check() const {
    STORM_LOG_THROW(isCounterexampleSet() || !isCounterexampleTypeSet(), storm::exceptions::InvalidSettingsException,
                    "Counterexample type was set but counterexample flag '-cex' is missing.");
    // Ensure that the model was given either symbolically or explicitly.
    STORM_LOG_THROW(
        !isCounterexampleSet() || !isMinimalCommandSetGenerationSet() || storm::settings::getModule<storm::settings::modules::IOSettings>().isPrismInputSet() ||
            storm::settings::getModule<storm::settings::modules::IOSettings>().isJaniInputSet(),
        storm::exceptions::InvalidSettingsException, "For the generation of a minimal command set, the model has to be specified in the PRISM/JANI format.");

    if (isMinimalCommandSetGenerationSet()) {
        STORM_LOG_WARN_COND(isUseMaxSatBasedMinimalCommandSetGenerationSet() || !isEncodeReachabilitySet(),
                            "Encoding reachability is only available for the MaxSat-based minimal command set generation, so selecting it has no effect.");
        STORM_LOG_WARN_COND(isUseMilpBasedMinimalCommandSetGenerationSet() || !isUseSchedulerCutsSet(),
                            "Using scheduler cuts is only available for the MaxSat-based minimal command set generation, so selecting it has no effect.");
    }

    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
