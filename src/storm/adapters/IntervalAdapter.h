#pragma once

#include "storm/adapters/IntervalForward.h"

// isNan() and the explicit instantiations below need storm::RationalNumber to be a complete type.
#include "storm/adapters/RationalNumberAdapter.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#pragma clang diagnostic ignored "-Wunused-template"
#include <carl/interval/Interval.h>
#pragma clang diagnostic pop

namespace carl {
template<typename Number>
inline size_t hash_value(carl::Interval<Number> const& i) {
    std::hash<carl::Interval<Number>> h;
    return h(i);
}
}  // namespace carl

namespace storm {

/*!
 * Type describing the interval bounds.
 */
using BoundType = carl::BoundType;

}  // namespace storm

namespace carl {
// Rationals are never NaN; avoids instantiating carl's isNan(), which lacks a rational overload.
// Must precede the explicit instantiation below.
template<>
inline bool Interval<storm::RationalNumber>::isNan() const {
    return false;
}
}  // namespace carl

// Interval is fully defined in carl's header, so consumers instantiate it themselves; no visibility pin needed.
namespace carl {
extern template class Interval<double>;
extern template class Interval<storm::RationalNumber>;
}  // namespace carl
