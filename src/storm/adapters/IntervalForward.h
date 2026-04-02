#pragma once

#include "storm/adapters/RationalNumberForward.h"

namespace carl {
template<typename Number>
class Interval;
}

namespace storm {

/*!
 * Interval type
 */
typedef carl::Interval<double> Interval;
typedef carl::Interval<storm::RationalNumber> RationalInterval;

namespace detail {
template<typename ValueType>
struct IntervalMetaProgrammingHelper {
    using BaseType = ValueType;
    static const bool isInterval = false;
};
template<>
struct IntervalMetaProgrammingHelper<Interval> {
    using BaseType = double;
    static const bool isInterval = true;
};
template<>
struct IntervalMetaProgrammingHelper<RationalInterval> {
    using BaseType = storm::RationalNumber;
    static const bool isInterval = true;
};
}  // namespace detail

/*!
 * Helper to check if a type is an interval
 */
template<typename ValueType>
constexpr bool IsIntervalType = detail::IntervalMetaProgrammingHelper<ValueType>::isInterval;

/*!
 * Helper to access the type in which interval boundaries are stored.
 * Yields the type identity if the given type is not an interval
 */
template<typename ValueType>
using IntervalBaseType = typename detail::IntervalMetaProgrammingHelper<ValueType>::BaseType;
}  // namespace storm
