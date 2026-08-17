/*! \mainpage Storm - A Modern Probabilistic Model Checker
 *
 * This document contains the Doxygen documentation of the Storm source code.
 *
 * \section more_info More information
 * For more information, installation guides and tutorials on how to use Storm, visit the Storm website: http://www.stormchecker.org.
 */

#include "storm-cli-utilities/cli.h"
#include "storm-cli/model-handling-main-cli.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/BaseException.h"
#include "storm/utility/macros.h"

void processOptions() {
    // Parse symbolic input (PRISM, JANI, properties, etc.)
    storm::cli::SymbolicInput symbolicInput = storm::cli::parseSymbolicInput();

    // Obtain settings for model processing
    storm::cli::ModelProcessingInformation mpi;

    // Preprocess the symbolic input
    std::tie(symbolicInput, mpi) = storm::cli::preprocessSymbolicInput(symbolicInput);

    STORM_LOG_WARN_COND(mpi.isCompatible,
                        "The model checking query does not seem to be supported for the selected engine. Storm will try to solve the query, but you will most "
                        "likely get an error for at least one of the provided properties.");

    // Export symbolic input (if requested)
    storm::cli::exportSymbolicInput(symbolicInput);

    storm::cli::processInput(symbolicInput, mpi);
}

void initSettings(std::string const& name, std::string const& executableName) {
    storm::settings::initializeAll(name, executableName);
    storm::settings::addModule<storm::settings::modules::CounterexampleGeneratorSettings>();
}

/*!
 * Main entry point of the executable storm.
 */
int main(const int argc, const char** argv) {
    try {
        return storm::cli::process("Storm", "storm", initSettings, processOptions, argc, argv);
    } catch (storm::exceptions::BaseException const& exception) {
        STORM_LOG_ERROR("An exception caused Storm to terminate. The message of the exception is: " << exception.what());
        return 1;
    } catch (std::exception const& exception) {
        STORM_LOG_ERROR("An unexpected exception occurred and caused Storm to terminate. The message of this exception is: " << exception.what());
        return 2;
    }
}
