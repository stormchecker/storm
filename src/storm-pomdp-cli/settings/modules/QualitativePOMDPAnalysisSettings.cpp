#include "storm-pomdp-cli/settings/modules/QualitativePOMDPAnalysisSettings.h"

#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingMemento.h"
#include "storm/settings/SettingsManager.h"

#include "storm/exceptions/InvalidArgumentException.h"

namespace storm {
namespace settings {
namespace modules {

const std::string QualitativePOMDPAnalysisSettings::moduleName = "pomdpQualitative";
const std::string exportSATCallsOption = "exportSATcallspath";
const std::string lookaheadHorizonOption = "lookaheadhorizon";
const std::string onlyDeterministicOption = "onlydeterministic";
const std::string winningRegionOption = "winningregion";
const std::string validationLevel = "validate";
const std::string lookaheadTypeOption = "lookaheadtype";
const std::string expensiveStatsOption = "allstats";
const std::string printWinningRegionOption = "printwinningregion";
const std::string exportWinningRegionOption = "exportwinningregion";
const std::string preventGraphPreprocessing = "nographprocessing";
const std::string beliefSupportMCOption = "belsupmc";
const std::string memlessSearchOption = "memlesssearch";
const std::vector<std::string> memlessSearchMethods = {"one-shot", "iterative"};

QualitativePOMDPAnalysisSettings::QualitativePOMDPAnalysisSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, memlessSearchOption, false, "Search for a qualitative memoryless scheduler")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("method", "method name")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(memlessSearchMethods))
                                         .setDefaultValueString("iterative")
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, exportSATCallsOption, false, "Export the SAT calls?.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("path", "The name of the path to which to write the models.").build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, lookaheadHorizonOption, false,
                                                   "In reachability in combination with a discrete ranking function, a lookahead is necessary.")
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("bound", "The lookahead. Use 0 for the number of states.")
                                         .setDefaultValueUnsignedInteger(0)
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, onlyDeterministicOption, false, "Search only for deterministic schedulers").setIsAdvanced().build());
    this->addOption(storm::settings::OptionBuilder(moduleName, beliefSupportMCOption, false,
                                                   "Create a symbolic description of the belief-support MDP to use for analysis purposes")
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, winningRegionOption, false, "Search for the winning region").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, lookaheadTypeOption, false, "What type to use for the ranking function")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("type", "The type.")
                                         .setDefaultValueString("real")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator({"bool", "int", "real"}))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, validationLevel, true, "Validate algorithm during runtime (for debugging)")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument(
                                         "level", "how regular to apply this validation. Use 0 for never, 1 for the end, and >=2 within computation steps.")
                                         .setDefaultValueUnsignedInteger(0)
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, expensiveStatsOption, true, "Compute all stats, even if this is expensive.").setIsAdvanced().build());
    this->addOption(storm::settings::OptionBuilder(moduleName, printWinningRegionOption, false, "Print Winning Region").setIsAdvanced().build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, exportWinningRegionOption, false, "Export the winning region.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("path", "The name of the file to which to write the winning region.").build())
            .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, preventGraphPreprocessing, true, "Prevent graph preprocessing (for debugging)").setIsAdvanced().build());
}

uint64_t QualitativePOMDPAnalysisSettings::getLookahead() const {
    return this->getOption(lookaheadHorizonOption).getArgumentByName("bound").getValueAsUnsignedInteger();
}

std::string QualitativePOMDPAnalysisSettings::getLookaheadType() const {
    return this->getOption(lookaheadTypeOption).getArgumentByName("type").getValueAsString();
}

bool QualitativePOMDPAnalysisSettings::isExportSATCallsSet() const {
    return this->getOption(exportSATCallsOption).getHasOptionBeenSet();
}

std::string QualitativePOMDPAnalysisSettings::getExportSATCallsPath() const {
    return this->getOption(exportSATCallsOption).getArgumentByName("path").getValueAsString();
}

bool QualitativePOMDPAnalysisSettings::isOnlyDeterministicSet() const {
    return this->getOption(onlyDeterministicOption).getHasOptionBeenSet();
}

bool QualitativePOMDPAnalysisSettings::isWinningRegionSet() const {
    return this->getOption(winningRegionOption).getHasOptionBeenSet();
}

bool QualitativePOMDPAnalysisSettings::isComputeOnBeliefSupportSet() const {
    return this->getOption(beliefSupportMCOption).getHasOptionBeenSet();
}

bool QualitativePOMDPAnalysisSettings::validateIntermediateSteps() const {
    return this->getOption(validationLevel).getArgumentByName("level").getValueAsUnsignedInteger() >= 2;
}
bool QualitativePOMDPAnalysisSettings::validateFinalResult() const {
    return this->getOption(validationLevel).getArgumentByName("level").getValueAsUnsignedInteger() >= 1;
}

bool QualitativePOMDPAnalysisSettings::computeExpensiveStats() const {
    return this->getOption(expensiveStatsOption).getHasOptionBeenSet();
}

std::string QualitativePOMDPAnalysisSettings::exportWinningRegionPath() const {
    return this->getOption(exportWinningRegionOption).getArgumentByName("path").getValueAsString();
}

bool QualitativePOMDPAnalysisSettings::isExportWinningRegionSet() const {
    return this->getOption(exportWinningRegionOption).getHasOptionBeenSet();
}

bool QualitativePOMDPAnalysisSettings::isPrintWinningRegionSet() const {
    return this->getOption(printWinningRegionOption).getHasOptionBeenSet();
}

bool QualitativePOMDPAnalysisSettings::isGraphPreprocessingAllowed() const {
    return this->getOption(preventGraphPreprocessing).getHasOptionBeenSet();
}

bool QualitativePOMDPAnalysisSettings::isMemlessSearchSet() const {
    return this->getOption(memlessSearchOption).getHasOptionBeenSet();
}

std::string QualitativePOMDPAnalysisSettings::getMemlessSearchMethod() const {
    return this->getOption(memlessSearchOption).getArgumentByName("method").getValueAsString();
}

void QualitativePOMDPAnalysisSettings::finalize() {}

bool QualitativePOMDPAnalysisSettings::check() const {
    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
