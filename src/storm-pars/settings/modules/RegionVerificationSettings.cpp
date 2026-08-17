#include "storm-pars/settings/modules/RegionVerificationSettings.h"

#include "storm-pars/modelchecker/region/RegionSplitEstimateKind.h"
#include "storm-pars/modelchecker/region/RegionSplittingStrategy.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/OptionBuilder.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/utility/macros.h"

namespace storm::settings::modules {

const std::string RegionVerificationSettings::moduleName = "regionverif";
const std::string splittingThresholdName = "splitting-threshold";
const std::string splittingHeuristicName = "splitting-heuristic";
const std::string estimateMethodName = "estimate-method";
const std::string checkEngineOptionName = "engine";

RegionVerificationSettings::RegionVerificationSettings() : ModuleSettings(moduleName) {
    this->addOption(
        storm::settings::OptionBuilder(moduleName, splittingThresholdName, false, "Sets the threshold for number of parameters in which to split regions.")
            .addArgument(
                storm::settings::ArgumentBuilder::createIntegerArgument("splitting-threshold", "The threshold for splitting, should be an integer > 0").build())
            .build());

    std::vector<std::string> strategies = {"estimate", "roundrobin", "default"};
    this->addOption(storm::settings::OptionBuilder(moduleName, splittingHeuristicName, false, "Sets which strategy is used for splitting regions.")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the strategy to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(strategies))
                                         .setDefaultValueString("default")
                                         .build())
                        .build());

    std::vector<std::string> estimates = {"delta", "distance", "deltaweighted", "derivative"};
    this->addOption(storm::settings::OptionBuilder(moduleName, estimateMethodName, false,
                                                   "Sets which estimate strategy is used for splitting regions (if splitting-stratgegy is estimate).")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the strategy to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(estimates))
                                         .setDefaultValueString("delta")
                                         .build())
                        .build());

    std::vector<std::string> engines = {"pl", "exactpl", "validatingpl", "robustpl"};
    this->addOption(storm::settings::OptionBuilder(moduleName, checkEngineOptionName, true, "Sets which engine is used for analyzing regions.")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the engine to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(engines))
                                         .setDefaultValueString("pl")
                                         .build())
                        .build());
}

int RegionVerificationSettings::getSplittingThreshold() const {
    return this->getOption(splittingThresholdName).getArgumentByName("splitting-threshold").getValueAsInteger();
}

bool RegionVerificationSettings::isSplittingThresholdSet() const {
    return this->getOption(splittingThresholdName).getHasOptionBeenSet();
}

storm::modelchecker::RegionCheckEngine RegionVerificationSettings::getRegionCheckEngine() const {
    std::string engineString = this->getOption(checkEngineOptionName).getArgumentByName("name").getValueAsString();

    storm::modelchecker::RegionCheckEngine result;
    if (engineString == "pl") {
        result = storm::modelchecker::RegionCheckEngine::ParameterLifting;
    } else if (engineString == "exactpl") {
        result = storm::modelchecker::RegionCheckEngine::ExactParameterLifting;
    } else if (engineString == "validatingpl") {
        result = storm::modelchecker::RegionCheckEngine::ValidatingParameterLifting;
    } else if (engineString == "robustpl") {
        result = storm::modelchecker::RegionCheckEngine::RobustParameterLifting;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown region check engine '" << engineString << "'.");
    }

    return result;
}

storm::modelchecker::RegionSplittingStrategy::Heuristic RegionVerificationSettings::getRegionSplittingHeuristic() const {
    std::string strategyString = this->getOption(splittingHeuristicName).getArgumentByName("name").getValueAsString();

    storm::modelchecker::RegionSplittingStrategy::Heuristic result;
    if (strategyString == "default") {
        result = storm::modelchecker::RegionSplittingStrategy::Heuristic::Default;
    } else if (strategyString == "estimate") {
        result = storm::modelchecker::RegionSplittingStrategy::Heuristic::EstimateBased;
    } else if (strategyString == "roundrobin") {
        result = storm::modelchecker::RegionSplittingStrategy::Heuristic::RoundRobin;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown splitting strategy '" << strategyString << "'.");
    }

    STORM_LOG_ERROR_COND(!getRegionSplittingEstimateMethod() || result == modelchecker::RegionSplittingStrategy::Heuristic::EstimateBased,
                         "Setting an estimate method requires setting the estimate splitting strategy");
    return result;
}

std::optional<storm::modelchecker::RegionSplitEstimateKind> RegionVerificationSettings::getRegionSplittingEstimateMethod() const {
    if (!this->getOption(estimateMethodName).getHasOptionBeenSet()) {
        return std::nullopt;
    }
    std::string strategyString = this->getOption(estimateMethodName).getArgumentByName("name").getValueAsString();

    storm::modelchecker::RegionSplitEstimateKind result;
    if (strategyString == "delta") {
        result = storm::modelchecker::RegionSplitEstimateKind::StateValueDelta;
    } else if (strategyString == "deltaweighted") {
        result = storm::modelchecker::RegionSplitEstimateKind::StateValueDeltaWeighted;
    } else if (strategyString == "distance") {
        result = storm::modelchecker::RegionSplitEstimateKind::Distance;
    } else if (strategyString == "derivative") {
        result = storm::modelchecker::RegionSplitEstimateKind::Derivative;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown splitting strategy '" << strategyString << "'.");
    }

    return result;
}

}  // namespace storm::settings::modules
