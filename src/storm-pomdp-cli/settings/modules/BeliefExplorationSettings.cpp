#include "storm-pomdp-cli/settings/modules/BeliefExplorationSettings.h"

#include "storm-pomdp/modelchecker/BeliefExplorationPomdpModelCheckerOptions.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/utility/NumberTraits.h"

namespace storm {
namespace settings {
namespace modules {

const std::string BeliefExplorationSettings::moduleName = "beliefExploration";

const std::string refineOption = "refine";
const std::string explorationTimeLimitOption = "exploration-time";
const std::string resolutionOption = "resolution";
const std::string clipGridResolutionOption = "clip-resolution";
const std::string sizeThresholdOption = "size-threshold";
const std::string gapThresholdOption = "gap-threshold";
const std::string optimalChoiceValueThresholdOption = "optimal-choice-value-threshold";
const std::string observationThresholdOption = "obs-threshold";
const std::string numericPrecisionOption = "numeric-precision";
const std::string triangulationModeOption = "triangulationmode";
const std::string clippingOption = "use-clipping";
const std::string cutZeroGapOption = "cut-zero-gap";
const std::string stateEliminationCutoffOption = "state-elimination-cutoff";

BeliefExplorationSettings::BeliefExplorationSettings() : ModuleSettings(moduleName) {
    this->addOption(
        storm::settings::OptionBuilder(moduleName, refineOption, false,
                                       "Refines the result bounds until reaching either the goal precision or the refinement step limit")
            .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("prec", "The goal precision.")
                             .setDefaultValueDouble(1e-4)
                             .makeOptional()
                             .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterEqualValidator(0.0))
                             .build())
            .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("steps", "The number of allowed refinement steps (0 means no limit).")
                             .setDefaultValueUnsignedInteger(0)
                             .makeOptional()
                             .build())
            .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, explorationTimeLimitOption, false, "Sets after which time no further states shall be explored.")
            .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("time", "In seconds.").setDefaultValueUnsignedInteger(0).build())
            .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, resolutionOption, false,
                                       "Sets the resolution of the discretization and how it is increased in case of refinement")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("init", "the initial resolution (higher means more precise)")
                             .setDefaultValueUnsignedInteger(2)
                             .addValidatorUnsignedInteger(storm::settings::ArgumentValidatorFactory::createUnsignedGreaterValidator(0))
                             .build())
            .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(
                             "factor", "Multiplied to the resolution of refined observations (higher means more precise).")
                             .setDefaultValueDouble(2)
                             .makeOptional()
                             .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterValidator(1))
                             .build())
            .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, clipGridResolutionOption, false, "Sets the resolution of the clipping grid")
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("resolution", "the resolution (higher means more precise)")
                                         .setDefaultValueUnsignedInteger(2)
                                         .addValidatorUnsignedInteger(storm::settings::ArgumentValidatorFactory::createUnsignedGreaterValidator(0))
                                         .build())
                        .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, observationThresholdOption, false, "Only observations whose score is below this threshold will be refined.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("init", "initial threshold (higher means more precise")
                             .setDefaultValueDouble(0.1)
                             .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleRangeValidatorIncluding(0, 1))
                             .build())
            .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(
                             "factor", "Controlls how fast the threshold is increased in each refinement step (higher means more precise).")
                             .setDefaultValueDouble(0.1)
                             .makeOptional()
                             .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleRangeValidatorIncluding(0, 1))
                             .build())
            .build());

    this->addOption(
        storm::settings::OptionBuilder(
            moduleName, sizeThresholdOption, false,
            "Sets how many new states are explored or rewired in a refinement step and how this value is increased in case of refinement.")
            .setIsAdvanced()
            .addArgument(
                storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("init", "initial limit (higher means more precise, 0 means automatic choice)")
                    .setDefaultValueUnsignedInteger(0)
                    .build())
            .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(
                             "factor", "Before each step the new threshold is set to the current state count times this number (higher means more precise).")
                             .setDefaultValueDouble(4)
                             .makeOptional()
                             .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterEqualValidator(1))
                             .build())
            .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, gapThresholdOption, false,
                                       "Sets how large the gap between known lower- and upper bounds at a beliefstate needs to be in order to explore")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("init", "initial threshold (higher means less precise")
                             .setDefaultValueDouble(0.1)
                             .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterEqualValidator(0))
                             .build())
            .addArgument(
                storm::settings::ArgumentBuilder::createDoubleArgument("factor", "Multiplied to the gap in each refinement step (higher means less precise).")
                    .setDefaultValueDouble(0.25)
                    .makeOptional()
                    .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleRangeValidatorIncluding(0, 1))
                    .build())
            .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, optimalChoiceValueThresholdOption, false,
                                                   "Sets how much worse a sub-optimal choice can be in order to be included in the relevant explored fragment")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("init", "initial threshold (higher means more precise")
                                         .setDefaultValueDouble(1e-3)
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterEqualValidator(0))
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(
                                         "factor", "Multiplied to the threshold in each refinement step (higher means more precise).")
                                         .setDefaultValueDouble(1)
                                         .makeOptional()
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterEqualValidator(1))
                                         .build())
                        .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, numericPrecisionOption, false, "Sets the precision used to determine whether two belief-states are equal.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("value", "the precision")
                             .setDefaultValueDouble(1e-9)
                             .makeOptional()
                             .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleRangeValidatorIncluding(0, 1))
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
        storm::settings::OptionBuilder(moduleName, clippingOption, false, "If this is set, unfolding will use  (grid) clipping instead of cut-offs only.")
            .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, cutZeroGapOption, false, "Cut beliefs where the gap between over- and underapproximation is 0.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, stateEliminationCutoffOption, false,
                                                   "If this is set, an additional unfolding step for cut-off beliefs is performed.")
                        .build());
}

bool BeliefExplorationSettings::isRefineSet() const {
    return this->getOption(refineOption).getHasOptionBeenSet();
}

bool BeliefExplorationSettings::isStateEliminationCutoffSet() const {
    return this->getOption(stateEliminationCutoffOption).getHasOptionBeenSet();
}

double BeliefExplorationSettings::getRefinePrecision() const {
    return this->getOption(refineOption).getArgumentByName("prec").getValueAsDouble();
}

uint64_t BeliefExplorationSettings::getRefineStepLimit() const {
    return this->getOption(refineOption).getArgumentByName("steps").getValueAsUnsignedInteger();
}

uint64_t BeliefExplorationSettings::getExplorationTimeLimit() const {
    return this->getOption(explorationTimeLimitOption).getArgumentByName("time").getValueAsUnsignedInteger();
}

uint64_t BeliefExplorationSettings::getResolutionInit() const {
    return this->getOption(resolutionOption).getArgumentByName("init").getValueAsUnsignedInteger();
}

uint64_t BeliefExplorationSettings::getClippingGridResolution() const {
    return this->getOption(clipGridResolutionOption).getArgumentByName("resolution").getValueAsUnsignedInteger();
}

double BeliefExplorationSettings::getResolutionFactor() const {
    return this->getOption(resolutionOption).getArgumentByName("factor").getValueAsDouble();
}

uint64_t BeliefExplorationSettings::getSizeThresholdInit() const {
    return this->getOption(sizeThresholdOption).getArgumentByName("init").getValueAsUnsignedInteger();
}

double BeliefExplorationSettings::getSizeThresholdFactor() const {
    return this->getOption(sizeThresholdOption).getArgumentByName("factor").getValueAsDouble();
}

double BeliefExplorationSettings::getGapThresholdInit() const {
    return this->getOption(gapThresholdOption).getArgumentByName("init").getValueAsDouble();
}

double BeliefExplorationSettings::getGapThresholdFactor() const {
    return this->getOption(gapThresholdOption).getArgumentByName("factor").getValueAsDouble();
}

double BeliefExplorationSettings::getOptimalChoiceValueThresholdInit() const {
    return this->getOption(optimalChoiceValueThresholdOption).getArgumentByName("init").getValueAsDouble();
}

double BeliefExplorationSettings::getOptimalChoiceValueThresholdFactor() const {
    return this->getOption(optimalChoiceValueThresholdOption).getArgumentByName("factor").getValueAsDouble();
}

double BeliefExplorationSettings::getObservationScoreThresholdInit() const {
    return this->getOption(observationThresholdOption).getArgumentByName("init").getValueAsDouble();
}

double BeliefExplorationSettings::getObservationScoreThresholdFactor() const {
    return this->getOption(observationThresholdOption).getArgumentByName("factor").getValueAsDouble();
}

bool BeliefExplorationSettings::isNumericPrecisionSetFromDefault() const {
    return !this->getOption(numericPrecisionOption).getHasOptionBeenSet() ||
           this->getOption(numericPrecisionOption).getArgumentByName("value").wasSetFromDefaultValue();
}

double BeliefExplorationSettings::getNumericPrecision() const {
    return this->getOption(numericPrecisionOption).getArgumentByName("value").getValueAsDouble();
}

bool BeliefExplorationSettings::isDynamicTriangulationModeSet() const {
    return this->getOption(triangulationModeOption).getArgumentByName("value").getValueAsString() == "dynamic";
}
bool BeliefExplorationSettings::isStaticTriangulationModeSet() const {
    return this->getOption(triangulationModeOption).getArgumentByName("value").getValueAsString() == "static";
}

bool BeliefExplorationSettings::isUseClippingSet() const {
    return this->getOption(clippingOption).getHasOptionBeenSet();
}

bool BeliefExplorationSettings::isCutZeroGapSet() const {
    return this->getOption(cutZeroGapOption).getHasOptionBeenSet();
}

template<typename ValueType>
void BeliefExplorationSettings::setValuesInOptionsStruct(storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType>& options) const {
    options.refine = isRefineSet();
    options.refinePrecision = storm::utility::convertNumber<ValueType>(getRefinePrecision());
    options.refineStepLimit = getRefineStepLimit();
    options.explorationTimeLimit = getExplorationTimeLimit();

    options.clippingGridRes = getClippingGridResolution();
    options.resolutionInit = getResolutionInit();
    options.resolutionFactor = storm::utility::convertNumber<ValueType>(getResolutionFactor());
    options.sizeThresholdInit = getSizeThresholdInit();
    options.sizeThresholdFactor = storm::utility::convertNumber<ValueType>(getSizeThresholdFactor());
    options.gapThresholdInit = storm::utility::convertNumber<ValueType>(getGapThresholdInit());
    options.gapThresholdFactor = storm::utility::convertNumber<ValueType>(getGapThresholdFactor());
    options.optimalChoiceValueThresholdInit = storm::utility::convertNumber<ValueType>(getOptimalChoiceValueThresholdInit());
    options.optimalChoiceValueThresholdFactor = storm::utility::convertNumber<ValueType>(getOptimalChoiceValueThresholdFactor());
    options.obsThresholdInit = storm::utility::convertNumber<ValueType>(getObservationScoreThresholdInit());
    options.obsThresholdIncrementFactor = storm::utility::convertNumber<ValueType>(getObservationScoreThresholdFactor());
    options.useClipping = isUseClippingSet();
    options.useStateEliminationCutoff = isStateEliminationCutoffSet();

    options.numericPrecision = storm::utility::convertNumber<ValueType>(getNumericPrecision());
    if (storm::NumberTraits<ValueType>::IsExact) {
        if (isNumericPrecisionSetFromDefault()) {
            STORM_LOG_WARN_COND(storm::utility::isZero(options.numericPrecision), "Setting numeric precision to zero because exact arithmethic is used.");
            options.numericPrecision = storm::utility::zero<ValueType>();
        } else {
            STORM_LOG_WARN_COND(storm::utility::isZero(options.numericPrecision),
                                "A non-zero numeric precision was set although exact arithmethic is used. Results might be inexact.");
        }
    }
    options.dynamicTriangulation = isDynamicTriangulationModeSet();
    options.cutZeroGap = isCutZeroGapSet();
}

template void BeliefExplorationSettings::setValuesInOptionsStruct<double>(
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<double>& options) const;
template void BeliefExplorationSettings::setValuesInOptionsStruct<storm::RationalNumber>(
    storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<storm::RationalNumber>& options) const;

}  // namespace modules
}  // namespace settings
}  // namespace storm
