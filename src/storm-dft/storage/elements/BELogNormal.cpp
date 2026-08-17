#include "BELogNormal.h"

#include <boost/math/distributions/lognormal.hpp>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"

namespace storm::dft {
namespace storage {
namespace elements {

template<>
double BELogNormal<double>::getUnreliability(double time) const {
    // Compute via boost as no closed form formula exists
    // TODO: initialize once if performance is an issue
    boost::math::lognormal_distribution<double> distribution(this->mean(), this->standardDeviation());
    return boost::math::cdf(distribution, time);
}

template<typename ValueType>
ValueType BELogNormal<ValueType>::getUnreliability(ValueType time) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing cumulative failure probability not supported for this data type.");
}

// Explicitly instantiate the class.
template class BELogNormal<double>;
template class BELogNormal<RationalFunction>;

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
