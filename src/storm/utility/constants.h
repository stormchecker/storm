#pragma once

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#include <cstdint>
#include <limits>
#include <string>

#include <map>
#include <vector>

#include "storm/utility/NumberTraits.h"

namespace storm {

// Forward-declare MatrixEntry class.
namespace storage {
template<typename IndexType, typename ValueType>
class MatrixEntry;
}

template<typename RationalType>
struct NumberTraits;

namespace utility {

namespace detail {
template<typename ValueType>
struct ElementLess {
    typedef std::less<ValueType> type;
};

struct DoubleLess {
    bool operator()(double a, double b) const {
        return (a == 0.0 && b > 0.0) || (b - a > 1e-17);
    }
};

template<>
struct ElementLess<double> {
    typedef DoubleLess type;
};

template<typename ValueType>
struct ElementGreater {
    typedef std::greater<ValueType> type;
};

struct DoubleGreater {
    bool operator()(double a, double b) const {
        return (b == 0.0 && a > 0.0) || (a - b > 1e-17);
    }
};

template<>
struct ElementGreater<double> {
    typedef DoubleGreater type;
};
}  // namespace detail

template<typename ValueType>
using ElementLess = typename detail::ElementLess<ValueType>::type;

template<typename ValueType>
using ElementGreater = typename detail::ElementGreater<ValueType>::type;

template<typename ValueType>
ValueType one();

template<typename ValueType>
ValueType zero();

template<typename ValueType>
ValueType infinity();

template<typename ValueType>
bool isApproxEqual(ValueType const& a, ValueType const& b, ValueType const& precision, bool relative);

template<typename ValueType>
bool isPositive(ValueType const& a);

template<typename ValueType>
bool isNonNegative(ValueType const& a);

/*!
 * Check whether a <= b <= c holds (if not strict, the default) or a < b < c (if strict).
 */
template<typename ValueType>
bool isBetween(ValueType const& a, ValueType const& b, ValueType const& c, bool strict = false);

template<typename ValueType>
bool isOne(ValueType const& a);

template<typename ValueType>
bool isZero(ValueType const& a);

template<typename ValueType>
bool isNan(ValueType const& a);

template<typename ValueType>
bool isAlmostZero(ValueType const& a);

template<typename ValueType>
bool isAlmostOne(ValueType const& a);

template<typename ValueType>
bool isConstant(ValueType const& a);

template<typename ValueType>
bool isInfinity(ValueType const& a);

template<typename ValueType>
bool isInteger(ValueType const& number);

template<typename TargetType, typename SourceType>
TargetType convertNumber(SourceType const& number);

template<typename ValueType>
std::pair<ValueType, ValueType> asFraction(ValueType const& number);

template<typename ValueType>
ValueType simplify(ValueType value);

template<typename IndexType, typename ValueType>
storm::storage::MatrixEntry<IndexType, ValueType>& simplify(storm::storage::MatrixEntry<IndexType, ValueType>& matrixEntry);

template<typename IndexType, typename ValueType>
storm::storage::MatrixEntry<IndexType, ValueType>&& simplify(storm::storage::MatrixEntry<IndexType, ValueType>&& matrixEntry);

template<typename ValueType>
std::pair<ValueType, ValueType> minmax(std::vector<ValueType> const& values);

template<typename ValueType>
ValueType minimum(std::vector<ValueType> const& values);

template<typename ValueType>
ValueType maximum(std::vector<ValueType> const& values);

template<typename K, typename ValueType>
std::pair<ValueType, ValueType> minmax(std::map<K, ValueType> const& values);

template<typename K, typename ValueType>
ValueType minimum(std::map<K, ValueType> const& values);

template<typename K, typename ValueType>
ValueType maximum(std::map<K, ValueType> const& values);

template<typename ValueType>
ValueType pow(ValueType const& value, int_fast64_t exponent);

template<typename ValueType>
ValueType max(ValueType const& first, ValueType const& second);

template<typename ValueType>
ValueType min(ValueType const& first, ValueType const& second);

template<typename ValueType>
ValueType sqrt(ValueType const& number);

template<typename ValueType>
ValueType abs(ValueType const& number);

template<typename ValueType>
ValueType floor(ValueType const& number);

template<typename ValueType>
ValueType ceil(ValueType const& number);

template<typename ValueType>
ValueType round(ValueType const& number);

template<typename ValueType>
ValueType log(ValueType const& number);

template<typename ValueType>
ValueType log10(ValueType const& number);

template<typename ValueType>
ValueType cos(ValueType const& number);

template<typename ValueType>
ValueType sin(ValueType const& number);

template<typename ValueType>
uint64_t numDigits(ValueType const& number);

/*!
 * Returns the minimum number of bits to represent the given number. Ignores sign, i.e. number and -number have the same size.
 */
template<typename ValueType>
uint64_t bitsize(ValueType const& number);

template<typename ValueType>
typename NumberTraits<ValueType>::IntegerType trunc(ValueType const& number);

template<typename RationalType>
typename NumberTraits<RationalType>::IntegerType numerator(RationalType const& number);

template<typename RationalType>
typename NumberTraits<RationalType>::IntegerType denominator(RationalType const& number);

/*!
 * (Integer-)Divides the dividend by the divisor and returns the result plus the remainder.
 */
template<typename IntegerType>
std::pair<IntegerType, IntegerType> divide(IntegerType const& dividend, IntegerType const& divisor);

template<typename IntegerType>
IntegerType mod(IntegerType const& first, IntegerType const& second);

template<typename ValueType>
std::string to_string(ValueType const& value);
}  // namespace utility
}  // namespace storm
