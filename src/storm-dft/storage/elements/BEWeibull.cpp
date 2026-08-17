#include "BEWeibull.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"

namespace storm::dft {
namespace storage {
namespace elements {

template<>
double BEWeibull<double>::getUnreliability(double time) const {
    // 1 - e^(-(t / lambda)^k)
    // where lambda is the rate and k the shape
    return 1 - exp(-std::pow(time / this->rate(), this->shape()));
}

template<typename ValueType>
ValueType BEWeibull<ValueType>::getUnreliability(ValueType time) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing cumulative failure probability not supported for this data type.");
}

// Explicitly instantiate the class.
template class BEWeibull<double>;
template class BEWeibull<RationalFunction>;

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
