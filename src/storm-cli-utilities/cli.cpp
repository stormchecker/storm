#include "cli.h"

#include "storm-cli-utilities/model-handling.h"
#include "storm-cli-utilities/print.h"
#include "storm-cli-utilities/resources.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/OptionParserException.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/DebugSettings.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/ResourceSettings.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/initialize.h"

namespace storm {
namespace cli {

bool parseOptions(const int argc, const char* argv[]) {
    try {
        storm::settings::mutableManager().setFromCommandLine(argc, argv);
    } catch (storm::exceptions::OptionParserException&) {
        STORM_LOG_ERROR("Unable to parse command line options. Type '" + std::string(argv[0]) + " --help' or '" + std::string(argv[0]) +
                        " --help all' for help.");
        return false;
    }

    storm::settings::modules::GeneralSettings const& general = storm::settings::getModule<storm::settings::modules::GeneralSettings>();
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

void setResourceLimits() {
    storm::settings::modules::ResourceSettings const& resources = storm::settings::getModule<storm::settings::modules::ResourceSettings>();

    // If we were given a time limit, we put it in place now.
    if (resources.isTimeoutSet()) {
        storm::utility::resources::setTimeoutAlarm(resources.getTimeoutInSeconds());
    }

    // register signal handler to handle aborts
    storm::utility::resources::installSignalHandler(storm::settings::getModule<storm::settings::modules::ResourceSettings>().getSignalWaitingTimeInSeconds());
}

void setFileLogging() {
    storm::settings::modules::DebugSettings const& debug = storm::settings::getModule<storm::settings::modules::DebugSettings>();
    if (debug.isLogfileSet()) {
        storm::utility::initializeFileLogging(debug.getLogfilename());
    }
}

void setLogLevel() {
    storm::settings::modules::GeneralSettings const& general = storm::settings::getModule<storm::settings::modules::GeneralSettings>();
    storm::settings::modules::DebugSettings const& debug = storm::settings::getModule<storm::settings::modules::DebugSettings>();
    storm::settings::modules::CoreSettings const& core = storm::settings::getModule<storm::settings::modules::CoreSettings>();

    if (general.isVerboseSet()) {
        storm::utility::setLogLevel(l3pp::LogLevel::INFO);
    }
    if (debug.isDebugSet()) {
        storm::utility::setLogLevel(l3pp::LogLevel::DEBUG);
    }
    if (debug.isTraceSet()) {
        storm::utility::setLogLevel(l3pp::LogLevel::TRACE);
    }

    // Statistics and progress messages live on their own log channels so they can be enabled
    // independently of general verbosity.
    if (core.isShowStatisticsSet()) {
        storm::utility::setStatisticsLogLevel(l3pp::LogLevel::INFO);
    }
    if (general.isShowProgressSet()) {
        storm::utility::setProgressLogLevel(l3pp::LogLevel::INFO);
    }

    setFileLogging();
}

int process(std::string const& name, std::string const& executableName, std::function<void(std::string const&, std::string const&)> initSettingsFunc,
            std::function<void(void)> processOptionsFunc, const int argc, const char** argv) {
    storm::utility::setUp();
    storm::cli::printHeader(name, argc, argv);

    // Initialize settings
    initSettingsFunc(name, executableName);

    storm::utility::Stopwatch totalTimer(true);
    if (!parseOptions(argc, argv)) {
        return -1;
    }

    // Start by setting some urgent options (log levels, resources, etc.)
    setResourceLimits();
    setLogLevel();
    setFileLogging();
    // Set output precision
    storm::utility::setOutputDigitsFromGeneralPrecision(storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());

    // Process options and start computations
    processOptionsFunc();

    totalTimer.stop();
    if (storm::settings::getModule<storm::settings::modules::ResourceSettings>().isPrintTimeAndMemorySet()) {
        storm::cli::printTimeAndMemoryStatistics(totalTimer.getTimeInMilliseconds());
    }

    // All operations have been performed, so we clean up everything and terminate.
    storm::utility::cleanUp();
    return 0;
}

}  // namespace cli
}  // namespace storm
