#include "BEConst.h"

#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm::dft {
namespace storage {
namespace elements {

template<typename ValueType>
ValueType BEConst<ValueType>::getUnreliability(ValueType time) const {
    return failed() ? storm::utility::one<ValueType>() : storm::utility::zero<ValueType>();
}

// Explicitly instantiate the class.
template class BEConst<double>;
template class BEConst<RationalFunction>;

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft
