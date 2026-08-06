#include "storm/settings/modules/MultiObjectiveSettings.h"

#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/ArgumentValidators.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

namespace storm {
namespace settings {
namespace modules {

const std::string MultiObjectiveSettings::moduleName = "multiobjective";
const std::string MultiObjectiveSettings::methodOptionName = "method";
const std::string MultiObjectiveSettings::exportPlotOptionName = "exportplot";
const std::string MultiObjectiveSettings::precisionOptionName = "precision";
const std::string MultiObjectiveSettings::weightedSumApproximationTradeoffOptionName = "approxtradeoff";
const std::string MultiObjectiveSettings::maxStepsOptionName = "maxsteps";
const std::string MultiObjectiveSettings::schedulerRestrictionOptionName = "purescheds";
const std::string MultiObjectiveSettings::printResultsOptionName = "printres";
const std::string MultiObjectiveSettings::encodingOptionName = "encoding";

MultiObjectiveSettings::MultiObjectiveSettings() : ModuleSettings(moduleName) {
    std::vector<std::string> methods = {"pcaa", "constraintbased"};
    this->addOption(storm::settings::OptionBuilder(moduleName, methodOptionName, true, "The method to be used for multi objective model checking.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("name", "The name of the method to use.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(methods))
                                         .setDefaultValueString("pcaa")
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, exportPlotOptionName, true, "Saves data for plotting of pareto curves and achievable values.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument(
                                         "directory", "A path to an existing directory in which the results will be saved.")
                                         .build())
                        .build());
    std::vector<std::string> precTypes = {"abs", "reldiff"};
    this->addOption(
        storm::settings::OptionBuilder(moduleName, precisionOptionName, true, "The precision used for the approximation of numerical- and pareto queries.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("value", "The precision.")
                             .setDefaultValueDouble(1e-04)
                             .addValidatorDouble(ArgumentValidatorFactory::createDoubleGreaterEqualValidator(0.0))
                             .build())
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("type", "The type of precision.")
                             .setDefaultValueString("abs")
                             .makeOptional()
                             .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(precTypes))
                             .build())
            .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, weightedSumApproximationTradeoffOptionName, true,
                                       "Sets the fraction of the approximation error that is allowed during weighted sum optimization in pcaa method.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("gamma", "the tradeoff factor.")
                             .addValidatorDouble(ArgumentValidatorFactory::createDoubleRangeValidatorExcluding(0.0, 1.0))
                             .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, maxStepsOptionName, true,
                                                   "Aborts the computation after the given number of refinement steps (= computed pareto optimal points).")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument(
                                         "value", "the threshold for the number of refinement steps to be performed.")
                                         .build())
                        .build());
    std::vector<std::string> memoryPatterns = {"positional", "goalmemory", "arbitrary", "counter"};
    this->addOption(
        storm::settings::OptionBuilder(moduleName, schedulerRestrictionOptionName, false,
                                       "Restricts the class of considered schedulers to non-randomized schedulers with the provided memory pattern.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("memorypattern", "The pattern of the memory.")
                             .setDefaultValueString("positional")
                             .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(memoryPatterns))
                             .makeOptional()
                             .build())
            .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("memorystates",
                                                                                         "The Number of memory states (only if supported by the pattern).")
                             .setDefaultValueUnsignedInteger(0)
                             .makeOptional()
                             .build())
            .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, printResultsOptionName, true, "Prints intermediate results of the computation to standard output.")
            .setIsAdvanced()
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, encodingOptionName, true, "The preferred for encoding for constraint-based methods.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("style", "The main type of encoding.")
                                         .setDefaultValueString("auto")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator({"auto", "classic", "flow"}))
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("constraints", "type of constraints.")
                                         .setDefaultValueString("bigm")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator({"bigm", "indicator"}))
                                         .makeOptional()
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("bscc", "bscc encoding.")
                                         .setDefaultValueString("flow")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator({"flow", "order"}))
                                         .makeOptional()
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createBooleanArgument("redundant", "enable redundant bscc constraints.")
                                         .setDefaultValueBoolean(false)
                                         .makeOptional()
                                         .build())
                        .build());
}

storm::modelchecker::multiobjective::MultiObjectiveMethod MultiObjectiveSettings::getMultiObjectiveMethod() const {
    std::string methodAsString = this->getOption(methodOptionName).getArgumentByName("name").getValueAsString();
    if (methodAsString == "pcaa") {
        return storm::modelchecker::multiobjective::MultiObjectiveMethod::Pcaa;
    } else {
        STORM_LOG_ASSERT(methodAsString == "constraintbased", "Unexpected method name for multi objective model checking method.");
        return storm::modelchecker::multiobjective::MultiObjectiveMethod::ConstraintBased;
    }
}

bool MultiObjectiveSettings::isExportPlotSet() const {
    return this->getOption(exportPlotOptionName).getHasOptionBeenSet();
}

std::string MultiObjectiveSettings::getExportPlotDirectory() const {
    std::string result = this->getOption(exportPlotOptionName).getArgumentByName("directory").getValueAsString();
    if (result.back() != '/') {
        result.push_back('/');
    }
    return result;
}

double MultiObjectiveSettings::getPrecision() const {
    return this->getOption(precisionOptionName).getArgumentByName("value").getValueAsDouble();
}

bool MultiObjectiveSettings::getPrecisionRelativeToDiff() const {
    return this->getOption(precisionOptionName).getArgumentByName("type").getValueAsString() == "reldiff";
}

bool MultiObjectiveSettings::getPrecisionAbsolute() const {
    return this->getOption(precisionOptionName).getArgumentByName("type").getValueAsString() == "abs";
}

bool MultiObjectiveSettings::isWeightedSumApproximationTradeoffSet() const {
    return this->getOption(weightedSumApproximationTradeoffOptionName).getHasOptionBeenSet();
}

double MultiObjectiveSettings::getWeightedSumApproximationTradeoff() const {
    return this->getOption(weightedSumApproximationTradeoffOptionName).getArgumentByName("gamma").getValueAsDouble();
}

bool MultiObjectiveSettings::isMaxStepsSet() const {
    return this->getOption(maxStepsOptionName).getHasOptionBeenSet();
}

uint_fast64_t MultiObjectiveSettings::getMaxSteps() const {
    return this->getOption(maxStepsOptionName).getArgumentByName("value").getValueAsUnsignedInteger();
}

bool MultiObjectiveSettings::hasSchedulerRestriction() const {
    return this->getOption(schedulerRestrictionOptionName).getHasOptionBeenSet();
}

storm::storage::SchedulerClass MultiObjectiveSettings::getSchedulerRestriction() const {
    storm::storage::SchedulerClass result;
    result.setIsDeterministic(true);

    std::string pattern = this->getOption(schedulerRestrictionOptionName).getArgumentByName("memorypattern").getValueAsString();
    uint64_t states = this->getOption(schedulerRestrictionOptionName).getArgumentByName("memorystates").getValueAsUnsignedInteger();
    if (pattern == "positional") {
        result.setPositional();
        STORM_LOG_THROW(states <= 1, storm::exceptions::IllegalArgumentException,
                        "The number of memory states should not be provided for the given memory pattern.");
    } else if (pattern == "goalmemory") {
        result.setMemoryPattern(storm::storage::SchedulerClass::MemoryPattern::GoalMemory);
        STORM_LOG_THROW(states == 0, storm::exceptions::IllegalArgumentException,
                        "The number of memory states should not be provided for the given memory pattern.");
    } else if (pattern == "arbitrary") {
        STORM_LOG_THROW(states > 0, storm::exceptions::IllegalArgumentException,
                        "Invalid number of memory states for provided pattern. Please specify a positive number.");
        result.setMemoryPattern(storm::storage::SchedulerClass::MemoryPattern::Arbitrary);
        result.setMemoryStates(states);
    } else if (pattern == "counter") {
        STORM_LOG_THROW(states > 0, storm::exceptions::IllegalArgumentException,
                        "Invalid number of memory states for provided pattern. Please specify a positive number.");
        result.setMemoryPattern(storm::storage::SchedulerClass::MemoryPattern::Counter);
        result.setMemoryStates(states);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Invalid memory pattern: " + pattern + ".");
    }
    return result;
}

bool MultiObjectiveSettings::isPrintResultsSet() const {
    return this->getOption(printResultsOptionName).getHasOptionBeenSet();
}

bool MultiObjectiveSettings::isClassicEncodingSet() const {
    return this->getOption(encodingOptionName).getArgumentByName("style").getValueAsString() == "classic";
}

bool MultiObjectiveSettings::isFlowEncodingSet() const {
    return this->getOption(encodingOptionName).getArgumentByName("style").getValueAsString() == "flow";
}

bool MultiObjectiveSettings::isAutoEncodingSet() const {
    return this->getOption(encodingOptionName).getArgumentByName("style").getValueAsString() == "auto";
}

bool MultiObjectiveSettings::isBsccDetectionViaFlowConstraintsSet() const {
    return this->getOption(encodingOptionName).getArgumentByName("bscc").getValueAsString() == "flow";
}

bool MultiObjectiveSettings::isBsccDetectionViaOrderConstraintsSet() const {
    return this->getOption(encodingOptionName).getArgumentByName("bscc").getValueAsString() == "order";
}

bool MultiObjectiveSettings::isBigMConstraintsSet() const {
    return this->getOption(encodingOptionName).getArgumentByName("constraints").getValueAsString() == "bigm";
}

bool MultiObjectiveSettings::isIndicatorConstraintsSet() const {
    return this->getOption(encodingOptionName).getArgumentByName("constraints").getValueAsString() == "indicator";
}

bool MultiObjectiveSettings::isRedundantBsccConstraintsSet() const {
    return this->getOption(encodingOptionName).getArgumentByName("redundant").getValueAsBoolean();
}

bool MultiObjectiveSettings::check() const {
    std::shared_ptr<storm::settings::ArgumentValidator<std::string>> validator = ArgumentValidatorFactory::createWritableFileValidator();

    if (isExportPlotSet()) {
        if (!(validator->isValid(getExportPlotDirectory() + "boundaries.csv") && validator->isValid(getExportPlotDirectory() + "overapproximation.csv") &&
              validator->isValid(getExportPlotDirectory() + "underapproximation.csv") && validator->isValid(getExportPlotDirectory() + "paretopoints.csv"))) {
            return false;
        }
    }

    if (hasSchedulerRestriction()) {
        getSchedulerRestriction();
    }

    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
