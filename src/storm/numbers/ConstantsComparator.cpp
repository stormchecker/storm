#include "ConstantsComparator.h"

#include <type_traits>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/numbers/NumberTraits.h"
#include "storm/numbers/constants.h"
#include "storm/storage/sparse/StateType.h"
#include "storm/utility/macros.h"

namespace storm {
namespace numbers {

template<typename ValueType>
ConstantsComparator<ValueType>::ConstantsComparator(ValueType const& precision, bool relative) : precision(precision), relative(relative) {
    // Intentionally left empty
}

template<typename ValueType>
bool ConstantsComparator<ValueType>::isOne(ValueType const& value) const {
    return isEqual(value, storm::numbers::one<ValueType>());
}

template<typename ValueType>
bool ConstantsComparator<ValueType>::isZero(ValueType const& value) const {
    return isEqual(value, storm::numbers::zero<ValueType>());
}

template<typename ValueType>
bool ConstantsComparator<ValueType>::isEqual(ValueType const& value1, ValueType const& value2) const {
    if (std::is_same<ValueType, storm::RationalFunction>() || std::is_same<ValueType, storm::Polynomial>()) {
        STORM_LOG_ASSERT(storm::numbers::isZero(precision), "Precision for rational functions must be zero.");
        return value1 == value2;
    } else {
        if (value1 == value2) {
            return true;
        } else if (storm::numbers::isZero(precision)) {
            return false;
        } else {
            return storm::numbers::isApproxEqual(value1, value2, precision, relative);
        }
    }
}

template<typename ValueType>
bool ConstantsComparator<ValueType>::isLess(ValueType const& value1, ValueType const& value2) const {
    STORM_LOG_ASSERT(!relative, "Relative precision and constants comparator is currently not supported.");
    return value1 < value2 - precision;
}

// Explicit instantiations.
template class ConstantsComparator<double>;
template class ConstantsComparator<int>;
template class ConstantsComparator<storm::storage::sparse::state_type>;

#if defined(STORM_HAVE_CLN)
template class ConstantsComparator<ClnRationalNumber>;
#endif

#if defined(STORM_HAVE_GMP)
template class ConstantsComparator<GmpRationalNumber>;
#endif

template class ConstantsComparator<storm::RationalFunction>;
template class ConstantsComparator<storm::Interval>;
template class ConstantsComparator<storm::RationalInterval>;
}  // namespace numbers
}  // namespace storm
