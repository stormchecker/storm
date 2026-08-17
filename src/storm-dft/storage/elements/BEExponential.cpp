#include "BEExponential.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"

namespace storm::dft {
namespace storage {
namespace elements {

template<>
double BEExponential<double>::getUnreliability(double time) const {
    // 1 - e^(-lambda * t)
    // where lambda is the rate
    return 1 - std::exp(-this->activeFailureRate() * time);
}

template<typename ValueType>
ValueType BEExponential<ValueType>::getUnreliability(ValueType time) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing cumulative failure probability not supported for this data type.");
}

// Explicitly instantiate the class.
template class BEExponential<double>;
template class BEExponential<RationalFunction>;

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
