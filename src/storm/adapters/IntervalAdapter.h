#pragma once

#include "storm/adapters/IntervalForward.h"

// isNan() below needs storm::RationalNumber to be a complete type.
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
template<>
inline bool Interval<storm::RationalNumber>::isNan() const {
    return false;
}
}  // namespace carl
