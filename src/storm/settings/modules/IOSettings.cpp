#include "storm/settings/modules/IOSettings.h"

#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/parser/CSVParser.h"
#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string IOSettings::moduleName = "io";
const std::string IOSettings::exportDotOptionName = "exportdot";
const std::string IOSettings::exportDotMaxWidthOptionName = "dot-maxwidth";
const std::string IOSettings::exportBuildOptionName = "exportbuild";
const std::string IOSettings::exportExplicitOptionName = "exportexplicit";
const std::string IOSettings::exportDdOptionName = "exportdd";
const std::string IOSettings::exportJaniDotOptionName = "exportjanidot";
const std::string IOSettings::exportCdfOptionName = "exportcdf";
const std::string IOSettings::exportCdfOptionShortName = "cdf";
const std::string IOSettings::exportSchedulerOptionName = "exportscheduler";
const std::string IOSettings::exportCheckResultOptionName = "exportresult";
const std::string IOSettings::exportCompressionOptionName = "compression";
const std::string IOSettings::exportDigitsOptionName = "digits";
const std::string IOSettings::explicitOptionName = "explicit";
const std::string IOSettings::explicitOptionShortName = "exp";
const std::string IOSettings::explicitDrnOptionName = "explicit-drn";
const std::string IOSettings::explicitDrnOptionShortName = "drn";
const std::string IOSettings::explicitUmbOptionName = "explicit-umb";
const std::string IOSettings::explicitUmbOptionShortName = "umb";
const std::string IOSettings::explicitImcaOptionName = "explicit-imca";
const std::string IOSettings::explicitImcaOptionShortName = "imca";
const std::string IOSettings::prismInputOptionName = "prism";
const std::string IOSettings::janiInputOptionName = "jani";
const std::string IOSettings::prismToJaniOptionName = "prism2jani";

const std::string IOSettings::transitionRewardsOptionName = "transrew";
const std::string IOSettings::stateRewardsOptionName = "staterew";
const std::string IOSettings::choiceLabelingOptionName = "choicelab";
const std::string IOSettings::constantsOptionName = "constants";
const std::string IOSettings::constantsOptionShortName = "const";

const std::string IOSettings::janiPropertyOptionName = "janiproperty";
const std::string IOSettings::janiPropertyOptionShortName = "jprop";
const std::string IOSettings::propertyOptionName = "prop";
const std::string IOSettings::propertyOptionShortName = "prop";
const std::string IOSettings::steadyStateDistrOptionName = "steadystate";
const std::string IOSettings::expectedVisitingTimesOptionName = "expvisittimes";

const std::string IOSettings::qvbsInputOptionName = "qvbs";
const std::string IOSettings::qvbsInputOptionShortName = "qvbs";
const std::string IOSettings::qvbsRootOptionName = "qvbsroot";
const std::string IOSettings::propertiesAsMultiOptionName = "propsasmulti";
const std::string IOSettings::cvarOptionName = "cvar";

const std::string IOSettings::uncertaintyResolutionModeName = "uncertainty-resolution";

const std::string preventDRNPlaceholderOptionName = "no-drn-placeholders";

IOSettings::IOSettings() : ModuleSettings(moduleName) {
    this->addOption(
        storm::settings::OptionBuilder(moduleName, exportDotOptionName, false,
                                       "If given, the loaded model will be written to the specified file in the dot format.")
            .setIsAdvanced()
            .addArgument(
                storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the file to which the model is to be written.").build())
            .build());
    this->addOption(storm::settings::OptionBuilder(
                        moduleName, exportDotMaxWidthOptionName, false,
                        "The maximal width for labels in the dot format. For longer lines a linebreak is inserted. Value 0 represents no linebreaks.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument(
                                         "width", "The maximal line width for the dot format. Default is 0 meaning no linebreaks.")
                                         .setDefaultValueUnsignedInteger(0)
                                         .build())
                        .build());
    std::vector<std::string> exportFormats({"auto", "dot", "drdd", "drn", "json", "umb"});
    this->addOption(
        storm::settings::OptionBuilder(moduleName, exportBuildOptionName, false, "Exports the built model to a file.")
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("file", "The output file.").build())
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("format", "The output format. 'auto' detects from the file extension.")
                             .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(exportFormats))
                             .setDefaultValueString("auto")
                             .makeOptional()
                             .build())
            .build());

    std::vector<std::string> compressionModes({"default", "none", "gzip", "xz"});
    this->addOption(storm::settings::OptionBuilder(moduleName, exportCompressionOptionName, false, "Configures compression of exported files (if supported).")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("mode", "The preferred compression mode.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(compressionModes))
                                         .setDefaultValueString("default")
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, exportDigitsOptionName, false, "Sets number of output digits of export (if supported).")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("num", "Number of digits.").build())
                        .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, exportJaniDotOptionName, false,
                                       "If given, the loaded jani model will be written to the specified file in the dot format.")
            .setIsAdvanced()
            .addArgument(
                storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the file to which the model is to be written.").build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, exportCdfOptionName, false,
                                                   "Exports the cumulative density function for reward bounded properties into a .csv file.")
                        .setIsAdvanced()
                        .setShortName(exportCdfOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument(
                                         "directory", "A path to an existing directory where the cdf files will be stored.")
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, exportSchedulerOptionName, false,
                                       "Exports the choices of an optimal scheduler to the given file (if supported by engine).")
            .setIsAdvanced()
            .addArgument(
                storm::settings::ArgumentBuilder::createStringArgument("filename", "The output file. Use file extension '.json' to export in json.").build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, exportCheckResultOptionName, false,
                                                   "Exports the result to a given file (if supported by engine). The export will be in json.")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The output file.").build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, exportExplicitOptionName, false,
                                       "If given, the loaded model will be written to the specified file in the drn format.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "the name of the file to which the model is to be writen.").build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, preventDRNPlaceholderOptionName, true, "If given, the exported DRN contains no placeholders")
                        .setIsAdvanced()
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, exportDdOptionName, false,
                                       "If given, the loaded model will be written to the specified file in the drdd format.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "the name of the file to which the model is to be writen.").build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, explicitOptionName, false, "Parses the model given in an explicit (sparse) representation.")
                        .setShortName(explicitOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("transition filename",
                                                                                            "The name of the file from which to read the transitions.")
                                         .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("labeling filename",
                                                                                            "The name of the file from which to read the state labeling.")
                                         .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, explicitUmbOptionName, false, "Parses the model given in the UMB format. The file can be compressed.")
            .setShortName(explicitUmbOptionShortName)
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("umb location", "The location of the umb encoding.").build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, explicitDrnOptionName, false, "Parses the model given in the DRN format.")
                        .setShortName(explicitDrnOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("drn filename", "The name of the DRN file containing the model.")
                                         .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, explicitImcaOptionName, false, "Parses the model given in the IMCA format.")
                        .setShortName(explicitImcaOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("imca filename", "The name of the imca file containing the model.")
                                         .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, prismInputOptionName, false, "Parses the model given in the PRISM format.")
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the file from which to read the PRISM input.")
                             .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                             .build())
            .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, janiInputOptionName, false, "Parses the model given in the JANI format.")
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The name of the file from which to read the JANI input.")
                             .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                             .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, prismToJaniOptionName, false, "If set, the input PRISM model is transformed to JANI.")
                        .setIsAdvanced()
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, propertyOptionName, false, "Specifies the properties to be checked on the model.")
            .setShortName(propertyOptionShortName)
            .addArgument(
                storm::settings::ArgumentBuilder::createStringArgument("property or filename", "The formula or the file containing the formulas.").build())
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filter", "The names of the properties to check.")
                             .setDefaultValueString("all")
                             .makeOptional()
                             .build())
            .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, transitionRewardsOptionName, false,
                                                   "If given, the transition rewards are read from this file and added to the explicit model. Note that this "
                                                   "requires the model to be given as an explicit model (i.e., via --" +
                                                       explicitOptionName + ").")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The file from which to read the transition rewards.")
                                         .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, stateRewardsOptionName, false,
                                                   "If given, the state rewards are read from this file and added to the explicit model. Note that this "
                                                   "requires the model to be given as an explicit model (i.e., via --" +
                                                       explicitOptionName + ").")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The file from which to read the state rewards.")
                                         .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                                         .build())
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, choiceLabelingOptionName, false,
                                                   "If given, the choice labels are read from this file and added to the explicit model. Note that this "
                                                   "requires the model to be given as an explicit model (i.e., via --" +
                                                       explicitOptionName + ").")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("filename", "The file from which to read the choice labels.")
                                         .addValidatorString(ArgumentValidatorFactory::createExistingFileValidator())
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(
            moduleName, constantsOptionName, false,
            "Specifies the constant replacements to use in symbolic models. Note that this requires the model to be given as an symbolic model (i.e., via --" +
                prismInputOptionName + " or --" + janiInputOptionName + ").")
            .setShortName(constantsOptionShortName)
            .addArgument(
                storm::settings::ArgumentBuilder::createStringArgument("values", "A comma separated list of constants and their value, e.g. a=1,b=2,c=3.")
                    .setDefaultValueString("")
                    .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, janiPropertyOptionName, false,
                                                   "Specifies the properties from the jani model (given by --" + janiInputOptionName + ") to be checked.")
                        .setShortName(janiPropertyOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("values", "A comma separated list of properties to be checked")
                                         .setDefaultValueString("")
                                         .makeOptional()
                                         .build())
                        .build());
    std::vector<std::string> steadyStateDistrAlgorithms({"auto", "eqsys", "evt", "classic"});
    this->addOption(
        storm::settings::OptionBuilder(moduleName, steadyStateDistrOptionName, false,
                                       "Computes the steady state distribution. Result can be exported using --" + exportCheckResultOptionName + ".")
            .addArgument(
                storm::settings::ArgumentBuilder::createStringArgument("algorithm", "The used algorithm. 'auto' chooses according to accuracy requirements.")
                    .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(steadyStateDistrAlgorithms))
                    .setDefaultValueString("auto")
                    .makeOptional()
                    .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, expectedVisitingTimesOptionName, false,
                                                   "Computes the expected number of times each state is visited (DTMC) or the expected time spend in each "
                                                   "state (CTMC). Result can be exported using --" +
                                                       exportCheckResultOptionName + ".")
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, qvbsInputOptionName, false, "Selects a model from the Quantitative Verification Benchmark Set.")
                        .setShortName(qvbsInputOptionShortName)
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("model", "The short model name as in the benchmark set.").build())
                        .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("instance-index", "The selected instance of this model.")
                                         .setDefaultValueUnsignedInteger(0)
                                         .makeOptional()
                                         .build())
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument(
                                         "filter", "The comma separated list of property names to check. Omit to check all, \"\" to check none.")
                                         .setDefaultValueString("")
                                         .makeOptional()
                                         .build())
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, propertiesAsMultiOptionName, false,
                                                   "If set, the selected properties are interpreted as a multi-objective formula.")
                        .setIsAdvanced()
                        .build());

    this->addOption(storm::settings::OptionBuilder(moduleName, cvarOptionName, false, "Computes the conditional value-at-risk for the selected property.")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("alpha", "The size of the tail.").build())
                        .build());

    std::vector<std::string> uncertaintyResolutionModes = {"minimize", "maximize", "robust", "cooperative", "min", "max"};
    this->addOption(storm::settings::OptionBuilder(moduleName, uncertaintyResolutionModeName, false, "Mode to resolve the uncertainty (intervals)")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("mode", "Mode to resolve the uncertainty (intervals) by nature.")
                                         .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(uncertaintyResolutionModes))
                                         .build())
                        .build());

#ifdef STORM_HAVE_QVBS
    std::string qvbsRootDefault = STORM_QVBS_ROOT;
#else
    std::string qvbsRootDefault = "";
#endif
    this->addOption(storm::settings::OptionBuilder(moduleName, qvbsRootOptionName, false,
                                                   "Specifies the root directory of the Quantitative Verification Benchmark Set. Default can be set in CMAKE.")
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument("path", "The path.").setDefaultValueString(qvbsRootDefault).build())
                        .build());
}

bool IOSettings::isExportDotSet() const {
    return this->getOption(exportDotOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getExportDotFilename() const {
    return this->getOption(exportDotOptionName).getArgumentByName("filename").getValueAsString();
}

size_t IOSettings::getExportDotMaxWidth() const {
    return this->getOption(exportDotMaxWidthOptionName).getArgumentByName("width").getValueAsUnsignedInteger();
}

bool IOSettings::isExportBuildSet() const {
    return this->getOption(exportBuildOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getExportBuildFilename() const {
    return this->getOption(exportBuildOptionName).getArgumentByName("file").getValueAsString();
}

storm::io::ModelExportFormat IOSettings::getExportBuildFormat() const {
    auto format = this->getOption(exportBuildOptionName).getArgumentByName("format").getValueAsString();
    if (format == "auto") {
        return storm::io::getModelExportFormatFromFileExtension(getExportBuildFilename());
    } else {
        return storm::io::getModelExportFormatFromString(format);
    }
}

bool IOSettings::isCompressionSet() const {
    return this->getOption(exportCompressionOptionName).getHasOptionBeenSet();
}

storm::io::CompressionMode IOSettings::getCompressionMode() const {
    auto mode = this->getOption(exportCompressionOptionName).getArgumentByName("mode").getValueAsString();
    return storm::io::getCompressionModeFromString(mode);
}

bool IOSettings::isExportDigitsSet() const {
    return this->getOption(exportDigitsOptionName).getHasOptionBeenSet();
}

std::size_t IOSettings::getExportDigits() const {
    return this->getOption(exportDigitsOptionName).getArgumentByName("num").getValueAsUnsignedInteger();
}

bool IOSettings::isExportJaniDotSet() const {
    return this->getOption(exportJaniDotOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getExportJaniDotFilename() const {
    return this->getOption(exportJaniDotOptionName).getArgumentByName("filename").getValueAsString();
}

bool IOSettings::isExportExplicitSet() const {
    return this->getOption(exportExplicitOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getExportExplicitFilename() const {
    return this->getOption(exportExplicitOptionName).getArgumentByName("filename").getValueAsString();
}

bool IOSettings::isExplicitExportPlaceholdersDisabled() const {
    return this->getOption(preventDRNPlaceholderOptionName).getHasOptionBeenSet();
}

bool IOSettings::isExportDdSet() const {
    return this->getOption(exportDdOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getExportDdFilename() const {
    return this->getOption(exportDdOptionName).getArgumentByName("filename").getValueAsString();
}

bool IOSettings::isExportCdfSet() const {
    return this->getOption(exportCdfOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getExportCdfDirectory() const {
    std::string result = this->getOption(exportCdfOptionName).getArgumentByName("directory").getValueAsString();
    if (result.back() != '/') {
        result.push_back('/');
    }
    return result;
}

bool IOSettings::isExportSchedulerSet() const {
    return this->getOption(exportSchedulerOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getExportSchedulerFilename() const {
    return this->getOption(exportSchedulerOptionName).getArgumentByName("filename").getValueAsString();
}

bool IOSettings::isExportCheckResultSet() const {
    return this->getOption(exportCheckResultOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getExportCheckResultFilename() const {
    return this->getOption(exportCheckResultOptionName).getArgumentByName("filename").getValueAsString();
}

bool IOSettings::isExplicitSet() const {
    return this->getOption(explicitOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getTransitionFilename() const {
    return this->getOption(explicitOptionName).getArgumentByName("transition filename").getValueAsString();
}

std::string IOSettings::getLabelingFilename() const {
    return this->getOption(explicitOptionName).getArgumentByName("labeling filename").getValueAsString();
}

bool IOSettings::isExplicitDRNSet() const {
    return this->getOption(explicitDrnOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getExplicitDRNFilename() const {
    return this->getOption(explicitDrnOptionName).getArgumentByName("drn filename").getValueAsString();
}

bool IOSettings::isExplicitUmbSet() const {
    return this->getOption(explicitUmbOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getExplicitUmbFilename() const {
    return this->getOption(explicitUmbOptionName).getArgumentByName("umb location").getValueAsString();
}

bool IOSettings::isExplicitIMCASet() const {
    return this->getOption(explicitImcaOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getExplicitIMCAFilename() const {
    return this->getOption(explicitImcaOptionName).getArgumentByName("imca filename").getValueAsString();
}

bool IOSettings::isPrismInputSet() const {
    return this->getOption(prismInputOptionName).getHasOptionBeenSet();
}

bool IOSettings::isPrismOrJaniInputSet() const {
    return isJaniInputSet() || isPrismInputSet();
}

bool IOSettings::isPrismToJaniSet() const {
    return this->getOption(prismToJaniOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getPrismInputFilename() const {
    return this->getOption(prismInputOptionName).getArgumentByName("filename").getValueAsString();
}

bool IOSettings::isJaniInputSet() const {
    return this->getOption(janiInputOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getJaniInputFilename() const {
    return this->getOption(janiInputOptionName).getArgumentByName("filename").getValueAsString();
}

bool IOSettings::isTransitionRewardsSet() const {
    return this->getOption(transitionRewardsOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getTransitionRewardsFilename() const {
    return this->getOption(transitionRewardsOptionName).getArgumentByName("filename").getValueAsString();
}

bool IOSettings::isStateRewardsSet() const {
    return this->getOption(stateRewardsOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getStateRewardsFilename() const {
    return this->getOption(stateRewardsOptionName).getArgumentByName("filename").getValueAsString();
}

bool IOSettings::isChoiceLabelingSet() const {
    return this->getOption(choiceLabelingOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getChoiceLabelingFilename() const {
    return this->getOption(choiceLabelingOptionName).getArgumentByName("filename").getValueAsString();
}

bool IOSettings::isConstantsSet() const {
    return this->getOption(constantsOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getConstantDefinitionString() const {
    return this->getOption(constantsOptionName).getArgumentByName("values").getValueAsString();
}

bool IOSettings::isJaniPropertiesSet() const {
    return this->getOption(janiPropertyOptionName).getHasOptionBeenSet();
}

bool IOSettings::areJaniPropertiesSelected() const {
    return this->getOption(janiPropertyOptionName).getHasOptionBeenSet() &&
           (this->getOption(janiPropertyOptionName).getArgumentByName("values").getValueAsString() != "");
}

std::vector<std::string> IOSettings::getSelectedJaniProperties() const {
    return storm::parser::parseCommaSeperatedValues(this->getOption(janiPropertyOptionName).getArgumentByName("values").getValueAsString());
}

bool IOSettings::isPropertySet() const {
    return this->getOption(propertyOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getProperty() const {
    return this->getOption(propertyOptionName).getArgumentByName("property or filename").getValueAsString();
}

std::string IOSettings::getPropertyFilter() const {
    return this->getOption(propertyOptionName).getArgumentByName("filter").getValueAsString();
}

bool IOSettings::isCvarSet() const {
    return this->getOption(cvarOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getCvarAlpha() const {
    return this->getOption(cvarOptionName).getArgumentByName("alpha").getValueAsString();
}

bool IOSettings::isComputeSteadyStateDistributionSet() const {
    return this->getOption(steadyStateDistrOptionName).getHasOptionBeenSet();
}

storm::SteadyStateDistributionAlgorithm IOSettings::getSteadyStateDistributionAlgorithm() const {
    auto alg = this->getOption(steadyStateDistrOptionName).getArgumentByName("algorithm").getValueAsString();
    if (alg == "auto") {
        return storm::SteadyStateDistributionAlgorithm::Automatic;
    } else if (alg == "eqsys") {
        return storm::SteadyStateDistributionAlgorithm::EquationSystem;
    } else if (alg == "classic") {
        return storm::SteadyStateDistributionAlgorithm::Classic;
    } else {
        STORM_LOG_ASSERT(alg == "evt", "Unexpected algorithm type.");
        return storm::SteadyStateDistributionAlgorithm::ExpectedVisitingTimes;
    }
}

bool IOSettings::isComputeExpectedVisitingTimesSet() const {
    return this->getOption(expectedVisitingTimesOptionName).getHasOptionBeenSet();
}

bool IOSettings::isQvbsInputSet() const {
    return this->getOption(qvbsInputOptionName).getHasOptionBeenSet();
}

std::string IOSettings::getQvbsModelName() const {
    return this->getOption(qvbsInputOptionName).getArgumentByName("model").getValueAsString();
}

uint64_t IOSettings::getQvbsInstanceIndex() const {
    return this->getOption(qvbsInputOptionName).getArgumentByName("instance-index").getValueAsUnsignedInteger();
}

boost::optional<std::vector<std::string>> IOSettings::getQvbsPropertyFilter() const {
    std::string listAsString = this->getOption(qvbsInputOptionName).getArgumentByName("filter").getValueAsString();
    if (listAsString == "") {
        if (this->getOption(qvbsInputOptionName).getArgumentByName("filter").wasSetFromDefaultValue()) {
            return boost::none;
        } else {
            return std::vector<std::string>();
        }
    } else {
        return storm::parser::parseCommaSeperatedValues(listAsString);
    }
}

std::string IOSettings::getQvbsRoot() const {
    auto const& path = this->getOption(qvbsRootOptionName).getArgumentByName("path");
#ifndef STORM_HAVE_QVBS
    STORM_LOG_THROW(this->getOption(qvbsRootOptionName).getHasOptionBeenSet(), storm::exceptions::InvalidSettingsException,
                    "QVBS Root is not specified. Either use the --" + qvbsRootOptionName + " option or specify it within CMAKE.");
#endif
    return path.getValueAsString();
}

bool IOSettings::isPropertiesAsMultiSet() const {
    return this->getOption(propertiesAsMultiOptionName).getHasOptionBeenSet();
}

UncertaintyResolutionModeSetting IOSettings::getUncertaintyResolutionMode() const {
    std::string uncertaintyResolutionModeString = this->getOption(uncertaintyResolutionModeName).getArgumentByName("mode").getValueAsString();

    if (uncertaintyResolutionModeString == "minimize" || uncertaintyResolutionModeString == "min") {
        return UncertaintyResolutionModeSetting::Minimize;
    } else if (uncertaintyResolutionModeString == "maximize" || uncertaintyResolutionModeString == "max") {
        return UncertaintyResolutionModeSetting::Maximize;
    } else if (uncertaintyResolutionModeString == "robust") {
        return UncertaintyResolutionModeSetting::Robust;
    } else if (uncertaintyResolutionModeString == "cooperative") {
        return UncertaintyResolutionModeSetting::Cooperative;
    } else if (uncertaintyResolutionModeString == "both") {
        STORM_LOG_ASSERT(false, "Uncertainty resolution mode 'both' not yet implemented.");
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Uncertainty resolution mode 'both' not yet implemented.");
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException, "Unknown nature resolution mode '" << uncertaintyResolutionModeString << "'.");
}

bool IOSettings::isUncertaintyResolutionModeSet() const {
    return this->getOption(uncertaintyResolutionModeName).getHasOptionBeenSet();
}

void IOSettings::finalize() {
    STORM_LOG_WARN_COND(!isExportDdSet(), "Option '--" << moduleName << ":" << exportDdOptionName << "' is depreciated. Use '--" << moduleName << ":"
                                                       << exportBuildOptionName << "' instead.");
    STORM_LOG_WARN_COND(!isExportDotSet(), "Option '--" << moduleName << ":" << exportDotOptionName << "' is depreciated. Use '--" << moduleName << ":"
                                                        << exportBuildOptionName << "' instead.");
    STORM_LOG_WARN_COND(!isExportExplicitSet(), "Option '--" << moduleName << ":" << exportExplicitOptionName << "' is depreciated. Use '--" << moduleName
                                                             << ":" << exportBuildOptionName << "' instead.");
}

bool IOSettings::check() const {
    // Ensure that at most one symbolic input model is given.
    uint64_t numSymbolicInputs = isJaniInputSet() ? 1 : 0;
    numSymbolicInputs += isPrismInputSet() ? 1 : 0;
    numSymbolicInputs += isQvbsInputSet() ? 1 : 0;
    STORM_LOG_THROW(numSymbolicInputs <= 1, storm::exceptions::InvalidSettingsException, "Multiple symbolic input models.");
    STORM_LOG_THROW(!isExportJaniDotSet() || isJaniInputSet() || isQvbsInputSet(), storm::exceptions::InvalidSettingsException,
                    "Jani-to-dot export is only available for jani models.");

    // Ensure that not two explicit input models were given.
    uint64_t numExplicitInputs = isExplicitSet() ? 1 : 0;
    numExplicitInputs += isExplicitDRNSet() ? 1 : 0;
    numExplicitInputs += isExplicitIMCASet() ? 1 : 0;
    STORM_LOG_THROW(numExplicitInputs <= 1, storm::exceptions::InvalidSettingsException, "Multiple explicit input models.");

    // Ensure that the model was given either symbolically or explicitly.
    STORM_LOG_THROW(numSymbolicInputs + numExplicitInputs <= 1, storm::exceptions::InvalidSettingsException,
                    "The model may be either given in an explicit or a symbolic format (PRISM or JANI), but not both.");

    // Make sure PRISM-to-JANI conversion is only set if the actual input is in PRISM format.
    STORM_LOG_THROW(!isPrismToJaniSet() || isPrismInputSet(), storm::exceptions::InvalidSettingsException,
                    "For the transformation from PRISM to JANI, the input model must be given in the prism format.");

    return true;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
