#pragma once

#include <type_traits>
#include <utility>
#include <vector>

#include "storm-dft/modelchecker/DFTASFChecker.h"
#include "storm-dft/modelchecker/DFTModelChecker.h"
#include "storm-dft/parser/DFTGalileoParser.h"
#include "storm-dft/parser/DFTJsonParser.h"
#include "storm-dft/transformations/DftToGspnTransformator.h"
#include "storm-dft/transformations/DftTransformer.h"
#include "storm-dft/utility/DftValidator.h"
#include "storm-dft/utility/FDEPConflictFinder.h"
#include "storm-dft/utility/FailureBoundFinder.h"
#include "storm-dft/utility/RelevantEvents.h"
#include "storm-gspn/api/storm-gspn.h"

namespace storm::dft {
namespace api {

/*!
 * Load DFT from Galileo file.
 *
 * @param file File containing DFT description in Galileo format.
 * @return DFT.
 */
template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> loadDFTGalileoFile(std::string const& file) {
    return std::make_shared<storm::dft::storage::DFT<ValueType>>(storm::dft::parser::DFTGalileoParser<ValueType>::parseDFT(file));
}

/*!
 * Load DFT from JSON string.
 *
 * @param jsonString String containing DFT description in JSON format.
 * @return DFT.
 */
template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> loadDFTJsonString(std::string const& jsonString) {
    return std::make_shared<storm::dft::storage::DFT<ValueType>>(storm::dft::parser::DFTJsonParser<ValueType>::parseJsonFromString(jsonString));
}

/*!
 * Load DFT from JSON file.
 *
 * @param file File containing DFT description in JSON format.
 * @return DFT.
 */
template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> loadDFTJsonFile(std::string const& file) {
    return std::make_shared<storm::dft::storage::DFT<ValueType>>(storm::dft::parser::DFTJsonParser<ValueType>::parseJsonFromFile(file));
}

/*!
 * Check whether the DFT is well-formed.
 *
 * @param dft DFT.
 * @param validForMarkovianAnalysis If true, additional checks are performed to check whether the DFT is valid for analysis via Markov models.
 * @return Pair where the first entry is true iff the DFT is well-formed. The second entry contains the error messages for ill-formed parts.
 */
template<typename ValueType>
std::pair<bool, std::string> isWellFormed(storm::dft::storage::DFT<ValueType> const& dft, bool validForMarkovianAnalysis = true) {
    std::stringstream stream;
    bool wellFormed = false;
    if (validForMarkovianAnalysis) {
        wellFormed = storm::dft::utility::DftValidator<ValueType>::isDftValidForMarkovianAnalysis(dft, stream);
    } else {
        wellFormed = storm::dft::utility::DftValidator<ValueType>::isDftWellFormed(dft, stream);
    }
    return std::pair<bool, std::string>(wellFormed, stream.str());
}

/*!
 * Check whether the DFT has potential modeling issues.
 *
 * @param dft DFT.
 * @return Pair where the first entry is true iff the DFT has potential modeling issues. The second entry contains the warning messages for the issues.
 */
template<typename ValueType>
std::pair<bool, std::string> hasPotentialModelingIssues(storm::dft::storage::DFT<ValueType> const& dft) {
    std::stringstream stream;
    bool modelingIssues = storm::dft::utility::DftValidator<ValueType>::hasPotentialModelingIssues(dft, stream);
    return std::pair<bool, std::string>(modelingIssues, stream.str());
}

/*!
 * Apply transformations for DFT.
 *
 * @param dft DFT.
 * @param uniqueBE Flag whether a unique constant failed BE is created.
 * @param binaryFDEP Flag whether all dependencies should be binary (only one dependent child).
 * @param exponentialDistributions Flag whether distributions should be transformed to exponential distributions (if possible).
 * @return Transformed DFT.
 */
template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> applyTransformations(storm::dft::storage::DFT<ValueType> const& dft, bool uniqueBE, bool binaryFDEP,
                                                                          bool exponentialDistributions) {
    std::shared_ptr<storm::dft::storage::DFT<ValueType>> transformedDft = std::make_shared<storm::dft::storage::DFT<ValueType>>(dft);
    if (exponentialDistributions && !storm::dft::transformations::DftTransformer<ValueType>::hasOnlyExponentialDistributions(*transformedDft)) {
        transformedDft = storm::dft::transformations::DftTransformer<ValueType>::transformExponentialDistributions(*transformedDft);
    }
    if (uniqueBE && !storm::dft::transformations::DftTransformer<ValueType>::hasUniqueFailedBE(*transformedDft)) {
        transformedDft = storm::dft::transformations::DftTransformer<ValueType>::transformUniqueFailedBE(*transformedDft);
    }
    if (binaryFDEP && storm::dft::transformations::DftTransformer<ValueType>::hasNonBinaryDependency(*transformedDft)) {
        transformedDft = storm::dft::transformations::DftTransformer<ValueType>::transformBinaryDependencies(*transformedDft);
    }
    return transformedDft;
}

/*!
 * Apply transformations to make DFT feasible for Markovian analysis.
 *
 * @param dft DFT.
 * @return Transformed DFT.
 */
template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> prepareForMarkovAnalysis(storm::dft::storage::DFT<ValueType> const& dft) {
    return storm::dft::api::applyTransformations(dft, true, true, true);
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
    std::set<size_t> conflict_set;
    for (auto const& conflict : fdepConflicts) {
        conflict_set.insert(size_t(conflict.first));
        conflict_set.insert(size_t(conflict.second));
    }
    for (size_t depId : dft.getDependencies()) {
        if (!conflict_set.count(depId)) {
            dft.setDependencyNotInConflict(depId);
        }
    }
    return !fdepConflicts.empty();
}

/*!
 * Get relevant event ids from given relevant event names and labels in properties.
 *
 * @param properties List of properties. All events occurring in a property are relevant.
 * @param additionalRelevantEventNames List of names of additional relevant events.
 * @return Relevant events.
 */
storm::dft::utility::RelevantEvents computeRelevantEvents(std::vector<std::shared_ptr<storm::logic::Formula const>> const& properties,
                                                          std::vector<std::string> const& additionalRelevantEventNames);

/*!
 * Compute the exact or approximate analysis result of the given DFT according to the given properties.
 * First the Markov model is built from the DFT and then this model is checked against the given properties.
 *
 * @param dft DFT.
 * @param properties PCTL formulas capturing the properties to check.
 * @param symred Flag whether symmetry reduction should be used.
 * @param allowModularisation Flag whether modularisation should be applied if possible.
 * @param relevantEvents Relevant events which should be observed.
 * @param allowDCForRelevant Whether to allow Don't Care propagation for relevant events
 * @param approximationError Allowed approximation error.  Value 0 indicates no approximation.
 * @param approximationHeuristic Heuristic used for state space exploration.
 * @param eliminateChains If true, chains of non-Markovian states are eliminated from the resulting MA.
 * @param labelBehavior Behavior of labels of eliminated states
 * @param printOutput If true, model information, timings, results, etc. are printed.
 * @return Results.
 */
template<typename ValueType>
typename storm::dft::modelchecker::DFTModelChecker<ValueType>::dft_results analyzeDFT(
    storm::dft::storage::DFT<ValueType> const& dft, std::vector<std::shared_ptr<storm::logic::Formula const>> const& properties, bool symred = true,
    bool allowModularisation = true, storm::dft::utility::RelevantEvents const& relevantEvents = {}, bool allowDCForRelevant = false,
    double approximationError = 0.0, storm::dft::builder::ApproximationHeuristic approximationHeuristic = storm::dft::builder::ApproximationHeuristic::DEPTH,
    bool eliminateChains = false, storm::transformer::EliminationLabelBehavior labelBehavior = storm::transformer::EliminationLabelBehavior::KeepLabels,
    bool printOutput = false) {
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

/*!
 * Analyze the DFT using BDDs
 *
 * @param dft DFT
 *
 * @param exportToDot
 * If true exports the bdd representing the top level event of the dft
 * in the dot format
 *
 * @param filename
 * The name of the file for exporting to dot
 *
 * @param calculateMTTF
 * If true calculates the mean time to failure
 *
 * @parameter mttfPrecision
 * A constant that is used to determine if the mttf calculation converged
 *
 * @parameter mttfStepsize
 * A constant that is used in the mttf calculation
 *
 * @parameter mttfAlgorithmName
 * The name of the mttf algorithm to use
 *
 * @param calculateMCS
 * If true calculates the minimal cut sets
 *
 * @param calculateProbability
 * If true calculates the system failure propbability
 *
 * @param useModularisation
 * If true tries modularisation
 *
 * @param importanceMeasureName
 * The name of the importance measure to calculate
 *
 * @param timepoints
 * The timebounds for probability calculations
 *
 * @param properties
 * The bounded until formulas to check (emulating the CTMC method)
 *
 * @param additionalRelevantEventNames
 * A vector of relevant events to be considered
 *
 * @param chunksize
 * The size of the chunks of doubles to work on at a time
 *
 */
template<typename ValueType>
void analyzeDFTBdd(std::shared_ptr<storm::dft::storage::DFT<ValueType>> const& dft, bool const exportToDot, std::string const& filename,
                   bool const calculateMttf, double const mttfPrecision, double const mttfStepsize, std::string const mttfAlgorithmName,
                   bool const calculateMCS, bool const calculateProbability, bool const useModularisation, std::string const importanceMeasureName,
                   std::vector<double> const& timepoints, std::vector<std::shared_ptr<storm::logic::Formula const>> const& properties,
                   std::vector<std::string> const& additionalRelevantEventNames, size_t const chunksize);

/*!
 * Analyze the DFT using the SMT encoding
 *
 * @param dft DFT.
 *
 * @return Result result vector
 */
template<typename ValueType>
void analyzeDFTSMT(storm::dft::storage::DFT<ValueType> const& dft, bool printOutput);

/*!
 * Export DFT to JSON file.
 *
 * @param dft DFT.
 * @param file File.
 */
template<typename ValueType>
void exportDFTToJsonFile(storm::dft::storage::DFT<ValueType> const& dft, std::string const& file);

/*!
 * Export DFT to JSON string.
 *
 * @param dft DFT.
 * @return DFT in JSON format.
 */
template<typename ValueType>
std::string exportDFTToJsonString(storm::dft::storage::DFT<ValueType> const& dft);

/*!
 * Export DFT to SMT encoding.
 *
 * @param dft DFT.
 * @param file File.
 */
template<typename ValueType>
void exportDFTToSMT(storm::dft::storage::DFT<ValueType> const& dft, std::string const& file);

/*!
 * Transform DFT to GSPN.
 *
 * @param dft DFT.
 * @return Pair of GSPN and id of failed place corresponding to the top level element.
 */
template<typename ValueType>
std::pair<std::shared_ptr<storm::gspn::GSPN>, uint64_t> transformToGSPN(storm::dft::storage::DFT<ValueType> const& dft);

/*!
 * Transform GSPN to Jani model.
 *
 * @param gspn GSPN.
 * @param toplevelFailedPlace Id of the failed place in the GSPN for the top level element in the DFT.
 * @return JANI model.
 */
std::shared_ptr<storm::jani::Model> transformToJani(storm::gspn::GSPN const& gspn, uint64_t toplevelFailedPlace);

}  // namespace api
}  // namespace storm::dft
