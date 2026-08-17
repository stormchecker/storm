#include "storm-dft/api/storm-dft.h"
#include "storm-cli-utilities/cli.h"
#include "storm-dft/parser/BEOrderParser.h"
#include "storm-dft/settings/DftSettings.h"
#include "storm-dft/settings/modules/DftGspnSettings.h"
#include "storm-dft/settings/modules/DftIOSettings.h"
#include "storm-dft/settings/modules/FaultTreeSettings.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/UnmetRequirementException.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/TransformationSettings.h"
#include "storm/utility/initialize.h"

/*!
 * Process commandline options and start computations.
 */
template<typename ValueType>
void processOptions() {
    auto const& dftIOSettings = storm::settings::getModule<storm::dft::settings::modules::DftIOSettings>();
    auto const& faultTreeSettings = storm::settings::getModule<storm::dft::settings::modules::FaultTreeSettings>();
    auto const& ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    auto const& dftGspnSettings = storm::settings::getModule<storm::dft::settings::modules::DftGspnSettings>();
    auto const& transformationSettings = storm::settings::getModule<storm::settings::modules::TransformationSettings>();

    // Build DFT from given file
    std::shared_ptr<storm::dft::storage::DFT<ValueType>> dft;
    if (dftIOSettings.isDftFileSet()) {
        STORM_LOG_DEBUG("Loading DFT from Galileo file " << dftIOSettings.getDftFilename());
        dft = storm::dft::api::loadDFTGalileoFile<ValueType>(dftIOSettings.getDftFilename());
    } else if (dftIOSettings.isDftJsonFileSet()) {
        STORM_LOG_DEBUG("Loading DFT from Json file " << dftIOSettings.getDftJsonFilename());
        dft = storm::dft::api::loadDFTJsonFile<ValueType>(dftIOSettings.getDftJsonFilename());
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidSettingsException, "No input model given.");
    }

    // Show statistics about DFT (number of gates, etc.)
    if (dftIOSettings.isShowDftStatisticsSet()) {
        dft->writeStatsToStream(std::cout);
        std::cout << '\n';
    }

    // Export to json
    if (dftIOSettings.isExportToJson()) {
        storm::dft::api::exportDFTToJsonFile<ValueType>(*dft, dftIOSettings.getExportJsonFilename());
    }

    // Check well-formedness of DFT
    auto wellFormedResult = storm::dft::api::isWellFormed(*dft, false);
    STORM_LOG_THROW(wellFormedResult.first, storm::exceptions::UnmetRequirementException, "DFT is not well-formed: " << wellFormedResult.second << ".");
    // Warn about potential modeling issues
    auto modelingIssues = storm::dft::api::hasPotentialModelingIssues(*dft);
    STORM_LOG_WARN_COND(!modelingIssues.first, modelingIssues.second);

    // Transformation to GSPN
    if (dftGspnSettings.isTransformToGspn()) {
        std::pair<std::shared_ptr<storm::gspn::GSPN>, uint64_t> pair = storm::dft::api::transformToGSPN(*dft);
        std::shared_ptr<storm::gspn::GSPN> gspn = pair.first;
        uint64_t toplevelFailedPlace = pair.second;

        // Export
        storm::api::handleGSPNExportSettings(*gspn);

        // Transform to Jani
        // TODO analyse Jani model
        std::shared_ptr<storm::jani::Model> model = storm::dft::api::transformToJani(*gspn, toplevelFailedPlace);
        return;
    }

    // Export to SMT
    if (dftIOSettings.isExportToSmt()) {
        storm::dft::api::exportDFTToSMT<ValueType>(*dft, dftIOSettings.getExportSmtFilename());
        return;
    }

    bool useSMT = false;
    uint64_t solverTimeout = 10;
#ifdef STORM_HAVE_Z3
    if (faultTreeSettings.solveWithSMT()) {
        useSMT = true;
        STORM_LOG_DEBUG("Use SMT for preprocessing");
    }
#endif

    // Apply transformations
    // TODO transform later before actual analysis
    dft = storm::dft::api::applyTransformations(*dft, faultTreeSettings.isUniqueFailedBE(), true, false);
    STORM_LOG_DEBUG(dft->getElementsString());

    // Compute minimal number of BE failures leading to system failure and
    // maximal number of BE failures not leading to system failure yet.
    // TODO: always needed?
    auto bounds = storm::dft::api::computeBEFailureBounds(*dft, useSMT, solverTimeout);
    STORM_LOG_DEBUG("BE failure bounds: lower bound: " << bounds.first << ", upper bound: " << bounds.second << ".");

#ifdef STORM_HAVE_Z3
    if (useSMT) {
        // Solve with SMT
        STORM_LOG_DEBUG("Running DFT analysis with use of SMT.");
        // Set dynamic behavior vector
        storm::dft::api::analyzeDFTSMT(*dft, true);
    }
#endif

    // BDD Analysis
    if (dftIOSettings.isExportToBddDot() || dftIOSettings.isAnalyzeWithBdds() || dftIOSettings.isMinimalCutSets() || dftIOSettings.isImportanceMeasureSet()) {
        bool const isImportanceMeasureSet{dftIOSettings.isImportanceMeasureSet()};
        bool const isMinimalCutSets{dftIOSettings.isMinimalCutSets()};
        bool const isMTTF{dftIOSettings.usePropExpectedTime()};
        double const mttfPrecision{faultTreeSettings.getMttfPrecision()};
        double const mttfStepsize{faultTreeSettings.getMttfStepsize()};
        std::string const mttfAlgorithm{faultTreeSettings.getMttfAlgorithm()};
        bool const isExportToBddDot{dftIOSettings.isExportToBddDot()};
        bool const isTimebound{dftIOSettings.usePropTimebound()};
        bool const isTimepoints{dftIOSettings.usePropTimepoints()};

        bool const probabilityAnalysis{ioSettings.isPropertySet() || !isImportanceMeasureSet};
        size_t const chunksize{faultTreeSettings.getChunksize()};
        bool const isModularisation{faultTreeSettings.useModularisation()};

        std::vector<double> timepoints{};
        if (isTimepoints) {
            timepoints = dftIOSettings.getPropTimepoints();
        }
        if (isTimebound) {
            timepoints.push_back(dftIOSettings.getPropTimebound());
        }

        std::string filename{""};
        if (isExportToBddDot) {
            filename = dftIOSettings.getExportBddDotFilename();
        }

        // gather manually inputted properties
        std::vector<std::shared_ptr<storm::logic::Formula const>> manuallyInputtedProperties;
        if (ioSettings.isPropertySet()) {
            manuallyInputtedProperties = storm::api::extractFormulasFromProperties(storm::api::parseProperties(ioSettings.getProperty()));
        }

        std::string importanceMeasureName{""};
        if (isImportanceMeasureSet) {
            importanceMeasureName = dftIOSettings.getImportanceMeasure();
        }

        // Set variable ordering
        if (dftIOSettings.isVariableOrderingFileSet()) {
            auto beOrder = storm::dft::parser::BEOrderParser<ValueType>::parseBEOrder(dftIOSettings.getVariableOrderingFilename(), *dft);
            dft->setBEOrder(beOrder);
        }

        auto const additionalRelevantEventNames{faultTreeSettings.getRelevantEvents()};
        storm::dft::api::analyzeDFTBdd<ValueType>(dft, isExportToBddDot, filename, isMTTF, mttfPrecision, mttfStepsize, mttfAlgorithm, isMinimalCutSets,
                                                  probabilityAnalysis, isModularisation, importanceMeasureName, timepoints, manuallyInputtedProperties,
                                                  additionalRelevantEventNames, chunksize);

        // don't perform other analysis if analyzeWithBdds is set
        if (dftIOSettings.isAnalyzeWithBdds()) {
            return;
        }
    }

    // From now on we analyse the DFT via model checking

    // Set min or max
    std::string optimizationDirection = "min";
    if (dftIOSettings.isComputeMaximalValue()) {
        optimizationDirection = "max";
    }

    // Construct properties to analyse.
    // We allow multiple properties to be checked at once.
    std::vector<std::string> properties;
    if (ioSettings.isPropertySet()) {
        properties.push_back(ioSettings.getProperty());
    }
    if (dftIOSettings.usePropExpectedTime()) {
        properties.push_back("T" + optimizationDirection + "=? [F \"failed\"]");
    }
    if (dftIOSettings.usePropProbability()) {
        properties.push_back("P" + optimizationDirection + "=? [F \"failed\"]");
    }
    if (dftIOSettings.usePropTimebound()) {
        std::stringstream stream;
        stream << "P" << optimizationDirection << "=? [F<=" << dftIOSettings.getPropTimebound() << " \"failed\"]";
        properties.push_back(stream.str());
    }
    if (dftIOSettings.usePropTimepoints()) {
        for (double timepoint : dftIOSettings.getPropTimepoints()) {
            std::stringstream stream;
            stream << "P" << optimizationDirection << "=? [F<=" << timepoint << " \"failed\"]";
            properties.push_back(stream.str());
        }
    }

    // Build properties
    std::vector<std::shared_ptr<storm::logic::Formula const>> props;
    if (!properties.empty()) {
        std::string propString;
        for (size_t i = 0; i < properties.size(); ++i) {
            propString += properties[i];
            if (i + 1 < properties.size()) {
                propString += ";";
            }
        }
        props = storm::api::extractFormulasFromProperties(storm::api::parseProperties(propString));
    }

    // Set relevant event names
    std::vector<std::string> additionalRelevantEventNames;
    if (faultTreeSettings.areRelevantEventsSet()) {
        // Possible clash of relevantEvents and disableDC was already considered in FaultTreeSettings::check().
        additionalRelevantEventNames = faultTreeSettings.getRelevantEvents();
    } else if (faultTreeSettings.isDisableDC()) {
        // All events are relevant
        additionalRelevantEventNames = {"all"};
    }
    storm::dft::utility::RelevantEvents relevantEvents = storm::dft::api::computeRelevantEvents(props, additionalRelevantEventNames);

    // Analyze DFT by translation to CTMC/MA
    dft = storm::dft::api::prepareForMarkovAnalysis<ValueType>(*dft);
    STORM_LOG_DEBUG("DFT after preparation for Markov analysis:\n" << dft->getElementsString());

    // Check which FDEPs actually introduce conflicts which need non-deterministic resolution
    bool hasConflicts = storm::dft::api::computeDependencyConflicts(*dft, useSMT, solverTimeout);
    if (hasConflicts) {
        STORM_LOG_DEBUG("FDEP conflicts found.");
    } else {
        STORM_LOG_DEBUG("No FDEP conflicts found.");
    }

    // TODO allow building of state space even without properties
    if (props.empty()) {
        STORM_LOG_WARN("No property given. No analysis will be performed.");
    } else {
        double approximationError = 0.0;
        if (faultTreeSettings.isApproximationErrorSet()) {
            approximationError = faultTreeSettings.getApproximationError();
        }
        storm::dft::api::analyzeDFT<ValueType>(*dft, props, faultTreeSettings.useSymmetryReduction(), faultTreeSettings.useModularisation(), relevantEvents,
                                               faultTreeSettings.isAllowDCForRelevantEvents(), approximationError,
                                               faultTreeSettings.getApproximationHeuristic(), transformationSettings.isChainEliminationSet(),
                                               transformationSettings.getLabelBehavior(), true);
    }
}

void process() {
    storm::settings::modules::GeneralSettings const& generalSettings = storm::settings::getModule<storm::settings::modules::GeneralSettings>();
    if (generalSettings.isParametricSet()) {
        processOptions<storm::RationalFunction>();
    } else if (generalSettings.isExactSet()) {
        STORM_LOG_WARN("Exact solving over rational numbers is not implemented. Performing exact solving using rational functions instead.");
        processOptions<storm::RationalFunction>();
    } else {
        processOptions<double>();
    }
}

/*!
 * Entry point for Storm-DFT.
 *
 * @param argc The argc argument of main().
 * @param argv The argv argument of main().
 * @return Return code, 0 if successful, > 0 otherwise.
 */
int main(const int argc, const char** argv) {
    try {
        return storm::cli::process("Storm-dft", "storm-dft", storm::dft::settings::initializeDftSettings, process, argc, argv);
    } catch (storm::exceptions::BaseException const& exception) {
        STORM_LOG_ERROR("An exception caused Storm-DFT to terminate. The message of the exception is: " << exception.what());
        return 1;
    } catch (std::exception const& exception) {
        STORM_LOG_ERROR("An unexpected exception occurred and caused Storm-DFT to terminate. The message of this exception is: " << exception.what());
        return 2;
    }
}
