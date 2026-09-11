#include "storm-cli-utilities/cli.h"
#include "storm-conv/api/storm-conv.h"
#include "storm-conv/settings/modules/JaniExportSettings.h"
#include "storm-dft-cli/settings/modules/DftGspnSettings.h"
#include "storm-dft-cli/settings/modules/DftIOSettings.h"
#include "storm-dft-cli/settings/modules/FaultTreeSettings.h"
#include "storm-dft/api/analysis.h"
#include "storm-dft/api/gspn_transformation.h"
#include "storm-dft/api/io.h"
#include "storm-dft/api/transformation.h"
#include "storm-dft/environment/AnalysisEnvironment.h"
#include "storm-dft/environment/DftEnvironment.h"
#include "storm-dft/environment/ModelBuilderEnvironment.h"
#include "storm-dft/environment/TransformationEnvironment.h"
#include "storm-dft/parser/BEOrderParser.h"
#include "storm-dft/settings/DftSettings.h"
#include "storm-gspn/api/storm-gspn.h"
#include "storm-parsers/api/properties.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/export.h"
#include "storm/api/properties.h"
#include "storm/exceptions/UnmetRequirementException.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/TransformationSettings.h"
#include "storm/utility/initialize.h"

/*!
 * Create the DFT environment from the commandline arguments.
 */
storm::dft::DftEnvironment createDftEnvironmentFromSettings() {
    storm::dft::DftEnvironment dftEnv;

    auto const& ftSettings = storm::settings::getModule<storm::dft::settings::modules::FaultTreeSettings>();
    auto const& transformationSettings = storm::settings::getModule<storm::settings::modules::TransformationSettings>();

    // Analysis environment
    auto& analysis = dftEnv.analysis();
    analysis.setUseModularisation(ftSettings.useModularisation());
#ifdef STORM_HAVE_Z3
    analysis.setSolveWithSMT(ftSettings.solveWithSMT());
#else
    analysis.setSolveWithSMT(false);
#endif
    analysis.setChunksize(ftSettings.getChunksize());
    analysis.setMttfPrecision(ftSettings.getMttfPrecision());
    analysis.setMttfStepsize(ftSettings.getMttfStepsize());
    analysis.setMttfAlgorithm(ftSettings.getMttfAlgorithm());
    if (ftSettings.isApproximationErrorSet()) {
        analysis.setApproximationError(ftSettings.getApproximationError());
    }
    analysis.setApproximationHeuristic(ftSettings.getApproximationHeuristic());

    // Model builder environment
    auto& modelBuilder = dftEnv.modelBuilder();
    modelBuilder.setUseSymmetryReduction(ftSettings.useSymmetryReduction());
    modelBuilder.setAllowDCForRelevantEvents(ftSettings.isAllowDCForRelevantEvents());
    modelBuilder.setAddLabelsClaiming(ftSettings.isAddLabelsClaiming());
    if (ftSettings.isMaxDepthSet()) {
        modelBuilder.setMaxDepth(ftSettings.getMaxDepth());
    }
    modelBuilder.setTakeFirstDependency(ftSettings.isTakeFirstDependency());
    modelBuilder.setUniqueFailedBE(ftSettings.isUniqueFailedBE());

    // Transformation environment
    auto& transformation = dftEnv.transformation();
    transformation.setUseBisimulation(storm::settings::getModule<storm::settings::modules::GeneralSettings>().isBisimulationSet());
    transformation.setEliminateChains(transformationSettings.isChainEliminationSet());
    transformation.setLabelBehavior(transformationSettings.getLabelBehavior());

    return dftEnv;
}

/*!
 * Process commandline options and start computations.
 */
template<typename ValueType>
void processOptions() {
    auto const& dftIOSettings = storm::settings::getModule<storm::dft::settings::modules::DftIOSettings>();
    auto const& faultTreeSettings = storm::settings::getModule<storm::dft::settings::modules::FaultTreeSettings>();
    auto const& ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    auto const& dftGspnSettings = storm::settings::getModule<storm::dft::settings::modules::DftGspnSettings>();
    storm::dft::DftEnvironment const dftEnv = createDftEnvironmentFromSettings();

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
        std::pair<std::shared_ptr<storm::gspn::GSPN>, uint64_t> pair =
            storm::dft::api::transformToGSPN(*dft, faultTreeSettings.isDisableDC(), dftGspnSettings.isExtendPriorities(),
                                             !dftGspnSettings.isDisableSmartTransformation(), dftGspnSettings.isMergeDCFailed());
        std::shared_ptr<storm::gspn::GSPN> gspn = pair.first;
        uint64_t toplevelFailedPlace = pair.second;

        // Export
        storm::api::handleGSPNExportSettings(*gspn);

        // Transform to Jani
        // TODO analyse Jani model
        auto [model, janiProperties] = storm::dft::api::transformToJani(*gspn, toplevelFailedPlace);
        if (dftGspnSettings.isWriteToJaniSet()) {
            auto const& janiExportSettings = storm::settings::getModule<storm::settings::modules::JaniExportSettings>();
            storm::api::exportJaniToFile(*model, janiProperties, dftGspnSettings.getWriteToJaniFilename(), janiExportSettings.isCompactJsonSet());
        }
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
    if (dftEnv.analysis().isSolveWithSMT()) {
        useSMT = true;
        STORM_LOG_DEBUG("Use SMT for preprocessing");
    }
#endif

    // Apply transformations
    // TODO transform later before actual analysis
    dft = storm::dft::api::applyTransformations(*dft, dftEnv.modelBuilder().isUniqueFailedBE(), true, false);
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
        double const mttfPrecision{dftEnv.analysis().getMttfPrecision()};
        double const mttfStepsize{dftEnv.analysis().getMttfStepsize()};
        std::string const mttfAlgorithm{dftEnv.analysis().getMttfAlgorithm()};
        bool const isExportToBddDot{dftIOSettings.isExportToBddDot()};
        bool const isTimebound{dftIOSettings.usePropTimebound()};
        bool const isTimepoints{dftIOSettings.usePropTimepoints()};

        bool const probabilityAnalysis{ioSettings.isPropertySet() || !isImportanceMeasureSet};
        size_t const chunksize{dftEnv.analysis().getChunksize()};
        bool const isModularisation{dftEnv.analysis().isUseModularisation()};

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
    storm::dft::utility::RelevantEvents relevantEvents =
        storm::dft::api::computeRelevantEvents(props, additionalRelevantEventNames, dftEnv.modelBuilder().isAddLabelsClaiming());

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
        auto exportCallback = [&ioSettings](std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, bool isFinalModel) {
            if (ioSettings.isExportExplicitSet()) {
                std::vector<std::string> parameterNames;
                storm::api::exportSparseModelAsDrn(model, ioSettings.getExportExplicitFilename(), parameterNames,
                                                   !ioSettings.isExplicitExportPlaceholdersDisabled());
            }
            if (isFinalModel && ioSettings.isExportDotSet()) {
                storm::api::exportSparseModelAsDot(model, ioSettings.getExportDotFilename(), ioSettings.getExportDotMaxWidth());
            }
        };
        storm::dft::api::analyzeDFT<ValueType>(dftEnv, *dft, props, relevantEvents, true, exportCallback);
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
 * Register the settings modules relevant for the Storm-DFT CLI.
 */
void initializeDftCliSettings(std::string const& name, std::string const& executableName) {
    storm::dft::settings::initializeDftSettings(name, executableName);
    storm::settings::addModule<storm::dft::settings::modules::DftIOSettings>();
    storm::settings::addModule<storm::dft::settings::modules::FaultTreeSettings>();
    storm::settings::addModule<storm::dft::settings::modules::DftGspnSettings>();
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
        return storm::cli::process("Storm-dft", "storm-dft", initializeDftCliSettings, process, argc, argv);
    } catch (storm::exceptions::BaseException const& exception) {
        STORM_LOG_ERROR("An exception caused Storm-DFT to terminate. The message of the exception is: " << exception.what());
        return 1;
    } catch (std::exception const& exception) {
        STORM_LOG_ERROR("An unexpected exception occurred and caused Storm-DFT to terminate. The message of this exception is: " << exception.what());
        return 2;
    }
}
