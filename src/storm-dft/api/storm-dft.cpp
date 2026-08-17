#include "storm-dft/api/storm-dft.h"

#include <memory>
#include <vector>

#include "storm-conv/api/storm-conv.h"
#include "storm-conv/settings/modules/JaniExportSettings.h"
#include "storm-dft/adapters/SFTBDDPropertyFormulaAdapter.h"
#include "storm-dft/modelchecker/DftModularizationChecker.h"
#include "storm-dft/modelchecker/SFTBDDChecker.h"
#include "storm-dft/settings/modules/DftGspnSettings.h"
#include "storm-dft/settings/modules/FaultTreeSettings.h"
#include "storm-dft/storage/DFT.h"
#include "storm-dft/storage/DftJsonExporter.h"
#include "storm-dft/storage/SylvanBddManager.h"
#include "storm-dft/transformations/SftToBddTransformator.h"
#include "storm-dft/utility/MTTFHelper.h"

#include "storm/environment/Environment.h"

namespace storm::dft {
namespace api {

template<>
void analyzeDFTBdd(std::shared_ptr<storm::dft::storage::DFT<double>> const& dft, bool const exportToDot, std::string const& filename, bool const calculateMttf,
                   double const mttfPrecision, double const mttfStepsize, std::string const mttfAlgorithmName, bool const calculateMCS,
                   bool const calculateProbability, bool const useModularisation, std::string const importanceMeasureName,
                   std::vector<double> const& timepoints, std::vector<std::shared_ptr<storm::logic::Formula const>> const& properties,
                   std::vector<std::string> const& additionalRelevantEventNames, size_t const chunksize) {
#ifdef STORM_HAVE_SYLVAN
    if (calculateMttf) {
        if (mttfAlgorithmName == "proceeding") {
            std::cout << "The numerically approximated MTTF is " << storm::dft::utility::MTTFHelperProceeding(dft, mttfStepsize, mttfPrecision) << '\n';
        } else if (mttfAlgorithmName == "variableChange") {
            std::cout << "The numerically approximated MTTF is " << storm::dft::utility::MTTFHelperVariableChange(dft, mttfStepsize) << '\n';
        }
    }

    if (useModularisation && calculateProbability) {
        storm::dft::modelchecker::DftModularizationChecker<double> checker{dft};
        if (chunksize == 1) {
            for (auto const& timebound : timepoints) {
                auto const probability{checker.getProbabilityAtTimebound(timebound)};
                std::cout << "System failure probability at timebound " << timebound << " is " << probability << '\n';
            }
        } else {
            auto const probabilities{checker.getProbabilitiesAtTimepoints(timepoints, chunksize)};
            for (size_t i{0}; i < timepoints.size(); ++i) {
                auto const timebound{timepoints[i]};
                auto const probability{probabilities[i]};
                std::cout << "System failure probability at timebound " << timebound << " is " << probability << '\n';
            }
        }
        if (!properties.empty()) {
            auto const probabilities{checker.check(properties, chunksize)};
            for (size_t i{0}; i < probabilities.size(); ++i) {
                std::cout << "Property \"" << properties.at(i)->toString() << "\" has result " << probabilities.at(i) << '\n';
            }
        }
        return;
    } else {
        STORM_LOG_THROW(dft->nrDynamicElements() == 0, storm::exceptions::NotSupportedException,
                        "DFT is dynamic. "
                        "Bdds can only be used on static fault trees. "
                        "Try modularisation.");
    }

    auto sylvanBddManager{storm::dft::storage::SylvanBddManager::createWithDefaultEnvironment()};
    sylvanBddManager->execute([&]() {
        storm::dft::utility::RelevantEvents relevantEvents{additionalRelevantEventNames.begin(), additionalRelevantEventNames.end()};
        storm::dft::adapters::SFTBDDPropertyFormulaAdapter adapter{dft, properties, sylvanBddManager, relevantEvents};
        auto checker{adapter.getSFTBDDChecker()};

        if (exportToDot) {
            checker->exportBddToDot(filename);
        }

        if (calculateMCS) {
            auto const minimalCutSets{checker->getMinimalCutSetsAsIndices()};
            auto const sylvanBddManager{checker->getSylvanBddManager()};

            std::cout << "{\n";
            for (auto const& minimalCutSet : minimalCutSets) {
                std::cout << '{';
                for (auto const& be : minimalCutSet) {
                    std::cout << sylvanBddManager->getName(be) << ' ';
                }
                std::cout << "},\n";
            }
            std::cout << "}\n";
        }

        if (calculateProbability) {
            if (chunksize == 1) {
                for (auto const& timebound : timepoints) {
                    auto const probability{checker->getProbabilityAtTimebound(timebound)};
                    std::cout << "System failure probability at timebound " << timebound << " is " << probability << '\n';
                }
            } else {
                auto const probabilities{checker->getProbabilitiesAtTimepoints(timepoints, chunksize)};
                for (size_t i{0}; i < timepoints.size(); ++i) {
                    auto const timebound{timepoints[i]};
                    auto const probability{probabilities[i]};
                    std::cout << "System failure probability at timebound " << timebound << " is " << probability << '\n';
                }
            }

            if (!properties.empty()) {
                auto const probabilities{adapter.check(chunksize)};
                for (size_t i{0}; i < probabilities.size(); ++i) {
                    std::cout << "Property \"" << properties.at(i)->toString() << "\" has result " << probabilities.at(i) << '\n';
                }
            }
        }

        if (importanceMeasureName != "" && timepoints.size() == 1) {
            auto const bes{dft->getBasicElements()};
            std::vector<double> values{};
            if (importanceMeasureName == "MIF") {
                values = checker->getAllBirnbaumFactorsAtTimebound(timepoints[0]);
            }
            if (importanceMeasureName == "CIF") {
                values = checker->getAllCIFsAtTimebound(timepoints[0]);
            }
            if (importanceMeasureName == "DIF") {
                values = checker->getAllDIFsAtTimebound(timepoints[0]);
            }
            if (importanceMeasureName == "RAW") {
                values = checker->getAllRAWsAtTimebound(timepoints[0]);
            }
            if (importanceMeasureName == "RRW") {
                values = checker->getAllRRWsAtTimebound(timepoints[0]);
            }

            for (size_t i{0}; i < bes.size(); ++i) {
                std::cout << importanceMeasureName << " for the basic event " << bes[i]->name() << " at timebound " << timepoints[0] << " is " << values[i]
                          << '\n';
            }
        } else if (importanceMeasureName != "") {
            auto const bes{dft->getBasicElements()};
            std::vector<std::vector<double>> values{};
            if (importanceMeasureName == "MIF") {
                values = checker->getAllBirnbaumFactorsAtTimepoints(timepoints, chunksize);
            }
            if (importanceMeasureName == "CIF") {
                values = checker->getAllCIFsAtTimepoints(timepoints, chunksize);
            }
            if (importanceMeasureName == "DIF") {
                values = checker->getAllDIFsAtTimepoints(timepoints, chunksize);
            }
            if (importanceMeasureName == "RAW") {
                values = checker->getAllRAWsAtTimepoints(timepoints, chunksize);
            }
            if (importanceMeasureName == "RRW") {
                values = checker->getAllRRWsAtTimepoints(timepoints, chunksize);
            }
            for (size_t i{0}; i < bes.size(); ++i) {
                for (size_t j{0}; j < timepoints.size(); ++j) {
                    std::cout << importanceMeasureName << " for the basic event " << bes[i]->name() << " at timebound " << timepoints[j] << " is "
                              << values[i][j] << '\n';
                }
            }
        }
    });
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
#endif
}

template<>
void analyzeDFTBdd(std::shared_ptr<storm::dft::storage::DFT<storm::RationalFunction>> const& dft, bool const exportToDot, std::string const& filename,
                   bool const calculateMttf, double const mttfPrecision, double const mttfStepsize, std::string const mttfAlgorithmName,
                   bool const calculateMCS, bool const calculateProbability, bool const useModularisation, std::string const importanceMeasureName,
                   std::vector<double> const& timepoints, std::vector<std::shared_ptr<storm::logic::Formula const>> const& properties,
                   std::vector<std::string> const& additionalRelevantEventNames, size_t const chunksize) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "BDD analysis is not supported for this data type.");
}

template<typename ValueType>
void exportDFTToJsonFile(storm::dft::storage::DFT<ValueType> const& dft, std::string const& file) {
    storm::dft::storage::DftJsonExporter<ValueType>::toFile(dft, file);
}

template<typename ValueType>
std::string exportDFTToJsonString(storm::dft::storage::DFT<ValueType> const& dft) {
    std::stringstream stream;
    storm::dft::storage::DftJsonExporter<ValueType>::toStream(dft, stream);
    return stream.str();
}

template<>
void exportDFTToSMT(storm::dft::storage::DFT<double> const& dft, std::string const& file) {
    storm::dft::modelchecker::DFTASFChecker asfChecker(dft);
    asfChecker.convert();
    asfChecker.toFile(file);
}

template<>
void exportDFTToSMT(storm::dft::storage::DFT<storm::RationalFunction> const& dft, std::string const& file) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Export to SMT does not support this data type.");
}

template<>
void analyzeDFTSMT(storm::dft::storage::DFT<double> const& dft, bool printOutput) {
    uint64_t solverTimeout = 10;

    storm::dft::modelchecker::DFTASFChecker smtChecker(dft);
    smtChecker.toSolver();
    // Removed bound computation etc. here
    smtChecker.setSolverTimeout(solverTimeout);
    smtChecker.checkTleNeverFailed();
    smtChecker.unsetSolverTimeout();
}

template<>
void analyzeDFTSMT(storm::dft::storage::DFT<storm::RationalFunction> const& dft, bool printOutput) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Analysis by SMT not supported for this data type.");
}

template<>
std::pair<std::shared_ptr<storm::gspn::GSPN>, uint64_t> transformToGSPN(storm::dft::storage::DFT<double> const& dft) {
    storm::dft::settings::modules::FaultTreeSettings const& ftSettings = storm::settings::getModule<storm::dft::settings::modules::FaultTreeSettings>();
    storm::dft::settings::modules::DftGspnSettings const& dftGspnSettings = storm::settings::getModule<storm::dft::settings::modules::DftGspnSettings>();

    // Set Don't Care elements
    std::set<uint64_t> dontCareElements;
    if (!ftSettings.isDisableDC()) {
        // Insert all elements as Don't Care elements
        for (std::size_t i = 0; i < dft.nrElements(); i++) {
            dontCareElements.insert(dft.getElement(i)->id());
        }
    }

    // Transform to GSPN
    storm::dft::transformations::DftToGspnTransformator<double> gspnTransformator(dft);
    auto priorities = gspnTransformator.computePriorities(dftGspnSettings.isExtendPriorities());
    gspnTransformator.transform(priorities, dontCareElements, !dftGspnSettings.isDisableSmartTransformation(), dftGspnSettings.isMergeDCFailed(),
                                dftGspnSettings.isExtendPriorities());
    std::shared_ptr<storm::gspn::GSPN> gspn(gspnTransformator.obtainGSPN());
    return std::make_pair(gspn, gspnTransformator.toplevelFailedPlaceId());
}

template<>
std::pair<std::shared_ptr<storm::gspn::GSPN>, uint64_t> transformToGSPN(storm::dft::storage::DFT<storm::RationalFunction> const& dft) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Transformation to GSPN not supported for this data type.");
}

std::shared_ptr<storm::jani::Model> transformToJani(storm::gspn::GSPN const& gspn, uint64_t toplevelFailedPlace) {
    // Build Jani model
    storm::builder::JaniGSPNBuilder builder(gspn);
    std::shared_ptr<storm::jani::Model> model(builder.build("dft_gspn"));

    // Build properties
    std::shared_ptr<storm::expressions::ExpressionManager> const& exprManager = gspn.getExpressionManager();
    storm::jani::Variable const& topfailedVar = builder.getPlaceVariable(toplevelFailedPlace);
    storm::expressions::Expression targetExpression = exprManager->integer(1) == topfailedVar.getExpressionVariable().getExpression();
    // Add variable for easier access to 'failed' state
    builder.addTransientVariable(model.get(), "failed", targetExpression);
    auto failedFormula = std::make_shared<storm::logic::AtomicExpressionFormula>(targetExpression);
    auto properties = builder.getStandardProperties(model.get(), failedFormula, "Failed", "a failed state", true);

    // Export Jani to file
    storm::dft::settings::modules::DftGspnSettings const& dftGspnSettings = storm::settings::getModule<storm::dft::settings::modules::DftGspnSettings>();
    if (dftGspnSettings.isWriteToJaniSet()) {
        auto const& jani = storm::settings::getModule<storm::settings::modules::JaniExportSettings>();
        storm::api::exportJaniToFile(*model, properties, dftGspnSettings.getWriteToJaniFilename(), jani.isCompactJsonSet());
    }

    return model;
}

storm::dft::utility::RelevantEvents computeRelevantEvents(std::vector<std::shared_ptr<storm::logic::Formula const>> const& properties,
                                                          std::vector<std::string> const& additionalRelevantEventNames) {
    storm::dft::utility::RelevantEvents events(additionalRelevantEventNames.begin(), additionalRelevantEventNames.end());
    events.insertNamesFromProperties(properties.begin(), properties.end());
    return events;
}

// Explicitly instantiate methods
template void exportDFTToJsonFile(storm::dft::storage::DFT<double> const&, std::string const&);
template std::string exportDFTToJsonString(storm::dft::storage::DFT<double> const&);

template void exportDFTToJsonFile(storm::dft::storage::DFT<storm::RationalFunction> const&, std::string const&);
template std::string exportDFTToJsonString(storm::dft::storage::DFT<storm::RationalFunction> const&);

}  // namespace api
}  // namespace storm::dft
