
#include "storm-conv/api/storm-conv.h"

#include "storm-conv/settings/ConvSettings.h"
#include "storm-conv/settings/modules/ConversionGeneralSettings.h"
#include "storm-conv/settings/modules/ConversionInputSettings.h"
#include "storm-conv/settings/modules/ConversionOutputSettings.h"
#include "storm-conv/settings/modules/JaniExportSettings.h"
#include "storm-conv/settings/modules/PrismExportSettings.h"
#include "storm/settings/SettingsManager.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm/api/storm.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/initialize.h"
#include "storm/utility/macros.h"

#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/Property.h"

#include "storm-cli-utilities/cli.h"
#include "storm-cli-utilities/print.h"
#include "storm/exceptions/OptionParserException.h"

namespace storm {
namespace conv {

void setUrgentOptions() {
    // Set the correct log level
    auto const& general = storm::settings::getModule<storm::settings::modules::ConversionGeneralSettings>();
    if (general.isVerboseSet()) {
        storm::utility::setLogLevel(l3pp::LogLevel::INFO);
    }
    if (general.isDebugOutputSet()) {
        storm::utility::setLogLevel(l3pp::LogLevel::DEBUG);
    }
    if (general.isTraceOutputSet()) {
        storm::utility::setLogLevel(l3pp::LogLevel::TRACE);
    }
}

storm::utility::Stopwatch startStopwatch(std::string const& message) {
    STORM_PRINT_AND_LOG(message);
    return storm::utility::Stopwatch(true);
}

void stopStopwatch(storm::utility::Stopwatch& stopWatch) {
    stopWatch.stop();
    STORM_PRINT_AND_LOG(" done. (" << stopWatch << " seconds).\n");
}

void processPrismInputJaniOutput(storm::prism::Program const& prismProg, std::vector<storm::jani::Property> const& properties) {
    auto const& output = storm::settings::getModule<storm::settings::modules::ConversionOutputSettings>();
    auto const& input = storm::settings::getModule<storm::settings::modules::ConversionInputSettings>();
    auto const& jani = storm::settings::getModule<storm::settings::modules::JaniExportSettings>();

    auto conversionTime = startStopwatch("Converting PRISM Program to JANI model ... ");

    storm::converter::PrismToJaniConverterOptions options;
    options.allVariablesGlobal = jani.isGlobalVarsSet();
    options.suffix = "";
    options.janiOptions = storm::converter::JaniConversionOptions(jani);
    options.janiOptions.substituteConstants = true;

    // Get the name of the output file
    std::string outputFilename = "";
    if (output.isJaniOutputFilenameSet()) {
        outputFilename = output.getJaniOutputFilename();
    } else if (input.isPrismInputSet()) {
        outputFilename = input.getPrismInputFilename();
        // Remove extension if present
        auto dotPos = outputFilename.rfind('.');
        if (dotPos != std::string::npos) {
            outputFilename.erase(dotPos);
        }
        std::string suffix = "";
        if (input.isConstantsSet()) {
            suffix = input.getConstantDefinitionString();
            std::replace(suffix.begin(), suffix.end(), ',', '_');
            std::replace(suffix.begin(), suffix.end(), '=', '-');
        }
        suffix = suffix + ".jani";
        outputFilename += suffix;
    }

    // Find a good model name
    auto startOfFilename = outputFilename.rfind("/");
    if (startOfFilename == std::string::npos) {
        startOfFilename = 0;
    } else {
        ++startOfFilename;
    }
    auto endOfFilename = outputFilename.rfind(".");
    if (endOfFilename == std::string::npos) {
        endOfFilename = outputFilename.size();
    }
    options.janiOptions.modelName = outputFilename.substr(startOfFilename, endOfFilename - startOfFilename);

    auto janiModelProperties = storm::api::convertPrismToJani(prismProg, properties, options);

    stopStopwatch(conversionTime);
    auto exportingTime = startStopwatch("Exporting JANI model ... ");

    if (outputFilename != "") {
        storm::api::exportJaniToFile(janiModelProperties.first, janiModelProperties.second, outputFilename, jani.isCompactJsonSet());
        STORM_PRINT_AND_LOG("Stored to file '" << outputFilename << "'");
    }

    stopStopwatch(exportingTime);
}

void processPrismInputPrismOutput(storm::prism::Program const& prismProg, std::vector<storm::jani::Property> const& properties) {
    auto const& output = storm::settings::getModule<storm::settings::modules::ConversionOutputSettings>();
    auto const& input = storm::settings::getModule<storm::settings::modules::ConversionInputSettings>();
    auto const& prism = storm::settings::getModule<storm::settings::modules::PrismExportSettings>();

    auto conversionTime = startStopwatch("Processing Prism Program ... ");

    // Get the name of the output file
    std::string outputFilename = "";
    if (output.isPrismOutputFilenameSet()) {
        outputFilename = output.getPrismOutputFilename();
    } else if (input.isPrismInputSet()) {
        outputFilename = input.getPrismInputFilename();
        // Remove extension if present
        auto dotPos = outputFilename.rfind('.');
        if (dotPos != std::string::npos) {
            outputFilename.erase(dotPos);
        }
        std::string suffix = "";
        if (input.isConstantsSet()) {
            suffix = input.getConstantDefinitionString();
            std::replace(suffix.begin(), suffix.end(), ',', '_');
            std::replace(suffix.begin(), suffix.end(), '=', '-');
        }
        suffix += "_converted.prism";
        outputFilename += suffix;
    }

    storm::prism::Program outputProgram = prismProg;
    std::vector<storm::jani::Property> outputProperties = properties;
    storm::api::transformPrism(outputProgram, outputProperties, prism.isSimplifySet(), prism.isExportFlattenedSet());

    stopStopwatch(conversionTime);
    auto exportingTime = startStopwatch("Exporting Prism program ... ");

    if (outputFilename != "") {
        storm::api::exportPrismToFile(outputProgram, outputProperties, outputFilename);
        STORM_PRINT_AND_LOG("Stored to file '" << outputFilename << "'");
    }

    stopStopwatch(exportingTime);
}

void processPrismInput() {
    auto parsingTime = startStopwatch("Parsing PRISM input ... ");

    auto const& input = storm::settings::getModule<storm::settings::modules::ConversionInputSettings>();

    // Parse the prism program
    storm::storage::SymbolicModelDescription prismProg = storm::api::parseProgram(input.getPrismInputFilename(), input.isPrismCompatibilityEnabled(), false);

    // Parse properties (if available)
    std::vector<storm::jani::Property> properties;
    if (input.isPropertyInputSet()) {
        boost::optional<std::set<std::string>> propertyFilter = storm::api::parsePropertyFilter(input.getPropertyInputFilter());
        properties = storm::api::parsePropertiesForSymbolicModelDescription(input.getPropertyInput(), prismProg, propertyFilter);
    }

    // Set constant definitions in program
    std::string constantDefinitionString = input.getConstantDefinitionString();
    auto constantDefinitions = prismProg.parseConstantDefinitions(constantDefinitionString);
    prismProg = storm::storage::SymbolicModelDescription(prismProg.asPrismProgram().defineUndefinedConstants(constantDefinitions));
    // Substitution of constants can only be done after conversion in order to preserve formula definitions in which
    // constants appear that are renamed in some modules...

    stopStopwatch(parsingTime);

    // Branch on the type of output
    auto const& output = storm::settings::getModule<storm::settings::modules::ConversionOutputSettings>();
    if (output.isJaniOutputSet()) {
        processPrismInputJaniOutput(prismProg.asPrismProgram(), properties);
    } else if (output.isPrismOutputSet()) {
        processPrismInputPrismOutput(prismProg.asPrismProgram(), properties);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidSettingsException,
                        "There is either no outputformat specified or the provided combination of input and output format is not compatible.");
    }
}

void processJaniInputJaniOutput(storm::jani::Model const& janiModel, std::vector<storm::jani::Property> const& properties) {
    auto conversionTime = startStopwatch("Performing transformations on JANI model ... ");

    auto const& output = storm::settings::getModule<storm::settings::modules::ConversionOutputSettings>();
    auto const& input = storm::settings::getModule<storm::settings::modules::ConversionInputSettings>();
    auto const& jani = storm::settings::getModule<storm::settings::modules::JaniExportSettings>();

    storm::converter::JaniConversionOptions options(jani);

    // Get the name of the output file
    std::string outputFilename = "";
    if (output.isJaniOutputFilenameSet()) {
        outputFilename = output.getJaniOutputFilename();
    } else if (input.isJaniInputSet()) {
        outputFilename = input.getJaniInputFilename();
        // Remove extension if present
        auto dotPos = outputFilename.rfind('.');
        if (dotPos != std::string::npos) {
            outputFilename.erase(dotPos);
        }
        outputFilename += "_converted.jani";
    }

    // Get a good model name from the output filename
    auto startOfFilename = outputFilename.rfind("/");
    if (startOfFilename == std::string::npos) {
        startOfFilename = 0;
    } else {
        ++startOfFilename;
    }
    auto endOfFilename = outputFilename.rfind(".");
    if (endOfFilename == std::string::npos) {
        endOfFilename = outputFilename.size();
    }
    options.modelName = outputFilename.substr(startOfFilename, endOfFilename - startOfFilename);

    auto transformedJaniModel = janiModel;
    auto transformedProperties = properties;
    storm::api::transformJani(transformedJaniModel, transformedProperties, options);

    stopStopwatch(conversionTime);
    auto exportingTime = startStopwatch("Exporting JANI model ... ");

    if (outputFilename != "") {
        storm::api::exportJaniToFile(transformedJaniModel, transformedProperties, outputFilename, jani.isCompactJsonSet());
        STORM_PRINT_AND_LOG("Stored to file '" << outputFilename << "'");
    }

    stopStopwatch(exportingTime);
}

void processJaniInput() {
    auto parsingTime = startStopwatch("Parsing JANI input ... ");

    auto const& input = storm::settings::getModule<storm::settings::modules::ConversionInputSettings>();

    // Parse the jani model and selected properties
    boost::optional<std::vector<std::string>> janiPropertyFilter;
    if (input.isJaniPropertiesSet()) {
        if (input.areJaniPropertiesSelected()) {
            janiPropertyFilter = input.getSelectedJaniProperties();
        } else {
            janiPropertyFilter = boost::none;
        }
    } else {
        if (input.isPropertyInputSet()) {
            janiPropertyFilter = std::vector<std::string>();
        } else {
            // If no properties are selected, take the ones from the jani file.
            janiPropertyFilter = boost::none;
        }
    }
    auto janiModelProperties = storm::api::parseJaniModel(input.getJaniInputFilename(), storm::jani::getAllKnownModelFeatures(), janiPropertyFilter);

    // Parse additional properties given from command line
    std::vector<storm::jani::Property> properties = std::move(janiModelProperties.second);
    if (input.isPropertyInputSet()) {
        boost::optional<std::set<std::string>> propertyFilter = storm::api::parsePropertyFilter(input.getPropertyInputFilter());
        auto additionalProperties = storm::api::parsePropertiesForSymbolicModelDescription(input.getPropertyInput(), janiModelProperties.first, propertyFilter);
        properties.insert(properties.end(), additionalProperties.begin(), additionalProperties.end());
    }

    storm::storage::SymbolicModelDescription symbDescr(janiModelProperties.first);

    // Substitute constant definitions in model and properties.
    std::string constantDefinitionString = input.getConstantDefinitionString();
    auto constantDefinitions = symbDescr.parseConstantDefinitions(constantDefinitionString);
    auto janiModel = janiModelProperties.first.defineUndefinedConstants(constantDefinitions).substituteConstants();
    if (!properties.empty()) {
        properties = storm::api::substituteConstantsInProperties(properties, constantDefinitions);
    }
    stopStopwatch(parsingTime);

    // Branch on the type of output
    auto const& output = storm::settings::getModule<storm::settings::modules::ConversionOutputSettings>();
    if (output.isJaniOutputSet()) {
        processJaniInputJaniOutput(janiModel, properties);
    } else {
        STORM_LOG_THROW(!output.isPrismOutputSet(), storm::exceptions::InvalidSettingsException, "Conversion from Jani to Prism is not supported.");
        STORM_LOG_THROW(false, storm::exceptions::InvalidSettingsException,
                        "There is either no outputformat specified or the provided combination of input and output format is not compatible.");
    }
}

void processOptions() {
    // Start by setting some urgent options (log levels, etc.)
    // We cannot use the general variants used for other executables since the "GeneralSettings" module is not available in Storm-conv
    setUrgentOptions();

    // Branch on the type of input
    auto const& input = storm::settings::getModule<storm::settings::modules::ConversionInputSettings>();
    STORM_LOG_THROW(!input.isPrismInputSet() || !input.isJaniInputSet(), storm::exceptions::InvalidSettingsException, "Multiple input options were set.");
    if (input.isPrismInputSet()) {
        processPrismInput();
    } else if (input.isJaniInputSet()) {
        processJaniInput();
    }
}
}  // namespace conv
}  // namespace storm

bool parseOptions(const int argc, const char* argv[]) {
    try {
        storm::settings::mutableManager().setFromCommandLine(argc, argv);
    } catch (storm::exceptions::OptionParserException&) {
        STORM_LOG_ERROR("Unable to parse command line options. Type '" + std::string(argv[0]) + " --help' or '" + std::string(argv[0]) +
                        " --help all' for help.");
        return false;
    }

    auto const& general = storm::settings::getModule<storm::settings::modules::ConversionGeneralSettings>();

    // Set options from config file (if given)
    if (general.isConfigSet()) {
        storm::settings::mutableManager().setFromConfigurationFile(general.getConfigFilename());
    }

    bool terminate = false;
    if (general.isHelpSet()) {
        storm::settings::manager().printHelp(general.getHelpFilterExpression());
        terminate = true;
    }

    if (general.isVersionSet()) {
        storm::cli::printVersion();
        terminate = true;
    }
    if (terminate) {
        exit(0);  // Terminate after help and version output with success.
        // TODO: Issue 674 discusses that this may not be ideal.
    }

    return true;
}

/*!
 * Main entry point of the executable storm-conv.
 */
int main(const int argc, const char** argv) {
    try {
        storm::utility::setUp();
        storm::cli::printHeader("Storm-conv", argc, argv);

        storm::settings::initializeConvSettings("Storm-conv", "storm-conv");
        if (!parseOptions(argc, argv)) {
            return -1;
        }

        storm::conv::processOptions();

        storm::utility::cleanUp();
        return 0;
    } catch (storm::exceptions::BaseException const& exception) {
        STORM_LOG_ERROR("An exception caused Storm-conv to terminate. The message of the exception is: " << exception.what());
        return 1;
    } catch (std::exception const& exception) {
        STORM_LOG_ERROR("An unexpected exception occurred and caused Storm-conv to terminate. The message of this exception is: " << exception.what());
        return 2;
    }
}
