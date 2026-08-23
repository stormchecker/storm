#pragma once

#include <memory>
#include <vector>

#include "storm-dft/modelchecker/DFTModelChecker.h"
#include "storm-dft/storage/DFT.h"
#include "storm-dft/storage/DftModule.h"
#include "storm-dft/storage/SylvanBddManager.h"

namespace storm::dft {
namespace modelchecker {

/*!
 * DFT analysis via modularization.
 * Dynamic modules are analyzed via model checking and replaced by a single BE capturing the probabilities of the module.
 * The resulting (static) fault tree is then analyzed via BDDs.
 *
 * @note All public functions must make sure that workDFT is set correctly and should assume workDFT to be in an erroneous state.
 */
template<typename ValueType>
class DftModularizationChecker {
   public:
    using DFTElementCPointer = std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const>;
    using FormulaVector = typename DFTModelChecker<ValueType>::property_vector;

    /*!
     * Initializes and computes all modules.
     * @param dft DFT.
     */
    DftModularizationChecker(std::shared_ptr<storm::dft::storage::DFT<ValueType>> dft);

    /*!
     * Calculate the properties specified by the formulas.
     * @param formulas List of formulas to check.
     * @param chunksize Chunk size used by the BDD checker.
     * @return Results corresponding to the given formulas.
     * @note Does not work with events in dynamic modules.
     */
    std::vector<ValueType> check(FormulaVector const &formulas, size_t chunksize = 0);

    /*!
     * Calculate the probability of failure for the given time points.
     * @param timepoints Time points.
     * @param chunksize Chunk size used by the BDD checker.
     * @return Probabilities that the top level event fails at the given time points.
     */
    std::vector<ValueType> getProbabilitiesAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize = 0);

    /*!
     * Calculate the probability of failure for the given time bound.
     * @param timebound Time bound.
     * @return The Probability that the top level event fails at the given time bound.
     */
    ValueType getProbabilityAtTimebound(ValueType const timebound) {
        // workDFT will be set in getProbabilitiesAtTimepoints()
        return getProbabilitiesAtTimepoints({timebound}).at(0);
    }

   private:
    /*!
     * Recursively populate the list of dynamic modules.
     * @param module Current module to consider.
     */
    void populateDynamicModules(storm::dft::storage::DftIndependentModule const &module);

    /*!
     * Calculate results for dynamic modules and replace them with BE's in workDFT.
     * @param timepoints Time points for which the failure probability should be computed.
     * @return Dft where dynamic modules are replaced.
     */
    std::shared_ptr<storm::dft::storage::DFT<ValueType>> replaceDynamicModules(std::vector<ValueType> const &timepoints);

    /*!
     * Analyse the given dynamic module.
     * @param module Module.
     * @param timepoints Time points for which the failure probability of element should be computed.
     */
    typename storm::dft::modelchecker::DFTModelChecker<ValueType>::dft_results analyseDynamicModule(storm::dft::storage::DftIndependentModule const &module,
                                                                                                    std::vector<ValueType> const &timepoints);

    // DFT.
    std::shared_ptr<storm::dft::storage::DFT<ValueType>> dft;
    // DFT modelchecker
    storm::dft::modelchecker::DFTModelChecker<ValueType> modelchecker;
    // don't reinitialize Sylvan BDD
    // temporary
    std::shared_ptr<storm::dft::storage::SylvanBddManager> sylvanBddManager;
    // Independent modules with their top element
    std::vector<storm::dft::storage::DftIndependentModule> dynamicModules;
};

}  // namespace modelchecker
}  // namespace storm::dft
