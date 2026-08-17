#include "BEErlang.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"

namespace storm::dft {
namespace storage {
namespace elements {

template<>
double BEErlang<double>::getUnreliability(double time) const {
    // 1 - \sum_{n=0}^{k-1} 1/n! * e^(-lambda * t) * (lambda * t)^n
    // where lambda is the rate and k the number of phases
    double res = 1;
    double factorFactorial = 1;
    double factorExp = std::exp(-this->activeFailureRate() * time);
    double factorPow = 1;
    for (unsigned i = 0; i <= this->phases() - 1; ++i) {
        if (i > 0) {
            factorFactorial /= i;
            factorPow *= this->activeFailureRate() * time;
        }
        res -= factorFactorial * factorExp * factorPow;
    }
    return res;
}

template<typename ValueType>
ValueType BEErlang<ValueType>::getUnreliability(ValueType time) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing cumulative failure probability not supported for this data type.");
}

// Explicitly instantiate the class.
template class BEErlang<double>;
template class BEErlang<RationalFunction>;

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
