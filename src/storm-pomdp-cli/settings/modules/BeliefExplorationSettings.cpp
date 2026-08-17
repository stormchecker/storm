#include "storm-pomdp-cli/settings/modules/BeliefExplorationSettings.h"

#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/OptionBuilder.h"

namespace storm::settings::modules {

const std::string BeliefExplorationSettings::moduleName = "beliefExploration";

const std::string explorationTimeLimitOption = "exploration-time";
const std::string resolutionOption = "resolution";
const std::string clipGridResolutionOption = "clip-resolution";
const std::string sizeThresholdOption = "size-threshold";
const std::string triangulationModeOption = "triangulationmode";
const std::string clippingOption = "use-clipping";
const std::string cutZeroGapOption = "cut-zero-gap";
const std::string inexactPreprocessingOption = "inexact-preprocessing";
const std::string beliefMdpNumberTypeOption = "belief-mdp-number-type";
std::vector<std::string> const beliefMdpNumberTypes = {"double", "rational", "match"};

BeliefExplorationSettings::BeliefExplorationSettings() : ModuleSettings(moduleName) {
    this->addOption(
        storm::settings::OptionBuilder(moduleName, explorationTimeLimitOption, false, "Sets after which time no further states shall be explored.")
            .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("time", "In seconds.").setDefaultValueUnsignedInteger(0).build())
            .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, resolutionOption, false, "Sets the resolution of the discretization")
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("resolution", "the resolution (higher means more precise)")
                                         .setDefaultValueUnsignedInteger(2)
                                         .addValidatorUnsignedInteger(storm::settings::ArgumentValidatorFactory::createUnsignedGreaterValidator(0))
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, clipGridResolutionOption, false, "Sets the resolution of the clipping grid")
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("resolution", "the resolution (higher means more precise)")
                                         .setDefaultValueUnsignedInteger(2)
                                         .addValidatorUnsignedInteger(storm::settings::ArgumentValidatorFactory::createUnsignedGreaterValidator(0))
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, sizeThresholdOption, false,
                                                   "Sets how many beliefs are explored in the unfolding before approximations are applied.")
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument(
                                         "threshold", "number of beliefs to explore (higher means more precise, 0 means automatic/heuristic choice)")
                                         .setDefaultValueUnsignedInteger(0)
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, triangulationModeOption, false, "Sets how to triangulate beliefs when discretizing.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "the triangulation mode")
                                         .setDefaultValueString("dynamic")
                                         .addValidatorString(storm::settings::ArgumentValidatorFactory::createMultipleChoiceValidator({"dynamic", "static"}))
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, clippingOption, false, "If this is set, unfolding will use grid clipping in addition to cut-offs.").build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, cutZeroGapOption, false, "Cut beliefs where the gap between over- and underapproximation is 0.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, inexactPreprocessingOption, false,
                                                   "If this is set, the POMDP will be analysed using floating point arithmetic for preprocessing. This speeds "
                                                   "up computations, but can lead to inaccurate results.")
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, beliefMdpNumberTypeOption, false, "Sets the number type to use for generated belief MDPs")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("type", "Type to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(beliefMdpNumberTypes))
                                         .setDefaultValueString("match")
                                         .build())
                        .build());
}

uint64_t BeliefExplorationSettings::getExplorationTimeLimit() const {
    return this->getOption(explorationTimeLimitOption).getArgumentByName("time").getValueAsUnsignedInteger();
}

uint64_t BeliefExplorationSettings::getResolutionInit() const {
    return this->getOption(resolutionOption).getArgumentByName("resolution").getValueAsUnsignedInteger();
}

uint64_t BeliefExplorationSettings::getClippingGridResolution() const {
    return this->getOption(clipGridResolutionOption).getArgumentByName("resolution").getValueAsUnsignedInteger();
}

uint64_t BeliefExplorationSettings::getSizeThresholdInit() const {
    return this->getOption(sizeThresholdOption).getArgumentByName("threshold").getValueAsUnsignedInteger();
}

bool BeliefExplorationSettings::isDynamicTriangulationModeSet() const {
    return this->getOption(triangulationModeOption).getArgumentByName("value").getValueAsString() == "dynamic";
}
bool BeliefExplorationSettings::isStaticTriangulationModeSet() const {
    return this->getOption(triangulationModeOption).getArgumentByName("value").getValueAsString() == "static";
}

bool BeliefExplorationSettings::isBeliefMDPNumberTypeDouble() const {
    return this->getOption(beliefMdpNumberTypeOption).getArgumentByName("type").getValueAsString() == "double";
}

bool BeliefExplorationSettings::isBeliefMDPNumberTypeRational() const {
    return this->getOption(beliefMdpNumberTypeOption).getArgumentByName("type").getValueAsString() == "rational";
}

bool BeliefExplorationSettings::isBeliefMDPNumberTypeMatch() const {
    return this->getOption(beliefMdpNumberTypeOption).getArgumentByName("type").getValueAsString() == "match";
}

bool BeliefExplorationSettings::isUseClippingSet() const {
    return this->getOption(clippingOption).getHasOptionBeenSet();
}

bool BeliefExplorationSettings::isCutZeroGapSet() const {
    return this->getOption(cutZeroGapOption).getHasOptionBeenSet();
}

bool BeliefExplorationSettings::isInexactPreprocessingSet() const {
    return this->getOption(inexactPreprocessingOption).getHasOptionBeenSet();
}
}  // namespace storm::settings::modules
