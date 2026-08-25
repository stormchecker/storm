#pragma once

#include "storm-dft/storage/DFT.h"
#include "storm-dft/utility/RelevantEvents.h"
#include "storm/api/storm.h"
#include "storm/logic/Formula.h"
#include "storm/utility/Stopwatch.h"

namespace storm::dft {
namespace modelchecker {

/*!
 * Analyser for DFTs.
 */
template<typename ValueType>
class DFTModelChecker {
   public:
    typedef std::pair<ValueType, ValueType> approximation_result;
    typedef std::vector<boost::variant<ValueType, approximation_result>> dft_results;
    typedef std::vector<std::shared_ptr<storm::logic::Formula const>> property_vector;

    class ResultOutputVisitor : public boost::static_visitor<> {
       public:
        void operator()(ValueType result, std::ostream& os) const {
            os << result;
        }

        void operator()(std::pair<ValueType, ValueType> const& result, std::ostream& os) const {
            os << "(" << result.first << ", " << result.second << ")";
        }
    };

    /*!
     * Constructor.
     */
    DFTModelChecker(bool printOutput) : printInfo(printOutput) {}

    /*!
     * Main method for checking DFTs.
     *
     * @param origDft Original DFT.
     * @param properties Properties to check for.
     * @param symred Flag whether symmetry reduction should be used.
     * @param allowModularisation Flag indicating if modularisation is allowed.
     * @param relevantEvents Relevant events which should be observed.
     * @param allowDCForRelevant Whether to allow Don't Care propagation for relevant events
     * @param approximationError Error allowed for approximation. Value 0 indicates no approximation.
     * @param approximationHeuristic Heuristic used for state space exploration.
     * @param eliminateChains If true, chains of non-Markovian states are eliminated from the resulting MA
     * @param labelBehavior Behavior of labels of eliminated states
     * @return Model checking results for the given properties.
     */
    dft_results check(storm::dft::storage::DFT<ValueType> const& origDft, property_vector const& properties, bool symred = true,
                      bool allowModularisation = true, storm::dft::utility::RelevantEvents const& relevantEvents = {}, bool allowDCForRelevant = false,
                      double approximationError = 0.0,
                      storm::dft::builder::ApproximationHeuristic approximationHeuristic = storm::dft::builder::ApproximationHeuristic::DEPTH,
                      bool eliminateChains = false,
                      storm::transformer::EliminationLabelBehavior labelBehavior = storm::transformer::EliminationLabelBehavior::KeepLabels);

    /*!
     * Print timings of all operations to stream.
     *
     * @param os Output stream to write to.
     */
    void printTimings(std::ostream& os = std::cout);

    /*!
     * Print result to stream.
     *
     * @param results List of results.
     * @param os Output stream to write to.
     */
    void printResults(dft_results const& results, std::ostream& os = std::cout);

   private:
    bool printInfo;

    // Timing values
    storm::utility::Stopwatch buildingTimer;
    storm::utility::Stopwatch explorationTimer;
    storm::utility::Stopwatch bisimulationTimer;
    storm::utility::Stopwatch modelCheckingTimer;
    storm::utility::Stopwatch totalTimer;

    /*!
     * Internal helper for model checking a DFT.
     *
     * @param dft DFT.
     * @param properties Properties to check for.
     * @param symred Flag indicating if symmetry reduction should be used.
     * @param allowModularisation Flag indicating if modularisation is allowed.
     * @param relevantEvents Relevant events which should be observed.
     * @param allowDCForRelevant Whether to allow Don't Care propagation for relevant events
     * @param approximationError Error allowed for approximation. Value 0 indicates no approximation.
     * @param approximationHeuristic Heuristic used for approximation.
     * @param eliminateChains If true, chains of non-Markovian states are eliminated from the resulting MA
     * @param labelBehavior Behavior of labels of eliminated states
     * @return Model checking results (or in case of approximation two results for lower and upper bound)
     */
    dft_results checkHelper(storm::dft::storage::DFT<ValueType> const& dft, property_vector const& properties, bool symred, bool allowModularisation,
                            storm::dft::utility::RelevantEvents const& relevantEvents, bool allowDCForRelevant = false, double approximationError = 0.0,
                            storm::dft::builder::ApproximationHeuristic approximationHeuristic = storm::dft::builder::ApproximationHeuristic::DEPTH,
                            bool eliminateChains = false,
                            storm::transformer::EliminationLabelBehavior labelBehavior = storm::transformer::EliminationLabelBehavior::KeepLabels);

    /*!
     * Internal helper for building a CTMC from a DFT via parallel composition.
     *
     * @param dft DFT.
     * @param properties Properties to check for.
     * @param symred Flag indicating if symmetry reduction should be used.
     * @param allowModularisation Flag indicating if modularisation is allowed.
     * @param relevantEvents Relevant events which should be observed.
     * @param allowDCForRelevant Whether to allow Don't Care propagation for relevant events
     * @return CTMC representing the DFT
     */
    std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> buildModelViaComposition(storm::dft::storage::DFT<ValueType> const& dft,
                                                                                     property_vector const& properties, bool symred, bool allowModularisation,
                                                                                     storm::dft::utility::RelevantEvents const& relevantEvents,
                                                                                     bool allowDCForRelevant);

    /*!
     * Check model generated from DFT.
     *
     * @param dft The DFT.
     * @param properties Properties to check for.
     * @param symred Flag indicating if symmetry reduction should be used.
     * @param relevantEvents Relevant events which should be observed.
     * @param allowDCForRelevant Whether to allow Don't Care propagation for relevant events
     * @param approximationError Error allowed for approximation. Value 0 indicates no approximation.
     * @param approximationHeuristic Heuristic used for approximation.
     * @param eliminateChains If true, chains of non-Markovian states are eliminated from the resulting MA
     * @param labelBehavior Behavior of labels of eliminated states
     *
     * @return Model checking result
     */
    dft_results checkDFT(storm::dft::storage::DFT<ValueType> const& dft, property_vector const& properties, bool symred,
                         storm::dft::utility::RelevantEvents const& relevantEvents, bool allowDCForRelevant, double approximationError = 0.0,
                         storm::dft::builder::ApproximationHeuristic approximationHeuristic = storm::dft::builder::ApproximationHeuristic::DEPTH,
                         bool eliminateChains = false,
                         storm::transformer::EliminationLabelBehavior labelBehavior = storm::transformer::EliminationLabelBehavior::KeepLabels);

    /*!
     * Check the given markov model for the given properties.
     *
     * @param model      Model to check
     * @param properties Properties to check for
     *
     * @return Model checking result
     */
    std::vector<ValueType> checkModel(std::shared_ptr<storm::models::sparse::Model<ValueType>>& model, property_vector const& properties);

    /*!
     * Checks if the computed approximation is sufficient, i.e.
     * upperBound - lowerBound <= approximationError * mean(lowerBound, upperBound).
     *
     * @param lowerBound         The lower bound on the result.
     * @param upperBound         The upper bound on the result.
     * @param approximationError The allowed error for approximating.
     * @param relative           Flag indicating if the error should be relative to 1 or
                                 to the mean of lower and upper bound.
     *
     * @return True, if the approximation is sufficient.
     */
    bool isApproximationSufficient(ValueType lowerBound, ValueType upperBound, double approximationError, bool relative);
};

}  // namespace modelchecker
}  // namespace storm::dft
