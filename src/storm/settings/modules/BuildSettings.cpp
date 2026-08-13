#include "storm/settings/modules/BuildSettings.h"

#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/parser/CSVParser.h"
#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string BuildSettings::moduleName = "build";

const std::string explorationOrderOptionName = "explorder";
const std::string explorationOrderOptionShortName = "eo";
const std::string explorationChecksOptionName = "explchecks";
const std::string explorationChecksOptionShortName = "ec";
const std::string prismCompatibilityOptionName = "prismcompat";
const std::string prismCompatibilityOptionShortName = "pc";
const std::string dontFixDeadlockOptionName = "nofixdl";
const std::string dontFixDeadlockOptionShortName = "ndl";
const std::string noBuildOptionName = "nobuild";
const std::string fullModelBuildOptionName = "buildfull";
const std::string applyNoMaxProgAssumptionOptionName = "nomaxprog";
const std::string checkZenoOptionName = "check-zeno";
const std::string buildChoiceLabelOptionName = "buildchoicelab";
const std::string buildChoiceOriginsOptionName = "buildchoiceorig";
const std::string buildStateValuationsOptionName = "buildstateval";
const std::string buildObservationValuationsOptionName = "buildobsval";
const std::string buildAllLabelsOptionName = "build-all-labels";
const std::string buildOutOfBoundsStateOptionName = "build-out-of-bounds-state";
const std::string buildOverlappingGuardsLabelOptionName = "build-overlapping-guards-label";
const std::string noSimplifyOptionName = "no-simplify";
const std::string bitsForUnboundedVariablesOptionName = "int-bits";
const std::string performLocationElimination = "location-elimination";
const std::string explorationStateLimitOptionName = "state-limit";

BuildSettings::BuildSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, prismCompatibilityOptionName, false,
                                                   "Enables PRISM compatibility. This may be necessary to process some PRISM models.")
                        .setShortName(prismCompatibilityOptionShortName)
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, dontFixDeadlockOptionName, false,
                                                   "If the model contains deadlock states, they are `fixed` by attaching self-loops. This option allows "
                                                   "disabling this operation, which may then violate prerequisites of various verification routines.")
                        .setShortName(dontFixDeadlockOptionShortName)
                        .setIsAdvanced()
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, fullModelBuildOptionName, false, "If set, include all rewards and labels.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, applyNoMaxProgAssumptionOptionName, false,
                                                   "If set, the maximum progress assumption is not applied while building the model (relevant for MAs)")
                        .setIsAdvanced()
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, checkZenoOptionName, false, "If set, the built Markov automaton is checked for the presence of Zeno cycles.")
            .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, buildChoiceLabelOptionName, false, "If set, also build the choice labels").setIsAdvanced().build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, buildChoiceOriginsOptionName, false,
                                       "If set, also build information that for each choice indicates the part(s) of the input that yielded the choice.")
            .setIsAdvanced()
            .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, buildStateValuationsOptionName, false, "If set, also build the state valuations").setIsAdvanced().build());
    this->addOption(storm::settings::OptionBuilder(moduleName, buildObservationValuationsOptionName, false,
                                                   "If set, also build the observation valuations (only relevant for POMDPs)")
                        .setIsAdvanced()
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, buildAllLabelsOptionName, false, "If set, build all labels").setIsAdvanced().build());
    this->addOption(storm::settings::OptionBuilder(moduleName, noBuildOptionName, false, "If set, do not build the model.").setIsAdvanced().build());

    std::vector<std::string> explorationOrders = {"dfs", "bfs"};
    this->addOption(storm::settings::OptionBuilder(moduleName, explorationOrderOptionName, false, "Sets which exploration order to use.")
                        .setShortName(explorationOrderOptionShortName)
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the exploration order to choose.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(explorationOrders))
                                         .setDefaultValueString("bfs")
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, explorationChecksOptionName, false,
                                                   "If set, additional checks (if available) are performed during model exploration to debug the model.")
                        .setShortName(explorationChecksOptionShortName)
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, buildOutOfBoundsStateOptionName, false, "If set, a state for out-of-bounds valuations is added")
                        .setIsAdvanced()
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, buildOverlappingGuardsLabelOptionName, false,
                                                   "For states where multiple guards are enabled, we add a label (for debugging DTMCs)")
                        .setIsAdvanced()
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, noSimplifyOptionName, false, "If set, simplification PRISM input is disabled.").setIsAdvanced().build());
    this->addOption(storm::settings::OptionBuilder(moduleName, bitsForUnboundedVariablesOptionName, false,
                                                   "Sets the number of bits that is used for unbounded integer variables.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("number", "The number of bits.")
                                         .addValidatorUnsignedInteger(ArgumentValidatorFactory::createUnsignedRangeValidatorExcluding(0, 63))
                                         .setDefaultValueUnsignedInteger(32)
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, performLocationElimination, false,
                                                   "If set, location elimination will be performed before the model is built.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument(
                                         "location-heuristic", "If this number of locations is reached, no further unfolding will be performed")
                                         .setDefaultValueUnsignedInteger(10)
                                         .makeOptional()
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument(
                                         "edges-heuristic", "Determines how many new edges may be created by a single elimination")
                                         .setDefaultValueUnsignedInteger(10000)
                                         .makeOptional()
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, explorationStateLimitOptionName, false,
                                                   "Potentially stopped exploration once the specified number of states is exceeded.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("number", "states to explore before stopping.").build())
                        .build());
}

bool BuildSettings::isExplorationOrderSet() const {
    return this->getOption(explorationOrderOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isPrismCompatibilityEnabled() const {
    return this->getOption(prismCompatibilityOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isDontFixDeadlocksSet() const {
    return this->getOption(dontFixDeadlockOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isBuildFullModelSet() const {
    return this->getOption(fullModelBuildOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isNoBuildModelSet() const {
    return this->getOption(noBuildOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isApplyNoMaximumProgressAssumptionSet() const {
    return this->getOption(applyNoMaxProgAssumptionOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isCheckZenoSet() const {
    return this->getOption(checkZenoOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isBuildChoiceLabelsSet() const {
    return this->getOption(buildChoiceLabelOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isBuildChoiceOriginsSet() const {
    return this->getOption(buildChoiceOriginsOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isBuildStateValuationsSet() const {
    return this->getOption(buildStateValuationsOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isBuildObservationValuationsSet() const {
    return this->getOption(buildObservationValuationsOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isBuildOutOfBoundsStateSet() const {
    return this->getOption(buildOutOfBoundsStateOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isAddOverlappingGuardsLabelSet() const {
    return this->getOption(buildOverlappingGuardsLabelOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isBuildAllLabelsSet() const {
    return this->getOption(buildAllLabelsOptionName).getHasOptionBeenSet();
}

storm::builder::ExplorationOrder BuildSettings::getExplorationOrder() const {
    std::string explorationOrderAsString = this->getOption(explorationOrderOptionName).getArgumentByName("name").getValueAsString();
    if (explorationOrderAsString == "dfs") {
        return storm::builder::ExplorationOrder::Dfs;
    } else if (explorationOrderAsString == "bfs") {
        return storm::builder::ExplorationOrder::Bfs;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown exploration order '" << explorationOrderAsString << "'.");
}

bool BuildSettings::isExplorationChecksSet() const {
    return this->getOption(explorationChecksOptionName).getHasOptionBeenSet();
}

bool BuildSettings::isNoSimplifySet() const {
    return this->getOption(noSimplifyOptionName).getHasOptionBeenSet();
}

uint64_t BuildSettings::getBitsForUnboundedVariables() const {
    return this->getOption(bitsForUnboundedVariablesOptionName).getArgumentByName("number").getValueAsUnsignedInteger();
}

bool BuildSettings::isLocationEliminationSet() const {
    return this->getOption(performLocationElimination).getHasOptionBeenSet();
}

uint64_t BuildSettings::getLocationEliminationLocationHeuristic() const {
    return this->getOption(performLocationElimination).getArgumentByName("location-heuristic").getValueAsUnsignedInteger();
}

uint64_t BuildSettings::getLocationEliminationEdgesHeuristic() const {
    return this->getOption(performLocationElimination).getArgumentByName("edges-heuristic").getValueAsUnsignedInteger();
}

bool BuildSettings::isExplorationStateLimitSet() const {
    return this->getOption(explorationStateLimitOptionName).getHasOptionBeenSet();
}

uint64_t BuildSettings::getExplorationStateLimit() const {
    return this->getOption(explorationStateLimitOptionName).getArgumentByName("number").getValueAsUnsignedInteger();
}

}  // namespace modules

}  // namespace settings
}  // namespace storm
