#pragma once

#include "storm-dft/storage/DFT.h"
#include "storm-pars/utility/parametric.h"

namespace storm::dft {
namespace transformations {

/*!
 * Instantiator to yield a concrete DFT from a parametric DFT (with parametric failure rates).
 */
template<typename ParametricType, typename ConstantType>
class DftInstantiator {
   public:
    /*!
     * Constructor.
     *
     * @param dft DFT.
     */
    DftInstantiator(storm::dft::storage::DFT<ParametricType> const& dft);

    /*!
     * Destructs the Instantiator.
     */
    virtual ~DftInstantiator() = default;

    /*!
     * Evaluates the occurring parametric functions and retrieves the instantiated DFT.
     * @param valuation Maps each occurring variables to the value with which it should be substituted.
     * @return The instantiated DFT.
     */
    std::shared_ptr<storm::dft::storage::DFT<ConstantType>> instantiate(storm::utility::parametric::Valuation<ParametricType> const& valuation);

    /*!
     *  Check validity
     */
    void checkValid() const;

   private:
    storm::dft::storage::DFT<ParametricType> const& dft;

    template<typename PT = ParametricType>
    typename std::enable_if<std::is_same<PT, ConstantType>::value, ConstantType>::type instantiate_helper(
        ParametricType const& function, storm::utility::parametric::Valuation<ParametricType> const& valuation) {
        return storm::utility::parametric::substitute(function, valuation);
    }

    template<typename PT = ParametricType>
    typename std::enable_if<!std::is_same<PT, ConstantType>::value, ConstantType>::type instantiate_helper(
        ParametricType const& function, storm::utility::parametric::Valuation<ParametricType> const& valuation) {
        return storm::utility::parametric::evaluate<ConstantType>(function, valuation);
    }

    /*!
     * Get list of children names.
     * @param element DFT element.
     * @return Names of element's children.
     */
    std::vector<std::string> getChildrenVector(std::shared_ptr<storm::dft::storage::elements::DFTElement<ParametricType> const> element);
};

}  // namespace transformations
}  // namespace storm::dft
