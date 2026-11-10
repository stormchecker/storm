#pragma once

#include <boost/variant.hpp>
#include <functional>

#include "storm-dft/environment/DftEnvironment.h"
#include "storm-dft/storage/DFT.h"
#include "storm-dft/utility/RelevantEvents.h"
#include "storm/logic/Formula.h"
#include "storm/transformer/NonMarkovianChainTransformer.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/Stopwatch.h"

namespace storm::dft {
namespace modelchecker {

/*!
 * Analyser for DFTs.
 */
template<typename ValueType>
class DFTModelChecker {
   public:
    typedef storm::utility::ExtendedValueType<ValueType> ExtendedValueType;
    typedef std::pair<ExtendedValueType, ExtendedValueType> approximation_result;
    typedef std::vector<boost::variant<ExtendedValueType, approximation_result>> dft_results;
    typedef std::vector<std::shared_ptr<storm::logic::Formula const>> property_vector;

    /*!
     * Callback invoked whenever a model has been built and is available for export.
     *
     * @param model The built model.
     * @param isFinal True if it is the final model.
     *                False if it is an intermediate model during the approximation loop.
     */
    using ModelExportCallback = std::function<void(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, bool isFinal)>;

    class ResultOutputVisitor : public boost::static_visitor<> {
       public:
        void operator()(ExtendedValueType const& result, std::ostream& os) const {
            os << result;
        }

        void operator()(approximation_result const& result, std::ostream& os) const {
            os << "(" << result.first << ", " << result.second << ")";
        }
    };

    /*!
     * Constructor.
     */
    DFTModelChecker(bool printOutput, ModelExportCallback exportCallback = {}) : printInfo(printOutput), exportCallback(std::move(exportCallback)) {}

    /*!
     * Main method for checking DFTs.
     *
     * @param env Environment holding the DFT model-checking configuration.
     * @param origDft Original DFT.
     * @param properties Properties to check for.
     * @param relevantEvents Relevant events which should be observed.
     * @return Model checking results for the given properties..
     */
    dft_results check(storm::dft::DftEnvironment const& env, storm::dft::storage::DFT<ValueType> const& origDft, property_vector const& properties,
                      storm::dft::utility::RelevantEvents const& relevantEvents = {});

    /*!
     * Print timings of all operations to stream.
     *
     * @param os Output stream to write to.
     */
    void printTimings(std::ostream& os = std::cout) const;

    /*!
     * Print result to stream.
     *
     * @param results List of results.
     * @param os Output stream to write to.
     */
    void printResults(dft_results const& results, std::ostream& os = std::cout) const;

   private:
    bool printInfo;
    ModelExportCallback exportCallback;

    // Timing values
    storm::utility::Stopwatch buildingTimer;
    storm::utility::Stopwatch explorationTimer;
    storm::utility::Stopwatch bisimulationTimer;
    storm::utility::Stopwatch modelCheckingTimer;
    storm::utility::Stopwatch totalTimer;

    /*!
     * Internal helper for model checking a DFT.
     *
     * @param env Environment holding the DFT model-checking configuration.
     * @param dft DFT.
     * @param properties Properties to check for.
     * @param relevantEvents Relevant events which should be observed.
     * @return Model checking results (or in case of approximation two results for lower and upper bound)
     */
    dft_results checkHelper(storm::dft::DftEnvironment const& env, storm::dft::storage::DFT<ValueType> const& dft, property_vector const& properties,
                            storm::dft::utility::RelevantEvents const& relevantEvents);

    /*!
     * Internal helper for building a CTMC from a DFT via parallel composition.
     *
     * @param env Environment holding the DFT model-checking configuration.
     * @param dft DFT.
     * @param properties Properties to check for.
     * @param relevantEvents Relevant events which should be observed.
     * @return CTMC representing the DFT
     */
    std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> buildModelViaComposition(storm::dft::DftEnvironment const& env,
                                                                                     storm::dft::storage::DFT<ValueType> const& dft,
                                                                                     property_vector const& properties,
                                                                                     storm::dft::utility::RelevantEvents const& relevantEvents);

    /*!
     * Check model generated from DFT.
     *
     * @param env Environment holding the DFT model-checking configuration.
     * @param dft The DFT.
     * @param properties Properties to check for.
     * @param relevantEvents Relevant events which should be observed.
     *
     * @return Model checking result
     */
    dft_results checkDFT(storm::dft::DftEnvironment const& env, storm::dft::storage::DFT<ValueType> const& dft, property_vector const& properties,
                         storm::dft::utility::RelevantEvents const& relevantEvents);

    /*!
     * Check the given markov model for the given properties.
     *
     * @param env        Environment holding the DFT model-checking configuration.
     * @param model      Model to check
     * @param properties Properties to check for
     *
     * @return Model checking result
     */
    std::vector<ExtendedValueType> checkModel(storm::dft::DftEnvironment const& env, std::shared_ptr<storm::models::sparse::Model<ValueType>>& model,
                                              property_vector const& properties);

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
