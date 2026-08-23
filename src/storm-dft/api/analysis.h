#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "storm-dft/builder/DftExplorationHeuristic.h"
#include "storm-dft/modelchecker/DFTModelChecker.h"
#include "storm-dft/storage/DFT.h"
#include "storm-dft/utility/RelevantEvents.h"
#include "storm/logic/Formula.h"

namespace storm::dft {
namespace api {

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
    bool printOutput = false);

/*!
 * Analyze the DFT using BDDs
 *
 * @param dft DFT
 * @param exportToDot If true exports the bdd representing the top level event of the dft in the dot format
 * @param filename The name of the file for exporting to dot
 * @param calculateMttf If true calculates the mean time to failure
 * @param mttfPrecision A constant that is used to determine if the mttf calculation converged
 * @param mttfStepsize A constant that is used in the mttf calculation
 * @param mttfAlgorithmName The name of the mttf algorithm to use
 * @param calculateMCS If true calculates the minimal cut sets
 * @param calculateProbability If true calculates the system failure probability
 * @param useModularisation If true tries modularisation
 * @param importanceMeasureName The name of the importance measure to calculate
 * @param timepoints The timebounds for probability calculations
 * @param properties The bounded until formulas to check (emulating the CTMC method)
 * @param additionalRelevantEventNames A vector of relevant events to be considered
 * @param chunksize The size of the chunks of doubles to work on at a time
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
 * @param printOutput If true, output is printed.
 */
template<typename ValueType>
void analyzeDFTSMT(storm::dft::storage::DFT<ValueType> const& dft, bool printOutput);

template<typename ValueType>
std::pair<uint64_t, uint64_t> computeBEFailureBounds(storm::dft::storage::DFT<ValueType> const& dft, bool useSMT, double solverTimeout);

template<typename ValueType>
bool computeDependencyConflicts(storm::dft::storage::DFT<ValueType>& dft, bool useSMT, double solverTimeout);

}  // namespace api
}  // namespace storm::dft
