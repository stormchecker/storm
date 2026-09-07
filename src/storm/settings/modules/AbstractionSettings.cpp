#include "AbstractionSettings.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string AbstractionSettings::methodOptionName = "method";
const std::string AbstractionSettings::moduleName = "abstraction";
const std::string AbstractionSettings::useDecompositionOptionName = "decomposition";
const std::string AbstractionSettings::splitModeOptionName = "split";
const std::string AbstractionSettings::addAllGuardsOptionName = "all-guards";
const std::string AbstractionSettings::addInitialExpressionsOptionName = "all-inits";
const std::string AbstractionSettings::useInterpolationOptionName = "interpolation";
const std::string AbstractionSettings::precisionOptionName = "precision";
const std::string AbstractionSettings::relativeOptionName = "relative";
const std::string AbstractionSettings::pivotHeuristicOptionName = "pivot-heuristic";
const std::string AbstractionSettings::reuseResultsOptionName = "reuse";
const std::string AbstractionSettings::restrictToRelevantStatesOptionName = "relevant";
const std::string AbstractionSettings::solveModeOptionName = "solve";
const std::string AbstractionSettings::maximalAbstractionOptionName = "maxabs";
const std::string AbstractionSettings::rankRefinementPredicatesOptionName = "rankpred";
const std::string AbstractionSettings::constraintsOptionName = "constraints";
const std::string AbstractionSettings::useEagerRefinementOptionName = "eagerref";
const std::string AbstractionSettings::debugOptionName = "debug";
const std::string AbstractionSettings::injectRefinementPredicatesOptionName = "injectref";
const std::string AbstractionSettings::fixPlayer1StrategyOptionName = "fixpl1strat";
const std::string AbstractionSettings::fixPlayer2StrategyOptionName = "fixpl2strat";
const std::string AbstractionSettings::validBlockModeOptionName = "validmode";

AbstractionSettings::AbstractionSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> methods = {"games", "bisimulation", "bisim"};
    this->addOption(storm::settings::OptionBuilder(moduleName, methodOptionName, true, "Sets which abstraction-refinement method to use.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the method to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(methods))
                                         .setDefaultValueString("bisim")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, maximalAbstractionOptionName, false,
                                                   "The maximal number of abstraction to perform before solving is aborted.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("count", "The maximal abstraction count.")
                                         .setDefaultValueUnsignedInteger(20000)
                                         .build())
                        .build());

    std::vector<std::string> onOff = {"on", "off"};

    this->addOption(storm::settings::OptionBuilder(moduleName, useDecompositionOptionName, true, "Sets whether to apply decomposition during the abstraction.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value of the flag.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(onOff))
                                         .setDefaultValueString("on")
                                         .build())
                        .build());

    std::vector<std::string> splitModes = {"all", "none", "non-guard"};
    this->addOption(storm::settings::OptionBuilder(moduleName, splitModeOptionName, true, "Sets which predicates are split into atoms for the refinement.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("mode", "The mode to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(splitModes))
                                         .setDefaultValueString("all")
                                         .build())
                        .build());

    std::vector<std::string> solveModes = {"dd", "sparse"};
    this->addOption(storm::settings::OptionBuilder(moduleName, solveModeOptionName, true, "Sets how the abstractions are solved.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("mode", "The mode to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(solveModes))
                                         .setDefaultValueString("sparse")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, addAllGuardsOptionName, true, "Sets whether all guards are added as initial predicates.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value of the flag.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(onOff))
                                         .setDefaultValueString("on")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, addInitialExpressionsOptionName, true,
                                                   "Sets whether all initial expressions are added as initial predicates.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value of the flag.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(onOff))
                                         .setDefaultValueString("on")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, useInterpolationOptionName, true,
                                                   "Sets whether interpolation is to be used to eliminate spurious pivot blocks.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value of the flag.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(onOff))
                                         .setDefaultValueString("on")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, precisionOptionName, true, "The precision used for detecting convergence.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("value", "The precision to achieve.")
                                         .setDefaultValueDouble(1e-03)
                                         .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorExcluding(0.0, 1.0))
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, relativeOptionName, true, "Sets whether to use a relative termination criterion.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value of the flag.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(onOff))
                                         .setDefaultValueString("off")
                                         .build())
                        .build());

    std::vector<std::string> pivotHeuristic = {"nearest-max-dev", "most-prob-path", "max-weighted-dev"};
    this->addOption(storm::settings::OptionBuilder(moduleName, pivotHeuristicOptionName, true, "Sets the pivot selection heuristic.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of an available heuristic.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(pivotHeuristic))
                                         .setDefaultValueString("nearest-max-dev")
                                         .build())
                        .build());

    std::vector<std::string> reuseModes = {"all", "none", "qualitative", "quantitative"};
    this->addOption(storm::settings::OptionBuilder(moduleName, reuseResultsOptionName, true, "Sets whether to reuse all results.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("mode", "The mode to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(reuseModes))
                                         .setDefaultValueString("all")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, restrictToRelevantStatesOptionName, true,
                                                   "Sets whether to restrict to relevant states during the abstraction.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value of the flag.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(onOff))
                                         .setDefaultValueString("on")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, rankRefinementPredicatesOptionName, true,
                                                   "Sets whether to rank the refinement predicates if there are multiple.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value of the flag.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(onOff))
                                         .setDefaultValueString("off")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, useEagerRefinementOptionName, true, "Sets whether to refine eagerly.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value of the flag.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(onOff))
                                         .setDefaultValueString("off")
                                         .build())
                        .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, constraintsOptionName, true, "Specifies additional constraints used by the abstraction.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("constraints", "The constraints to use.").setDefaultValueString("").build())
            .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, injectRefinementPredicatesOptionName, true,
                                       "Specifies predicates used by the refinement instead of the derived predicates.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("predicates", "The (semicolon-separated) refinement predicates to use.")
                             .setDefaultValueString("")
                             .build())
            .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, fixPlayer1StrategyOptionName, true, "Sets whether to fix player 1 strategies.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value of the flag.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(onOff))
                                         .setDefaultValueString("on")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, fixPlayer2StrategyOptionName, true, "Sets whether to fix player 2 strategies.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value of the flag.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(onOff))
                                         .setDefaultValueString("on")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, debugOptionName, true, "Sets whether to enable debug mode.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value of the flag.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(onOff))
                                         .setDefaultValueString("off")
                                         .build())
                        .build());

    std::vector<std::string> validModes = {"morepreds", "blockenum"};
    this->addOption(storm::settings::OptionBuilder(moduleName, validBlockModeOptionName, true, "Sets the mode to guarantee valid blocks only.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("mode", "The mode to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(validModes))
                                         .setDefaultValueString("morepreds")
                                         .build())
                        .build());
}

AbstractionSettings::Method AbstractionSettings::getAbstractionRefinementMethod() const {
    std::string methodAsString = this->getOption(methodOptionName).getArgumentByName("name").getValueAsString();
    if (methodAsString == "games") {
        return Method::Games;
    } else if (methodAsString == "bisimulation" || methodAsString == "bisim") {
        return Method::Bisimulation;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown abstraction-refinement method '" << methodAsString << "'.");
}

bool AbstractionSettings::isUseDecompositionSet() const {
    return this->getOption(useDecompositionOptionName).getArgumentByName("value").getValueAsString() == "on";
}

AbstractionSettings::SplitMode AbstractionSettings::getSplitMode() const {
    std::string splitModeAsString = this->getOption(splitModeOptionName).getArgumentByName("mode").getValueAsString();
    if (splitModeAsString == "all") {
        return SplitMode::All;
    } else if (splitModeAsString == "none") {
        return SplitMode::None;
    } else if (splitModeAsString == "non-guard") {
        return SplitMode::NonGuard;
    }
    return SplitMode::All;
}

AbstractionSettings::SolveMode AbstractionSettings::getSolveMode() const {
    std::string solveModeAsString = this->getOption(solveModeOptionName).getArgumentByName("mode").getValueAsString();
    if (solveModeAsString == "dd") {
        return SolveMode::Dd;
    }
    return SolveMode::Sparse;
}

bool AbstractionSettings::isAddAllGuardsSet() const {
    return this->getOption(addAllGuardsOptionName).getArgumentByName("value").getValueAsString() == "on";
}

bool AbstractionSettings::isAddAllInitialExpressionsSet() const {
    return this->getOption(addInitialExpressionsOptionName).getArgumentByName("value").getValueAsString() == "on";
}

void AbstractionSettings::setAddAllGuards(bool value) {
    this->getOption(addAllGuardsOptionName).getArgumentByName("value").setFromStringValue(value ? "on" : "off");
}

void AbstractionSettings::setAddAllInitialExpressions(bool value) {
    this->getOption(addInitialExpressionsOptionName).getArgumentByName("value").setFromStringValue(value ? "on" : "off");
}

bool AbstractionSettings::isUseInterpolationSet() const {
    return this->getOption(useInterpolationOptionName).getArgumentByName("value").getValueAsString() == "on";
}

bool AbstractionSettings::isRestrictToRelevantStatesSet() const {
    return this->getOption(restrictToRelevantStatesOptionName).getArgumentByName("value").getValueAsString() == "on";
}

double AbstractionSettings::getPrecision() const {
    return this->getOption(precisionOptionName).getArgumentByName("value").getValueAsDouble();
}

bool AbstractionSettings::getRelativeTerminationCriterion() const {
    return this->getOption(relativeOptionName).getArgumentByName("value").getValueAsString() == "on";
}

AbstractionSettings::PivotSelectionHeuristic AbstractionSettings::getPivotSelectionHeuristic() const {
    std::string heuristicName = this->getOption(pivotHeuristicOptionName).getArgumentByName("name").getValueAsString();
    if (heuristicName == "nearest-max-dev") {
        return AbstractionSettings::PivotSelectionHeuristic::NearestMaximalDeviation;
    } else if (heuristicName == "most-prob-path") {
        return AbstractionSettings::PivotSelectionHeuristic::MostProbablePath;
    } else if (heuristicName == "max-weighted-dev") {
        return AbstractionSettings::PivotSelectionHeuristic::MaxWeightedDeviation;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown pivot selection heuristic '" << heuristicName << "'.");
}

AbstractionSettings::ReuseMode AbstractionSettings::getReuseMode() const {
    std::string reuseModeAsString = this->getOption(reuseResultsOptionName).getArgumentByName("mode").getValueAsString();
    if (reuseModeAsString == "all") {
        return ReuseMode::All;
    } else if (reuseModeAsString == "none") {
        return ReuseMode::None;
    } else if (reuseModeAsString == "qualitative") {
        return ReuseMode::Qualitative;
    } else if (reuseModeAsString == "quantitative") {
        return ReuseMode::Quantitative;
    }
    return ReuseMode::All;
}

uint_fast64_t AbstractionSettings::getMaximalAbstractionCount() const {
    return this->getOption(maximalAbstractionOptionName).getArgumentByName("count").getValueAsUnsignedInteger();
}

bool AbstractionSettings::isRankRefinementPredicatesSet() const {
    return this->getOption(rankRefinementPredicatesOptionName).getArgumentByName("value").getValueAsString() == "on";
}

bool AbstractionSettings::isConstraintsSet() const {
    return this->getOption(constraintsOptionName).getHasOptionBeenSet();
}

std::string AbstractionSettings::getConstraintString() const {
    return this->getOption(constraintsOptionName).getArgumentByName("constraints").getValueAsString();
}

bool AbstractionSettings::isUseEagerRefinementSet() const {
    return this->getOption(useEagerRefinementOptionName).getArgumentByName("value").getValueAsString() == "on";
}

bool AbstractionSettings::isDebugSet() const {
    return this->getOption(debugOptionName).getArgumentByName("value").getValueAsString() == "on";
}

bool AbstractionSettings::isInjectRefinementPredicatesSet() const {
    return this->getOption(injectRefinementPredicatesOptionName).getHasOptionBeenSet();
}

std::string AbstractionSettings::getInjectedRefinementPredicates() const {
    return this->getOption(injectRefinementPredicatesOptionName).getArgumentByName("predicates").getValueAsString();
}

bool AbstractionSettings::isFixPlayer1StrategySet() const {
    return this->getOption(fixPlayer1StrategyOptionName).getArgumentByName("value").getValueAsString() == "on";
}

bool AbstractionSettings::isFixPlayer2StrategySet() const {
    return this->getOption(fixPlayer2StrategyOptionName).getArgumentByName("value").getValueAsString() == "on";
}

AbstractionSettings::ValidBlockMode AbstractionSettings::getValidBlockMode() const {
    std::string modeAsString = this->getOption(validBlockModeOptionName).getArgumentByName("mode").getValueAsString();
    if (modeAsString == "morepreds") {
        return ValidBlockMode::MorePredicates;
    } else if (modeAsString == "blockenum") {
        return ValidBlockMode::BlockEnumeration;
    }
    return ValidBlockMode::MorePredicates;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
