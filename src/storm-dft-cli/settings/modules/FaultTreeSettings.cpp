#include "FaultTreeSettings.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/parser/CSVParser.h"
#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"

namespace storm::dft {
namespace settings {
namespace modules {

const std::string FaultTreeSettings::moduleName = "dft";
const std::string FaultTreeSettings::noSymmetryReductionOptionName = "nosymmetryreduction";
const std::string FaultTreeSettings::noSymmetryReductionOptionShortName = "nosymred";
const std::string FaultTreeSettings::modularisationOptionName = "modularisation";
const std::string FaultTreeSettings::disableDCOptionName = "disabledc";
const std::string FaultTreeSettings::allowDCRelevantOptionName = "allowdcrelevant";
const std::string FaultTreeSettings::relevantEventsOptionName = "relevantevents";
const std::string FaultTreeSettings::addLabelsClaimingOptionName = "labels-claiming";
const std::string FaultTreeSettings::approximationErrorOptionName = "approximation";
const std::string FaultTreeSettings::approximationErrorOptionShortName = "approx";
const std::string FaultTreeSettings::approximationHeuristicOptionName = "approximationheuristic";
const std::string FaultTreeSettings::maxDepthOptionName = "maxdepth";
const std::string FaultTreeSettings::firstDependencyOptionName = "firstdep";
const std::string FaultTreeSettings::uniqueFailedBEOptionName = "uniquefailedbe";
#ifdef STORM_HAVE_Z3
const std::string FaultTreeSettings::solveWithSmtOptionName = "smt";
#endif
const std::string FaultTreeSettings::chunksizeOptionName = "chunksize";
const std::string FaultTreeSettings::mttfPrecisionName = "mttf-precision";
const std::string FaultTreeSettings::mttfStepsizeName = "mttf-stepsize";
const std::string FaultTreeSettings::mttfAlgorithmName = "mttf-algorithm";

FaultTreeSettings::FaultTreeSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, noSymmetryReductionOptionName, false, "Do not exploit symmetric structure of model.")
                        .setShortName(noSymmetryReductionOptionShortName)
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, modularisationOptionName, false, "Use modularisation (not applicable for expected time).").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, disableDCOptionName, false, "Disable Don't Care propagation.").build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, firstDependencyOptionName, false, "Avoid non-determinism by always taking the first possible dependency.")
            .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, relevantEventsOptionName, false, "Specifies the relevant events from the DFT.")
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("values",
                                                                                "A comma separated list of names of relevant events. 'all' marks all events as "
                                                                                "relevant, The default '' marks only the top level event as relevant.")
                             .setDefaultValueString("")
                             .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, allowDCRelevantOptionName, false, "Allow Don't Care propagation for relevant events.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, addLabelsClaimingOptionName, false, "Add labels representing claiming operations.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, approximationErrorOptionName, false, "Approximation error allowed.")
                        .setShortName(approximationErrorOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("error", "The relative approximation error to use.")
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterEqualValidator(0.0))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, approximationHeuristicOptionName, false, "Set the heuristic used for approximation.")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("heuristic", "The name of the heuristic used for approximation.")
                                         .setDefaultValueString("depth")
                                         .addValidatorString(storm::settings::ArgumentValidatorFactory::createMultipleChoiceValidator(
                                             {"depth", "probability", "bounddifference"}))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, maxDepthOptionName, false, "Maximal depth for state space exploration.")
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("depth", "The maximal depth.").build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, uniqueFailedBEOptionName, false, "Use a unique constantly failed BE.").build());
#ifdef STORM_HAVE_Z3
    this->addOption(storm::settings::OptionBuilder(moduleName, solveWithSmtOptionName, true, "Solve the DFT with SMT.").build());
#endif
    this->addOption(storm::settings::OptionBuilder(moduleName, chunksizeOptionName, false, "Calculate probabilies in chunks.")
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument(
                                         "chunksize", "The size of the chunks used to calculate probabilities. Set to 0 for maximal size.")
                                         .setDefaultValueUnsignedInteger(1)
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, mttfPrecisionName, false,
                                                   "The precision used for detecting convergence of the iterative MTTF approximation method.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("value", "The precision to achieve.")
                                         .setDefaultValueDouble(1e-12)
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleRangeValidatorExcluding(0.0, 1.0))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, mttfStepsizeName, false,
                                                   "The stepsize used to iterativly approximate the integral in the MTTF approximation method.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("value", "The stepsize to use.")
                                         .setDefaultValueDouble(1e-10)
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleRangeValidatorExcluding(0.0, 1.0))
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, mttfAlgorithmName, false, "The algorithm used to approximate the MTTF.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("algorithm", "The algorithm to use.")
                             .addValidatorString(storm::settings::ArgumentValidatorFactory::createMultipleChoiceValidator({"proceeding", "variableChange"}))
                             .setDefaultValueString("proceeding")
                             .build())
            .build());
}

bool FaultTreeSettings::useSymmetryReduction() const {
    return !this->getOption(noSymmetryReductionOptionName).getHasOptionBeenSet();
}

bool FaultTreeSettings::useModularisation() const {
    return this->getOption(modularisationOptionName).getHasOptionBeenSet();
}

bool FaultTreeSettings::isDisableDC() const {
    return this->getOption(disableDCOptionName).getHasOptionBeenSet();
}

bool FaultTreeSettings::isAllowDCForRelevantEvents() const {
    return this->getOption(allowDCRelevantOptionName).getHasOptionBeenSet();
}

bool FaultTreeSettings::areRelevantEventsSet() const {
    return this->getOption(relevantEventsOptionName).getHasOptionBeenSet() &&
           (this->getOption(relevantEventsOptionName).getArgumentByName("values").getValueAsString() != "");
}

std::vector<std::string> FaultTreeSettings::getRelevantEvents() const {
    return storm::parser::parseCommaSeperatedValues(this->getOption(relevantEventsOptionName).getArgumentByName("values").getValueAsString());
}

bool FaultTreeSettings::isAddLabelsClaiming() const {
    return this->getOption(addLabelsClaimingOptionName).getHasOptionBeenSet();
}

bool FaultTreeSettings::isApproximationErrorSet() const {
    return this->getOption(approximationErrorOptionName).getHasOptionBeenSet();
}

double FaultTreeSettings::getApproximationError() const {
    return this->getOption(approximationErrorOptionName).getArgumentByName("error").getValueAsDouble();
}

storm::dft::builder::ApproximationHeuristic FaultTreeSettings::getApproximationHeuristic() const {
    std::string heuristicAsString = this->getOption(approximationHeuristicOptionName).getArgumentByName("heuristic").getValueAsString();
    if (heuristicAsString == "depth") {
        return storm::dft::builder::ApproximationHeuristic::DEPTH;
    } else if (heuristicAsString == "probability") {
        return storm::dft::builder::ApproximationHeuristic::PROBABILITY;
    } else if (heuristicAsString == "bounddifference") {
        return storm::dft::builder::ApproximationHeuristic::BOUNDDIFFERENCE;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Illegal value '" << heuristicAsString << "' set as heuristic for approximation.");
}

bool FaultTreeSettings::isMaxDepthSet() const {
    return this->getOption(maxDepthOptionName).getHasOptionBeenSet();
}

uint_fast64_t FaultTreeSettings::getMaxDepth() const {
    return this->getOption(maxDepthOptionName).getArgumentByName("depth").getValueAsUnsignedInteger();
}

bool FaultTreeSettings::isTakeFirstDependency() const {
    return this->getOption(firstDependencyOptionName).getHasOptionBeenSet();
}

bool FaultTreeSettings::isUniqueFailedBE() const {
    return this->getOption(uniqueFailedBEOptionName).getHasOptionBeenSet();
}

#ifdef STORM_HAVE_Z3

bool FaultTreeSettings::solveWithSMT() const {
    return this->getOption(solveWithSmtOptionName).getHasOptionBeenSet();
}

#endif

bool FaultTreeSettings::isChunksizeSet() const {
    return this->getOption(chunksizeOptionName).getHasOptionBeenSet();
}

size_t FaultTreeSettings::getChunksize() const {
    return this->getOption(chunksizeOptionName).getArgumentByName("chunksize").getValueAsUnsignedInteger();
}

double FaultTreeSettings::getMttfPrecision() const {
    return this->getOption(mttfPrecisionName).getArgumentByName("value").getValueAsDouble();
}

double FaultTreeSettings::getMttfStepsize() const {
    return this->getOption(mttfStepsizeName).getArgumentByName("value").getValueAsDouble();
}

std::string FaultTreeSettings::getMttfAlgorithm() const {
    return this->getOption(mttfAlgorithmName).getArgumentByName("algorithm").getValueAsString();
}

void FaultTreeSettings::finalize() {}

bool FaultTreeSettings::check() const {
    // Ensure that disableDC and relevantEvents are not set at the same time
    STORM_LOG_THROW(!isDisableDC() || !areRelevantEventsSet(), storm::exceptions::InvalidSettingsException, "DisableDC and relevantSets can not both be set.");
    STORM_LOG_THROW(!isMaxDepthSet() || getApproximationHeuristic() == storm::dft::builder::ApproximationHeuristic::DEPTH,
                    storm::exceptions::InvalidSettingsException, "Maximal depth requires approximation heuristic depth.");
    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm::dft
