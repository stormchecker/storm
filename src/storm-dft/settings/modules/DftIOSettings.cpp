#include "DftIOSettings.h"

#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"

namespace storm::dft {
namespace settings {
namespace modules {

const std::string DftIOSettings::moduleName = "dftIO";
const std::string DftIOSettings::dftFileOptionName = "dftfile";
const std::string DftIOSettings::dftFileOptionShortName = "dft";
const std::string DftIOSettings::dftJsonFileOptionName = "dftfile-json";
const std::string DftIOSettings::dftJsonFileOptionShortName = "dftjson";
const std::string DftIOSettings::propExpectedTimeOptionName = "expectedtime";
const std::string DftIOSettings::propExpectedTimeOptionShortName = "mttf";
const std::string DftIOSettings::propProbabilityOptionName = "probability";
const std::string DftIOSettings::propTimeboundOptionName = "timebound";
const std::string DftIOSettings::propTimepointsOptionName = "timepoints";
const std::string DftIOSettings::minValueOptionName = "min";
const std::string DftIOSettings::maxValueOptionName = "max";
const std::string DftIOSettings::analyzeWithBdds = "bdd";
const std::string DftIOSettings::minimalCutSets = "mcs";
const std::string DftIOSettings::exportToJsonOptionName = "export-json";
const std::string DftIOSettings::exportToSmtOptionName = "export-smt";
const std::string DftIOSettings::exportToBddDotOptionName = "export-bdd-dot";
const std::string DftIOSettings::dftStatisticsOptionName = "dft-statistics";
const std::string DftIOSettings::dftStatisticsOptionShortName = "dftstats";
const std::string DftIOSettings::importanceMeasureOptionName = "importance";
const std::string DftIOSettings::variableOrderingFileOptionName = "be-order";

DftIOSettings::DftIOSettings() : ModuleSettings(moduleName) {
    this->addOption(
        storm::settings::OptionBuilder(moduleName, dftFileOptionName, false, "Parses the model given in the Galileo format.")
            .setShortName(dftFileOptionShortName)
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the file from which to read the DFT model.")
                             .addValidatorString(storm::settings::ArgumentValidatorFactory::createExistingFileValidator())
                             .build())
            .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, dftJsonFileOptionName, false, "Parses the model given in the Cytoscape JSON format.")
            .setShortName(dftJsonFileOptionShortName)
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the JSON file from which to read the DFT model.")
                             .addValidatorString(storm::settings::ArgumentValidatorFactory::createExistingFileValidator())
                             .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, propExpectedTimeOptionName, false, "Compute expected time of system failure.")
                        .setShortName(propExpectedTimeOptionShortName)
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, propProbabilityOptionName, false, "Compute probability of system failure.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, propTimeboundOptionName, false, "Compute probability of system failure up to given timebound.")
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("time", "The timebound to use.")
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterValidator(0.0))
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, propTimepointsOptionName, false,
                                                   "Compute probability of system failure up to given timebound for a set of given timepoints [starttime, "
                                                   "starttime+inc, starttime+2inc, ... ,endtime]")
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("starttime", "The timebound to start from.")
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterEqualValidator(0.0))
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("endtime", "The timebound to end with.")
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterEqualValidator(0.0))
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument("inc", "The value to increment with to get the next timepoint.")
                                         .addValidatorDouble(storm::settings::ArgumentValidatorFactory::createDoubleGreaterEqualValidator(0.0))
                                         .setDefaultValueDouble(1.0)
                                         .makeOptional()
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, minValueOptionName, false, "Compute minimal value in case of non-determinism.").build());
    this->addOption(storm::settings::OptionBuilder(moduleName, maxValueOptionName, false, "Compute maximal value in case of non-determinism.").build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, analyzeWithBdds, false, "Try to use Bdds for the analysis. Unsupportet properties will be ignored.")
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, minimalCutSets, false, "Calculate minimal cut sets.").build());

    this->addOption(storm::settings::OptionBuilder(moduleName, exportToJsonOptionName, false, "Export the model to the Cytoscape JSON format.")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the JSON file to export to.").build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, exportToSmtOptionName, false, "Export the model as SMT encoding to the smtlib2 format.")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the smtlib2 file to export to.").build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, exportToBddDotOptionName, false, "Export the model as the graph of a BDD in the dot format.")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the dot file to export to.").build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, dftStatisticsOptionName, false, "Sets whether to display DFT statistics if available.")
                        .setShortName(dftStatisticsOptionShortName)
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, importanceMeasureOptionName, false, "Calculate importance measures for all basic events in the SFT.")
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("measure", "The name of the measure. Valid values: [MIF,DIF,CIF,RAW,RRW]")
                             .addValidatorString(storm::settings::ArgumentValidatorFactory::createMultipleChoiceValidator({"MIF", "DIF", "CIF", "RAW", "RRW"}))
                             .build())
            .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, variableOrderingFileOptionName, false,
                                       "File containing the order of BEs used as variable ordering for the BDD.")
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the file from which to read the variable ordering.")
                             .addValidatorString(storm::settings::ArgumentValidatorFactory::createExistingFileValidator())
                             .build())
            .build());
}

bool DftIOSettings::isDftFileSet() const {
    return this->getOption(dftFileOptionName).getHasOptionBeenSet();
}

std::string DftIOSettings::getDftFilename() const {
    return this->getOption(dftFileOptionName).getArgumentByName("filename").getValueAsString();
}

bool DftIOSettings::isDftJsonFileSet() const {
    return this->getOption(dftJsonFileOptionName).getHasOptionBeenSet();
}

std::string DftIOSettings::getDftJsonFilename() const {
    return this->getOption(dftJsonFileOptionName).getArgumentByName("filename").getValueAsString();
}

bool DftIOSettings::usePropExpectedTime() const {
    return this->getOption(propExpectedTimeOptionName).getHasOptionBeenSet();
}

bool DftIOSettings::usePropProbability() const {
    return this->getOption(propProbabilityOptionName).getHasOptionBeenSet();
}

bool DftIOSettings::usePropTimebound() const {
    return this->getOption(propTimeboundOptionName).getHasOptionBeenSet();
}

double DftIOSettings::getPropTimebound() const {
    return this->getOption(propTimeboundOptionName).getArgumentByName("time").getValueAsDouble();
}

bool DftIOSettings::usePropTimepoints() const {
    return this->getOption(propTimepointsOptionName).getHasOptionBeenSet();
}

std::vector<double> DftIOSettings::getPropTimepoints() const {
    double starttime = this->getOption(propTimepointsOptionName).getArgumentByName("starttime").getValueAsDouble();
    double endtime = this->getOption(propTimepointsOptionName).getArgumentByName("endtime").getValueAsDouble();
    double inc = this->getOption(propTimepointsOptionName).getArgumentByName("inc").getValueAsDouble();
    std::vector<double> timepoints;
    for (double time = starttime; time <= endtime; time += inc) {
        timepoints.push_back(time);
    }
    return timepoints;
}

bool DftIOSettings::isComputeMinimalValue() const {
    return this->getOption(minValueOptionName).getHasOptionBeenSet();
}

bool DftIOSettings::isComputeMaximalValue() const {
    return this->getOption(maxValueOptionName).getHasOptionBeenSet();
}

bool DftIOSettings::isAnalyzeWithBdds() const {
    return this->getOption(analyzeWithBdds).getHasOptionBeenSet();
}

bool DftIOSettings::isMinimalCutSets() const {
    return this->getOption(minimalCutSets).getHasOptionBeenSet();
}

bool DftIOSettings::isExportToJson() const {
    return this->getOption(exportToJsonOptionName).getHasOptionBeenSet();
}

std::string DftIOSettings::getExportJsonFilename() const {
    return this->getOption(exportToJsonOptionName).getArgumentByName("filename").getValueAsString();
}

bool DftIOSettings::isExportToSmt() const {
    return this->getOption(exportToSmtOptionName).getHasOptionBeenSet();
}

std::string DftIOSettings::getExportSmtFilename() const {
    return this->getOption(exportToSmtOptionName).getArgumentByName("filename").getValueAsString();
}

bool DftIOSettings::isExportToBddDot() const {
    return this->getOption(exportToBddDotOptionName).getHasOptionBeenSet();
}

std::string DftIOSettings::getExportBddDotFilename() const {
    return this->getOption(exportToBddDotOptionName).getArgumentByName("filename").getValueAsString();
}

bool DftIOSettings::isShowDftStatisticsSet() const {
    return this->getOption(dftStatisticsOptionName).getHasOptionBeenSet();
}

bool DftIOSettings::isImportanceMeasureSet() const {
    return this->getOption(importanceMeasureOptionName).getHasOptionBeenSet();
}

std::string DftIOSettings::getImportanceMeasure() const {
    return this->getOption(importanceMeasureOptionName).getArgumentByName("measure").getValueAsString();
}

bool DftIOSettings::isVariableOrderingFileSet() const {
    return this->getOption(variableOrderingFileOptionName).getHasOptionBeenSet();
}

std::string DftIOSettings::getVariableOrderingFilename() const {
    return this->getOption(variableOrderingFileOptionName).getArgumentByName("filename").getValueAsString();
}

void DftIOSettings::finalize() {}

bool DftIOSettings::check() const {
    // Ensure that at most one input file is set
    STORM_LOG_THROW(!isDftFileSet() || !isDftJsonFileSet(), storm::exceptions::InvalidSettingsException,
                    "The DFT may be either given in Galileo or JSON format, but not both.");
    // Ensure that at most one of min or max is set
    STORM_LOG_THROW(!isComputeMinimalValue() || !isComputeMaximalValue(), storm::exceptions::InvalidSettingsException, "Min and max can not both be set.");
    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm::dft
