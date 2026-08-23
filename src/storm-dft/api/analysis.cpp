#include "analysis.h"

#include <memory>
#include <vector>

#include "storm-dft/adapters/SFTBDDPropertyFormulaAdapter.h"
#include "storm-dft/modelchecker/DftModularizationChecker.h"
#include "storm-dft/modelchecker/SFTBDDChecker.h"
#include "storm-dft/settings/modules/FaultTreeSettings.h"
#include "storm-dft/storage/DFT.h"
#include "storm-dft/storage/SylvanBddManager.h"
#include "storm-dft/utility/FDEPConflictFinder.h"
#include "storm-dft/utility/FailureBoundFinder.h"
#include "storm-dft/utility/MTTFHelper.h"
#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm::dft {
namespace api {

storm::dft::utility::RelevantEvents computeRelevantEvents(std::vector<std::shared_ptr<storm::logic::Formula const>> const& properties,
                                                          std::vector<std::string> const& additionalRelevantEventNames) {
    storm::dft::utility::RelevantEvents events(additionalRelevantEventNames.begin(), additionalRelevantEventNames.end());
    events.insertNamesFromProperties(properties.begin(), properties.end());
    return events;
}

template<typename ValueType>
typename storm::dft::modelchecker::DFTModelChecker<ValueType>::dft_results analyzeDFT(
    storm::dft::storage::DFT<ValueType> const& dft, std::vector<std::shared_ptr<storm::logic::Formula const>> const& properties, bool symred,
    bool allowModularisation, storm::dft::utility::RelevantEvents const& relevantEvents, bool allowDCForRelevant, double approximationError,
    storm::dft::builder::ApproximationHeuristic approximationHeuristic, bool eliminateChains, storm::transformer::EliminationLabelBehavior labelBehavior,
    bool printOutput) {
    storm::dft::modelchecker::DFTModelChecker<ValueType> modelChecker(printOutput);
    typename storm::dft::modelchecker::DFTModelChecker<ValueType>::dft_results results =
        modelChecker.check(dft, properties, symred, allowModularisation, relevantEvents, allowDCForRelevant, approximationError, approximationHeuristic,
                           eliminateChains, labelBehavior);
    if (printOutput) {
        modelChecker.printTimings();
        modelChecker.printResults(results);
    }
    return results;
}

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

template<typename ValueType>
std::pair<uint64_t, uint64_t> computeBEFailureBounds(storm::dft::storage::DFT<ValueType> const& dft, bool useSMT, double solverTimeout) {
    uint64_t lowerBEBound = storm::dft::utility::FailureBoundFinder::getLeastFailureBound(dft, useSMT, solverTimeout);
    uint64_t upperBEBound = storm::dft::utility::FailureBoundFinder::getAlwaysFailedBound(dft, useSMT, solverTimeout);
    return std::make_pair(lowerBEBound, upperBEBound);
}

template<typename ValueType>
bool computeDependencyConflicts(storm::dft::storage::DFT<ValueType>& dft, bool useSMT, double solverTimeout) {
    std::vector<std::pair<uint64_t, uint64_t>> fdepConflicts =
        storm::dft::utility::FDEPConflictFinder<ValueType>::getDependencyConflicts(dft, useSMT, solverTimeout);

    for (auto const& pair : fdepConflicts) {
        STORM_LOG_DEBUG("Conflict between " << dft.getElement(pair.first)->name() << " and " << dft.getElement(pair.second)->name());
    }

    // Set the conflict map of the dft
    std::set<uint64_t> conflict_set;
    for (auto const& conflict : fdepConflicts) {
        conflict_set.insert(conflict.first);
        conflict_set.insert(conflict.second);
    }
    for (size_t depId : dft.getDependencies()) {
        if (!conflict_set.contains(depId)) {
            dft.setDependencyNotInConflict(depId);
        }
    }
    return !fdepConflicts.empty();
}

// Explicitly instantiate methods
template typename storm::dft::modelchecker::DFTModelChecker<double>::dft_results analyzeDFT(storm::dft::storage::DFT<double> const&,
                                                                                            std::vector<std::shared_ptr<storm::logic::Formula const>> const&,
                                                                                            bool, bool, storm::dft::utility::RelevantEvents const&, bool,
                                                                                            double, storm::dft::builder::ApproximationHeuristic, bool,
                                                                                            storm::transformer::EliminationLabelBehavior, bool);
template std::pair<uint64_t, uint64_t> computeBEFailureBounds(storm::dft::storage::DFT<double> const&, bool, double);
template bool computeDependencyConflicts(storm::dft::storage::DFT<double>&, bool, double);

template typename storm::dft::modelchecker::DFTModelChecker<storm::RationalFunction>::dft_results analyzeDFT(
    storm::dft::storage::DFT<storm::RationalFunction> const&, std::vector<std::shared_ptr<storm::logic::Formula const>> const&, bool, bool,
    storm::dft::utility::RelevantEvents const&, bool, double, storm::dft::builder::ApproximationHeuristic, bool, storm::transformer::EliminationLabelBehavior,
    bool);
template std::pair<uint64_t, uint64_t> computeBEFailureBounds(storm::dft::storage::DFT<storm::RationalFunction> const&, bool, double);
template bool computeDependencyConflicts(storm::dft::storage::DFT<storm::RationalFunction>&, bool, double);

}  // namespace api
}  // namespace storm::dft
