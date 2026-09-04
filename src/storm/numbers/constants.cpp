#include "constants.h"

#include <cmath>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/numbers/NumberTraits.h"
#include "storm/storage/sparse/StateType.h"
#include "storm/utility/logging.h"
#include "storm/utility/macros.h"

namespace storm {
namespace numbers {

template<typename ValueType>
ValueType one() {
    return ValueType(1);
}

template<typename ValueType>
ValueType zero() {
    return ValueType(0);
}

template<typename ValueType>
ValueType infinity() {
    // std::numeric_limits<T>::infinity() is zero for types that have no infinity, so asking for the infinity of an
    // integral type used to silently yield zero. Reject it instead.
    static_assert(!std::numeric_limits<ValueType>::is_integer, "There is no infinity for integral types.");
    return std::numeric_limits<ValueType>::infinity();
}

template<typename ValueType>
bool isOne(ValueType const& a) {
    return a == one<ValueType>();
}

template<typename ValueType>
bool isZero(ValueType const& a) {
    return a == zero<ValueType>();
}

template<typename ValueType>
bool isNan(ValueType const&) {
    return false;
}

template<>
bool isNan(double const& value) {
    return std::isnan(value);
}

template<typename ValueType>
bool isApproxEqual(ValueType const& a, ValueType const& b, ValueType const& precision, bool relative) {
    ValueType const absDiff = abs<ValueType>(a - b);
    if (relative) {
        return absDiff <= precision * max(abs(a), abs(b));
    } else {
        return absDiff <= precision;
    }
}

template<typename ValueType>
bool isPositive(ValueType const& a) {
    return a > zero<ValueType>();
}

template<typename ValueType>
bool isNonNegative(ValueType const& a) {
    return a >= zero<ValueType>();
}

/**
 * Compare whether a <= b <= c or a < b < c, based on the strictness parameter.
 * @param a A constant value
 * @param b Some value
 * @param c A constant value
 * @param strict Changes the type of the comparison
 * @return true if b is between a and c.
 */
template<typename ValueType>
bool isBetween(ValueType const& a, ValueType const& b, ValueType const& c, bool strict) {
    STORM_LOG_ASSERT(isConstant(a), "Checking whether something is between two values is only sensible on constants.");
    STORM_LOG_ASSERT(isConstant(c), "Checking whether something is between two values is only sensible on constants.");
    if (strict) {
        return a < b && b < c;
    } else {
        return a <= b && b <= c;
    }
}

template<typename ValueType>
bool isAlmostZero(ValueType const& a) {
    return a < convert<ValueType>(1e-12) && a > -convert<ValueType>(1e-12);
}

template<typename ValueType>
bool isAlmostOne(ValueType const& a) {
    return a < convert<ValueType>(1.0 + 1e-12) && a > convert<ValueType>(1.0 - 1e-12);
}

template<typename ValueType>
bool isConstant(ValueType const&) {
    return true;
}

template<typename ValueType>
bool isInfinity(ValueType const& a) {
    if constexpr (std::numeric_limits<ValueType>::is_integer) {
        // Integral types have no infinity, so no value of them is infinite.
        return false;
    } else {
        return a == infinity<ValueType>();
    }
}

template<typename ValueType>
bool isInteger(ValueType const& number) {
    ValueType iPart;
    ValueType result = std::modf(number, &iPart);
    return result == zero<ValueType>();
}

template<>
bool isInteger(int const&) {
    return true;
}

template<>
bool isInteger(uint32_t const&) {
    return true;
}

template<>
bool isInteger(storm::storage::sparse::state_type const&) {
    return true;
}

template<typename TargetType, typename SourceType>
TargetType convert(SourceType const& number) {
    return static_cast<TargetType>(number);
}

template<>
uint_fast64_t convert(double const& number) {
    return std::llround(number);
}

template<>
int_fast64_t convert(double const& number) {
    return std::llround(number);
}

template<>
double convert(uint_fast64_t const& number) {
    return number;
}

template<>
double convert(double const& number) {
    return number;
}

template<>
double convert(long long const& number) {
    return static_cast<double>(number);
}

template<>
storm::storage::sparse::state_type convert(long long const& number) {
    return static_cast<storm::storage::sparse::state_type>(number);
}

template<typename ValueType>
ValueType simplify(ValueType value) {
    // In the general case, we don't do anything here, but merely return the value. If something else is
    // supposed to happen here, the templated function can be specialized for this particular type.
    return value;
}

template<typename ValueType>
std::pair<ValueType, ValueType> minmax(std::vector<ValueType> const& values) {
    STORM_LOG_ASSERT(!values.empty(), "Expected non-empty values.");
    ValueType min = values.front();
    ValueType max = values.front();
    for (auto const& vt : values) {
        if (vt < min) {
            min = vt;
        }
        if (vt > max) {
            max = vt;
        }
    }
    return std::make_pair(min, max);
}

template<typename ValueType>
ValueType minimum(std::vector<ValueType> const& values) {
    STORM_LOG_ASSERT(!values.empty(), "Expected non-empty values.");
    ValueType min = values.front();
    for (auto const& vt : values) {
        if (vt < min) {
            min = vt;
        }
    }
    return min;
}

template<typename ValueType>
ValueType maximum(std::vector<ValueType> const& values) {
    STORM_LOG_ASSERT(!values.empty(), "Expected non-empty values.");
    ValueType max = values.front();
    for (auto const& vt : values) {
        if (vt > max) {
            max = vt;
        }
    }
    return max;
}

template<typename K, typename ValueType>
std::pair<ValueType, ValueType> minmax(std::map<K, ValueType> const& values) {
    STORM_LOG_ASSERT(!values.empty(), "Expected non-empty values.");
    ValueType min = values.begin()->second;
    ValueType max = values.begin()->second;
    for (auto const& vt : values) {
        if (vt.second < min) {
            min = vt.second;
        }
        if (vt.second > max) {
            max = vt.second;
        }
    }
    return std::make_pair(min, max);
}

template<typename K, typename ValueType>
ValueType minimum(std::map<K, ValueType> const& values) {
    return minmax(values).first;
}

template<typename K, typename ValueType>
ValueType maximum(std::map<K, ValueType> const& values) {
    return minmax(values).second;
}

template<typename ValueType>
ValueType pow(ValueType const& value, int_fast64_t exponent) {
    return std::pow(value, exponent);
}

template<typename ValueType>
ValueType max(ValueType const& first, ValueType const& second) {
    return std::max(first, second);
}

template<typename ValueType>
ValueType min(ValueType const& first, ValueType const& second) {
    return std::min(first, second);
}

template<typename ValueType>
ValueType sqrt(ValueType const& number) {
    return std::sqrt(number);
}

template<typename ValueType>
ValueType abs(ValueType const& number) {
    return std::fabs(number);
}

template<typename ValueType>
ValueType floor(ValueType const& number) {
    return std::floor(number);
}

template<typename ValueType>
ValueType ceil(ValueType const& number) {
    return std::ceil(number);
}

template<typename ValueType>
ValueType round(ValueType const& number) {
    // Rounding towards infinity
    return floor<ValueType>(number + storm::numbers::convert<ValueType>(0.5));
}

template<typename ValueType>
ValueType log(ValueType const& number) {
    return std::log(number);
}

template<typename ValueType>
ValueType log10(ValueType const& number) {
    return std::log10(number);
}

template<typename ValueType>
ValueType cos(ValueType const& number) {
    return std::cos(number);
}

template<typename ValueType>
ValueType sin(ValueType const& number) {
    return std::sin(number);
}

template<typename ValueType>
uint64_t numDigits(ValueType const& number) {
    auto numDigits = 0;
    ValueType remaining = storm::numbers::one<ValueType>() / number;
    ValueType ten = storm::numbers::convert<ValueType>(10);
    while (remaining >= storm::numbers::one<ValueType>()) {
        ++numDigits;
        remaining = storm::numbers::floor<ValueType>(remaining / ten);
    }
    return numDigits;
}

template<typename ValueType>
uint64_t bitsize(ValueType const& number) {
    if constexpr (std::is_same_v<ValueType, uint64_t>) {
        return std::bit_width(number);
    } else {
        if (storm::numbers::isZero(number)) {
            return 0;
        } else {
            // GMPs sizeinbase returns 1 if number is zero.
            // see https://gmplib.org/manual/Miscellaneous-Integer-Functions
            return carl::bitsize(number);
        }
    }
}

template<typename ValueType>
typename NumberTraits<ValueType>::IntegerType trunc(ValueType const& number) {
    return static_cast<typename NumberTraits<ValueType>::IntegerType>(std::trunc(number));
}

template<typename IntegerType>
IntegerType mod(IntegerType const& first, IntegerType const& second) {
    return std::fmod(first, second);
}

template<typename IntegerType>
std::pair<IntegerType, IntegerType> divide(IntegerType const& dividend, IntegerType const& divisor) {
    return std::make_pair(dividend / divisor, mod(dividend, divisor));
}

template<typename ValueType>
std::string to_string(ValueType const& value) {
    std::stringstream ss;
    ss.precision(std::numeric_limits<ValueType>::max_digits10 + 2);
    ss << value;
    return ss.str();
}

#if defined(STORM_HAVE_CLN)
template<>
storm::ClnRationalNumber infinity() {
    // FIXME: this should be treated more properly.
    return storm::ClnRationalNumber(100000000000);
}

template<>
bool isOne(storm::ClnRationalNumber const& a) {
    return carl::isOne(a);
}

template<>
bool isZero(storm::ClnRationalNumber const& a) {
    return carl::isZero(a);
}

template<>
bool isInteger(storm::ClnRationalNumber const& number) {
    return carl::isInteger(number);
}

template<>
std::pair<storm::ClnRationalNumber, storm::ClnRationalNumber> minmax(std::vector<storm::ClnRationalNumber> const& values) {
    STORM_LOG_ASSERT(!values.empty(), "Expected non-empty values.");
    storm::ClnRationalNumber min = values.front();
    storm::ClnRationalNumber max = values.front();
    for (auto const& vt : values) {
        if (vt == storm::numbers::infinity<storm::ClnRationalNumber>()) {
            max = vt;
        } else {
            if (vt < min) {
                min = vt;
            }
            if (vt > max) {
                max = vt;
            }
        }
    }
    return std::make_pair(min, max);
}

template<>
uint_fast64_t convert(ClnRationalNumber const& number) {
    return carl::toInt<carl::uint>(number);
}

template<>
int_fast64_t convert(ClnRationalNumber const& number) {
    return carl::toInt<carl::sint>(number);
}

template<>
ClnRationalNumber convert(double const& number) {
    return carl::rationalize<ClnRationalNumber>(number);
}

template<>
ClnRationalNumber convert(int const& number) {
    return carl::rationalize<ClnRationalNumber>(number);
}

template<>
ClnRationalNumber convert(NumberTraits<ClnRationalNumber>::IntegerType const& number) {
    return ClnRationalNumber(number);
}

template<>
ClnRationalNumber convert(uint_fast64_t const& number) {
    STORM_LOG_ASSERT(static_cast<carl::uint>(number) == number, "Rationalizing failed, because the number is too large.");
    return carl::rationalize<ClnRationalNumber>(static_cast<carl::uint>(number));
}

template<>
int64_t convert(NumberTraits<ClnRationalNumber>::IntegerType const& number) {
    return carl::toInt<carl::sint>(number);
}

template<>
typename NumberTraits<ClnRationalNumber>::IntegerType convert(uint_fast64_t const& number) {
    STORM_LOG_ASSERT(static_cast<unsigned long int>(number) == number, "Conversion failed, because the number is too large.");
    return NumberTraits<ClnRationalNumber>::IntegerType(static_cast<unsigned long int>(number));
}

template<>
typename NumberTraits<ClnRationalNumber>::IntegerType convert(int_fast64_t const& number) {
    STORM_LOG_ASSERT(static_cast<long int>(number) == number, "Conversion failed, because the number is too large.");
    return NumberTraits<ClnRationalNumber>::IntegerType(static_cast<long int>(number));
}

template<>
typename NumberTraits<ClnRationalNumber>::IntegerType convert(double const& number) {
    if (number < static_cast<double>(std::numeric_limits<uint64_t>::max())) {
        return NumberTraits<ClnRationalNumber>::IntegerType(static_cast<uint64_t>(number));
    } else {
        return carl::round(carl::rationalize<ClnRationalNumber>(number));
    }
}

template<>
ClnRationalNumber convert(int_fast64_t const& number) {
    STORM_LOG_ASSERT(static_cast<carl::sint>(number) == number, "Rationalizing failed, because the number is too large.");
    return carl::rationalize<ClnRationalNumber>(static_cast<carl::sint>(number));
}

template<>
double convert(ClnRationalNumber const& number) {
    return carl::toDouble(number);
}

template<>
ClnRationalNumber convert(std::string const& number) {
    ClnRationalNumber result;
    if (carl::try_parse<ClnRationalNumber>(number, result)) {
        return result;
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Unable to parse '" << number << "' as a rational number.");
}

template<>
std::pair<ClnRationalNumber, ClnRationalNumber> asFraction(ClnRationalNumber const& number) {
    return std::make_pair(carl::getNum(number), carl::getDenom(number));
}

template<>
ClnRationalNumber sqrt(ClnRationalNumber const& number) {
    return carl::sqrt(number);
}

template<>
ClnRationalNumber abs(storm::ClnRationalNumber const& number) {
    return carl::abs(number);
}

template<>
ClnRationalNumber floor(storm::ClnRationalNumber const& number) {
    return carl::floor(number);
}

template<>
ClnRationalNumber ceil(storm::ClnRationalNumber const& number) {
    return carl::ceil(number);
}

template<>
ClnRationalNumber log(ClnRationalNumber const& number) {
    return carl::log(number);
}

template<>
ClnRationalNumber log10(ClnRationalNumber const& number) {
    return carl::log10(number);
}

template<>
ClnRationalNumber cos(ClnRationalNumber const& number) {
    return carl::cos(number);
}

template<>
ClnRationalNumber sin(ClnRationalNumber const& number) {
    return carl::sin(number);
}

template<>
typename NumberTraits<ClnRationalNumber>::IntegerType trunc(ClnRationalNumber const& number) {
    return cln::truncate1(number);
}

template<>
typename NumberTraits<ClnRationalNumber>::IntegerType mod(NumberTraits<ClnRationalNumber>::IntegerType const& first,
                                                          NumberTraits<ClnRationalNumber>::IntegerType const& second) {
    return carl::mod(first, second);
}

template<>
std::pair<typename NumberTraits<ClnRationalNumber>::IntegerType, typename NumberTraits<ClnRationalNumber>::IntegerType> divide(
    typename NumberTraits<ClnRationalNumber>::IntegerType const& dividend, typename NumberTraits<ClnRationalNumber>::IntegerType const& divisor) {
    std::pair<typename NumberTraits<ClnRationalNumber>::IntegerType, typename NumberTraits<ClnRationalNumber>::IntegerType> result;
    carl::divide(dividend, divisor, result.first, result.second);
    return result;
}

template<>
typename NumberTraits<ClnRationalNumber>::IntegerType pow(typename NumberTraits<ClnRationalNumber>::IntegerType const& value, int_fast64_t exponent) {
    STORM_LOG_THROW(exponent >= 0, storm::exceptions::InvalidArgumentException,
                    "Tried to compute the power 'x^y' as an integer, but the exponent 'y' is negative.");
    return carl::pow(value, exponent);
}

template<>
ClnRationalNumber pow(ClnRationalNumber const& value, int_fast64_t exponent) {
    if (exponent >= 0) {
        return carl::pow(value, exponent);
    } else {
        return storm::numbers::one<ClnRationalNumber>() / carl::pow(value, -exponent);
    }
}

template<>
NumberTraits<ClnRationalNumber>::IntegerType numerator(ClnRationalNumber const& number) {
    return carl::getNum(number);
}

template<>
NumberTraits<ClnRationalNumber>::IntegerType denominator(ClnRationalNumber const& number) {
    return carl::getDenom(number);
}
#endif

#if defined(STORM_HAVE_GMP)
template<>
storm::GmpRationalNumber infinity() {
    // FIXME: this should be treated more properly.
    return storm::GmpRationalNumber(100000000000);
}

template<>
bool isOne(storm::GmpRationalNumber const& a) {
    return carl::isOne(a);
}

template<>
bool isZero(storm::GmpRationalNumber const& a) {
    return carl::isZero(a);
}

template<>
bool isInteger(storm::GmpRationalNumber const& number) {
    return carl::isInteger(number);
}

template<>
std::pair<storm::GmpRationalNumber, storm::GmpRationalNumber> minmax(std::vector<storm::GmpRationalNumber> const& values) {
    STORM_LOG_ASSERT(!values.empty(), "Expected non-empty values.");
    storm::GmpRationalNumber min = values.front();
    storm::GmpRationalNumber max = values.front();
    for (auto const& vt : values) {
        if (vt == storm::numbers::infinity<storm::GmpRationalNumber>()) {
            max = vt;
        } else {
            if (vt < min) {
                min = vt;
            }
            if (vt > max) {
                max = vt;
            }
        }
    }
    return std::make_pair(min, max);
}

template<>
std::pair<storm::GmpRationalNumber, storm::GmpRationalNumber> minmax(std::map<uint64_t, storm::GmpRationalNumber> const& values) {
    STORM_LOG_ASSERT(!values.empty(), "Expected non-empty values.");
    storm::GmpRationalNumber min = values.begin()->second;
    storm::GmpRationalNumber max = values.begin()->second;
    for (auto const& vt : values) {
        if (vt.second == storm::numbers::infinity<storm::GmpRationalNumber>()) {
            max = vt.second;
        } else {
            if (vt.second < min) {
                min = vt.second;
            }
            if (vt.second > max) {
                max = vt.second;
            }
        }
    }
    return std::make_pair(min, max);
}

template<>
uint_fast64_t convert(GmpRationalNumber const& number) {
    return carl::toInt<carl::uint>(number);
}

template<>
int_fast64_t convert(GmpRationalNumber const& number) {
    return carl::toInt<carl::sint>(number);
}

template<>
GmpRationalNumber convert(double const& number) {
    return carl::rationalize<GmpRationalNumber>(number);
}

template<>
GmpRationalNumber convert(int const& number) {
    return carl::rationalize<GmpRationalNumber>(number);
}

template<>
GmpRationalNumber convert(uint_fast64_t const& number) {
    STORM_LOG_ASSERT(static_cast<carl::uint>(number) == number, "Rationalizing failed, because the number is too large.");
    return carl::rationalize<GmpRationalNumber>(static_cast<carl::uint>(number));
}

template<>
GmpRationalNumber convert(NumberTraits<GmpRationalNumber>::IntegerType const& number) {
    return GmpRationalNumber(number);
}

template<>
int64_t convert(NumberTraits<GmpRationalNumber>::IntegerType const& number) {
    return carl::toInt<carl::sint>(number);
}

template<>
typename NumberTraits<GmpRationalNumber>::IntegerType convert(uint_fast64_t const& number) {
    STORM_LOG_ASSERT(static_cast<unsigned long int>(number) == number, "Conversion failed, because the number is too large.");
    return NumberTraits<GmpRationalNumber>::IntegerType(static_cast<unsigned long int>(number));
}

template<>
typename NumberTraits<GmpRationalNumber>::IntegerType convert(int_fast64_t const& number) {
    STORM_LOG_ASSERT(static_cast<long int>(number) == number, "Conversion failed, because the number is too large.");
    return NumberTraits<GmpRationalNumber>::IntegerType(static_cast<long int>(number));
}

template<>
typename NumberTraits<GmpRationalNumber>::IntegerType convert(double const& number) {
    return NumberTraits<GmpRationalNumber>::IntegerType(number);
}

template<>
GmpRationalNumber convert(int_fast64_t const& number) {
    STORM_LOG_ASSERT(static_cast<carl::sint>(number) == number, "Rationalizing failed, because the number is too large.");
    return carl::rationalize<GmpRationalNumber>(static_cast<carl::sint>(number));
}

template<>
double convert(GmpRationalNumber const& number) {
    return carl::toDouble(number);
}

template<>
GmpRationalNumber convert(std::string const& number) {
    GmpRationalNumber result;
    if (carl::try_parse<GmpRationalNumber>(number, result)) {
        return result;
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Unable to parse '" << number << "' as a rational number.");
}

template<>
std::pair<GmpRationalNumber, GmpRationalNumber> asFraction(GmpRationalNumber const& number) {
    return std::make_pair(carl::getNum(number), carl::getDenom(number));
}

template<>
GmpRationalNumber sqrt(GmpRationalNumber const& number) {
    return carl::sqrt(number);
}

template<>
GmpRationalNumber abs(storm::GmpRationalNumber const& number) {
    return carl::abs(number);
}

template<>
GmpRationalNumber floor(storm::GmpRationalNumber const& number) {
    return carl::floor(number);
}

template<>
GmpRationalNumber ceil(storm::GmpRationalNumber const& number) {
    return carl::ceil(number);
}

template<>
GmpRationalNumber log(GmpRationalNumber const& number) {
    return carl::log(number);
}

template<>
GmpRationalNumber log10(GmpRationalNumber const& number) {
    STORM_LOG_WARN("Using log10 for GMP rational numbers is not exact, it converts to doubles internally! Avoid if possible.");
    return carl::log10(number);
}

template<>
GmpRationalNumber cos(GmpRationalNumber const& number) {
    return carl::cos(number);
}

template<>
GmpRationalNumber sin(GmpRationalNumber const& number) {
    return carl::sin(number);
}

template<>
typename NumberTraits<GmpRationalNumber>::IntegerType trunc(GmpRationalNumber const& number) {
    return carl::getNum(number) / carl::getDenom(number);
}

template<>
typename NumberTraits<GmpRationalNumber>::IntegerType mod(typename NumberTraits<GmpRationalNumber>::IntegerType const& first,
                                                          typename NumberTraits<GmpRationalNumber>::IntegerType const& second) {
    return carl::mod(first, second);
}

template<>
std::pair<typename NumberTraits<GmpRationalNumber>::IntegerType, typename NumberTraits<GmpRationalNumber>::IntegerType> divide(
    typename NumberTraits<GmpRationalNumber>::IntegerType const& dividend, typename NumberTraits<GmpRationalNumber>::IntegerType const& divisor) {
    std::pair<typename NumberTraits<GmpRationalNumber>::IntegerType, typename NumberTraits<GmpRationalNumber>::IntegerType> result;
    carl::divide(dividend, divisor, result.first, result.second);
    return result;
}

template<>
typename NumberTraits<GmpRationalNumber>::IntegerType pow(typename NumberTraits<GmpRationalNumber>::IntegerType const& value, int_fast64_t exponent) {
    STORM_LOG_THROW(exponent >= 0, storm::exceptions::InvalidArgumentException,
                    "Tried to compute the power 'x^y' as an integer, but the exponent 'y' is negative.");
    return carl::pow(value, exponent);
}

template<>
GmpRationalNumber pow(GmpRationalNumber const& value, int_fast64_t exponent) {
    if (exponent >= 0) {
        return carl::pow(value, exponent);
    } else {
        return storm::numbers::one<GmpRationalNumber>() / carl::pow(value, -exponent);
    }
}

template<>
typename NumberTraits<GmpRationalNumber>::IntegerType numerator(GmpRationalNumber const& number) {
    return carl::getNum(number);
}

template<>
typename NumberTraits<GmpRationalNumber>::IntegerType denominator(GmpRationalNumber const& number) {
    return carl::getDenom(number);
}
#endif

#if defined(STORM_HAVE_GMP) && defined(STORM_HAVE_CLN)
template<>
storm::GmpRationalNumber convert(storm::ClnRationalNumber const& number) {
    return carl::parse<storm::GmpRationalNumber>(to_string(number));
}

template<>
storm::ClnRationalNumber convert(storm::GmpRationalNumber const& number) {
    return carl::parse<storm::ClnRationalNumber>(to_string(number));
}
#endif

template<>
storm::RationalFunction infinity() {
    // FIXME: this should be treated more properly.
    return storm::RationalFunction(convert<RationalFunctionCoefficient>(100000000000));
}

template<>
bool isOne(storm::RationalFunction const& a) {
    return a.isOne();
}

template<>
bool isOne(storm::Polynomial const& a) {
    return a.isOne();
}

template<>
bool isZero(storm::RationalFunction const& a) {
    return a.isZero();
}

template<>
bool isZero(storm::Polynomial const& a) {
    return a.isZero();
}

template<>
bool isConstant(storm::RationalFunction const& a) {
    return a.isConstant();
}

template<>
bool isConstant(storm::Polynomial const& a) {
    return a.isConstant();
}

template<>
bool isApproxEqual(storm::RationalFunction const& a, storm::RationalFunction const& b, storm::RationalFunction const& precision, bool relative) {
    STORM_LOG_ASSERT(isZero(precision), "Approx equal on rational functions is only defined for precision zero.");
    return a == b;
}

template<>
bool isConstant(storm::Interval const& a) {
    return a.isPointInterval();
}

template<>
bool isConstant(storm::RationalInterval const& a) {
    return a.isPointInterval();
}

template<>
bool isInfinity(storm::RationalFunction const& a) {
    // FIXME: this should be treated more properly.
    return a == infinity<storm::RationalFunction>();
}

template<>
bool isInteger(storm::RationalFunction const& func) {
    return storm::numbers::isConstant(func) && storm::numbers::isOne(func.denominator());
}

template<>
RationalFunction convert(double const& number) {
    return RationalFunction(carl::rationalize<RationalFunctionCoefficient>(number));
}

template<>
RationalFunction convert(int_fast64_t const& number) {
    STORM_LOG_ASSERT(static_cast<carl::sint>(number) == number, "Rationalizing failed, because the number is too large.");
    return RationalFunction(carl::rationalize<RationalFunctionCoefficient>(static_cast<carl::sint>(number)));
}

#if defined(STORM_HAVE_CLN)
template<>
RationalFunction convert(ClnRationalNumber const& number) {
    return RationalFunction(convert<storm::RationalFunctionCoefficient>(number));
}

template<>
ClnRationalNumber convert(RationalFunction const& number) {
    storm::RationalFunctionCoefficient tmp = number.nominatorAsNumber() / number.denominatorAsNumber();
    return convert<ClnRationalNumber>(tmp);
}
#endif

#if defined(STORM_HAVE_GMP)
template<>
RationalFunction convert(GmpRationalNumber const& number) {
    return RationalFunction(convert<storm::RationalFunctionCoefficient>(number));
}

template<>
GmpRationalNumber convert(RationalFunction const& number) {
    return convert<GmpRationalNumber>(number.nominatorAsNumber() / number.denominatorAsNumber());
}
#endif

template<>
carl::uint convert(RationalFunction const& func) {
    return carl::toInt<carl::uint>(convert<RationalFunctionCoefficient>(func));
}

template<>
carl::sint convert(RationalFunction const& func) {
    return carl::toInt<carl::sint>(convert<RationalFunctionCoefficient>(func));
}

template<>
double convert(RationalFunction const& func) {
    return carl::toDouble(convert<RationalFunctionCoefficient>(func));
}

template<>
RationalFunction convert(RationalFunction const& number) {
    return number;
}

template<>
RationalFunction convert(std::string const& number) {
    return RationalFunction(convert<RationalFunctionCoefficient>(number));
}

template<>
RationalFunction convert(storm::storage::sparse::state_type const& number) {
    return RationalFunction(convert<RationalFunctionCoefficient>(number));
}

template<>
RationalFunction& simplify(RationalFunction& value);

template<>
RationalFunction&& simplify(RationalFunction&& value);

template<>
RationalFunction simplify(RationalFunction value) {
    value.simplify();
    return value;
}

template<>
RationalFunction& simplify(RationalFunction& value) {
    value.simplify();
    return value;
}

template<>
RationalFunction&& simplify(RationalFunction&& value) {
    value.simplify();
    return std::move(value);
}

template<>
bool isAlmostZero(storm::RationalFunction const& a) {
    return a.isConstant() && isAlmostZero(convert<RationalFunctionCoefficient>(a));
}

template<>
bool isAlmostOne(storm::RationalFunction const& a) {
    return a.isConstant() && isAlmostOne(convert<RationalFunctionCoefficient>(a));
}

template<>
bool isNonNegative(storm::RationalFunction const& a) {
    return a.isConstant() && isNonNegative(convert<RationalFunctionCoefficient>(a));
}

template<>
bool isPositive(storm::RationalFunction const& a) {
    return a.isConstant() && isPositive(convert<RationalFunctionCoefficient>(a));
}

template<>
bool isBetween(storm::RationalFunction const& a, storm::RationalFunction const& b, storm::RationalFunction const& c, bool strict) {
    STORM_LOG_ASSERT(a.isConstant(), "Lower bound must be a constant.");
    STORM_LOG_ASSERT(c.isConstant(), "Upper bound must be a constant.");
    return b.isConstant() &&
           isBetween(convert<RationalFunctionCoefficient>(a), convert<RationalFunctionCoefficient>(b), convert<RationalFunctionCoefficient>(c), strict);
}

template<>
std::pair<storm::RationalFunction, storm::RationalFunction> minmax(std::vector<storm::RationalFunction> const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Minimum/maximum for rational functions is not defined.");
}

template<>
storm::RationalFunction minimum(std::vector<storm::RationalFunction> const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Minimum for rational functions is not defined.");
}

template<>
storm::RationalFunction maximum(std::vector<storm::RationalFunction> const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Maximum for rational functions is not defined.");
}

template<>
std::pair<storm::RationalFunction, storm::RationalFunction> minmax(std::map<uint64_t, storm::RationalFunction> const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Maximum/maximum for rational functions is not defined.");
}

template<>
storm::RationalFunction minimum(std::map<uint64_t, storm::RationalFunction> const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Minimum for rational functions is not defined.");
}

template<>
storm::RationalFunction maximum(std::map<uint64_t, storm::RationalFunction> const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Maximum for rational functions is not defined.");
}

template<>
RationalFunction pow(RationalFunction const& value, int_fast64_t exponent) {
    if (exponent >= 0) {
        return carl::pow(value, exponent);
    } else {
        return storm::numbers::one<RationalFunction>() / carl::pow(value, -exponent);
    }
}

template<>
std::string to_string(RationalFunction const& f) {
    std::stringstream ss;
    if (f.isConstant()) {
        if (f.denominator().isOne()) {
            ss << f.nominatorAsNumber();
        } else {
            ss << f.nominatorAsNumber() << "/" << f.denominatorAsNumber();
        }
    } else if (f.denominator().isOne()) {
        ss << f.nominatorAsPolynomial().coefficient() * f.nominatorAsPolynomial().polynomial();
    } else {
        ss << "(" << f.nominatorAsPolynomial() << ")/(" << f.denominatorAsPolynomial() << ")";
    }
    return ss.str();
}

template<>
double convert(std::string const& value) {
    return convert<double>(convert<storm::RationalNumber>(value));
}

template<>
storm::Interval convert(double const& number) {
    return storm::Interval(number);
}

template<>
storm::Interval convert(uint64_t const& number) {
    return storm::Interval(convert<double>(number));
}

template<>
storm::RationalInterval convert(double const& number) {
    return storm::RationalInterval(convert<storm::RationalNumber>(number));
}

template<>
storm::RationalInterval convert(uint64_t const& number) {
    return storm::RationalInterval(convert<storm::RationalNumber>(number));
}

#if defined(STORM_HAVE_GMP)
template<>
storm::Interval convert(storm::GmpRationalNumber const& n) {
    return storm::Interval(convert<double>(n));
}

template<>
storm::GmpRationalNumber convert(storm::Interval const& number) {
    STORM_LOG_ASSERT(number.isPointInterval(), "Interval must be a point interval to convert.");
    return convert<storm::GmpRationalNumber>(number.lower());
}

template<>
storm::RationalInterval convert(storm::GmpRationalNumber const& n) {
    return storm::RationalInterval(convert<storm::RationalNumber>(n));
}

template<>
storm::GmpRationalNumber convert(storm::RationalInterval const& number) {
    STORM_LOG_ASSERT(number.isPointInterval(), "Interval must be a point interval to convert.");
    return convert<storm::GmpRationalNumber>(number.lower());
}
#endif

#if defined(STORM_HAVE_CLN)
template<>
storm::Interval convert(storm::ClnRationalNumber const& n) {
    return storm::Interval(convert<double>(n));
}

template<>
storm::ClnRationalNumber convert(storm::Interval const& number) {
    STORM_LOG_ASSERT(number.isPointInterval(), "Interval must be a point interval to convert.");
    return convert<storm::ClnRationalNumber>(number.lower());
}

template<>
storm::RationalInterval convert(storm::ClnRationalNumber const& n) {
    return storm::RationalInterval(convert<storm::RationalNumber>(n));
}

template<>
storm::ClnRationalNumber convert(storm::RationalInterval const& number) {
    STORM_LOG_ASSERT(number.isPointInterval(), "Interval must be a point interval to convert.");
    return convert<storm::ClnRationalNumber>(number.lower());
}
#endif

template<>
double convert(storm::Interval const& number) {
    STORM_LOG_ASSERT(number.isPointInterval(), "Interval must be a point interval to convert.");
    return number.lower();
}

template<>
double convert(storm::RationalInterval const& number) {
    STORM_LOG_ASSERT(number.isPointInterval(), "Rational interval must be a point interval to convert.");
    return convert<double>(number.lower());
}

template<>
storm::Interval convert(storm::RationalInterval const& number) {
    using BT = storm::IntervalBaseType<storm::Interval>;
    return storm::Interval(convert<BT>(number.lower()), convert<BT>(number.upper()));
}

template<>
storm::RationalInterval convert(storm::Interval const& number) {
    using BT = storm::IntervalBaseType<storm::RationalInterval>;
    return storm::RationalInterval(convert<BT>(number.lower()), convert<BT>(number.upper()));
}

template<>
storm::Interval abs(storm::Interval const& interval) {
    return interval.abs();
}

template<>
bool isApproxEqual(storm::Interval const& a, storm::Interval const& b, storm::Interval const& precision, bool relative) {
    STORM_LOG_ASSERT(precision.isPointInterval(), "Precision must be a point interval.");
    return isApproxEqual<double>(a.lower(), b.lower(), precision.center(), relative) &&
           isApproxEqual<double>(a.upper(), b.upper(), precision.center(), relative);
}

template<>
bool isApproxEqual(storm::RationalInterval const& a, storm::RationalInterval const& b, storm::RationalInterval const& precision, bool relative) {
    STORM_LOG_ASSERT(precision.isPointInterval(), "Precision must be a point interval.");
    return isApproxEqual<storm::RationalNumber>(a.lower(), b.lower(), precision.center(), relative) &&
           isApproxEqual<storm::RationalNumber>(a.upper(), b.upper(), precision.center(), relative);
}

template<>
storm::RationalInterval abs(storm::RationalInterval const& interval) {
    return interval.abs();
}

// Explicit instantiations.

// double
template double one();
template double zero();
template double infinity();
template bool isOne(double const& value);
template bool isZero(double const& value);
template bool isNonNegative(double const& value);
template bool isPositive(double const& value);
template bool isAlmostZero(double const& value);
template bool isAlmostOne(double const& value);
template bool isConstant(double const& value);
template bool isInfinity(double const& value);
template bool isInteger(double const& number);
template bool isBetween(double const& a, double const& b, double const& c, bool strict);
template bool isApproxEqual(double const& a, double const& b, double const& precision, bool relative);
template double simplify(double value);
template std::pair<double, double> minmax(std::vector<double> const&);
template double minimum(std::vector<double> const&);
template double maximum(std::vector<double> const&);
template std::pair<double, double> minmax(std::map<uint64_t, double> const&);
template double minimum(std::map<uint64_t, double> const&);
template double maximum(std::map<uint64_t, double> const&);
template double pow(double const& value, int_fast64_t exponent);
template double max(double const& first, double const& second);
template double min(double const& first, double const& second);
template double sqrt(double const& number);
template double abs(double const& number);
template double floor(double const& number);
template double ceil(double const& number);
template double round(double const& number);
template double log(double const& number);
template double log10(double const& number);
template double cos(double const& number);
template double sin(double const& number);
template typename NumberTraits<double>::IntegerType trunc(double const& number);
template double mod(double const& first, double const& second);
template std::string to_string(double const& value);

// int
template int one();
template int zero();
template bool isOne(int const& value);
template bool isZero(int const& value);
template bool isConstant(int const& value);
template bool isInfinity(int const& value);
template bool isNonNegative(int const& value);
template bool isPositive(int const& value);
template bool isApproxEqual(int const& a, int const& b, int const& precision, bool relative);
template bool isBetween(int const& a, int const& b, int const& c, bool strict);

// uint32_t
template uint32_t one();
template uint32_t zero();
template bool isOne(uint32_t const& value);
template bool isZero(uint32_t const& value);
template bool isConstant(uint32_t const& value);
template bool isNonNegative(uint32_t const& value);
template bool isPositive(uint32_t const& value);
template bool isInfinity(uint32_t const& value);
template bool isBetween(uint32_t const& a, uint32_t const& b, uint32_t const& c, bool strict);

// storm::storage::sparse::state_type
template storm::storage::sparse::state_type one();
template storm::storage::sparse::state_type zero();
template bool isApproxEqual(storm::storage::sparse::state_type const& a, storm::storage::sparse::state_type const& b,
                            storm::storage::sparse::state_type const& precision, bool relative);
template bool isOne(storm::storage::sparse::state_type const& value);
template bool isZero(storm::storage::sparse::state_type const& value);
template bool isConstant(storm::storage::sparse::state_type const& value);
template bool isPositive(storm::storage::sparse::state_type const& value);
template bool isNonNegative(storm::storage::sparse::state_type const& value);
template bool isInfinity(storm::storage::sparse::state_type const& value);
template bool isBetween(storm::storage::sparse::state_type const& a, storm::storage::sparse::state_type const& b, storm::storage::sparse::state_type const& c,
                        bool strict);
template uint64_t bitsize(storm::storage::sparse::state_type const& number);

// int64_t
template int64_t zero();
template int64_t one();
template int64_t convert(int64_t const&);

// other instantiations
template unsigned long convert(long const&);
template double convert(long const&);

#if defined(STORM_HAVE_CLN)
// Instantiations for (CLN) rational number.
template storm::ClnRationalNumber one();
template NumberTraits<storm::ClnRationalNumber>::IntegerType one();
template storm::ClnRationalNumber zero();
template NumberTraits<storm::ClnRationalNumber>::IntegerType zero();
template bool isZero(NumberTraits<storm::ClnRationalNumber>::IntegerType const& value);
template bool isConstant(storm::ClnRationalNumber const& value);
template bool isPositive(storm::ClnRationalNumber const& value);
template bool isNonNegative(storm::ClnRationalNumber const& value);
template bool isInfinity(storm::ClnRationalNumber const& value);
template bool isNan(storm::ClnRationalNumber const& value);
template bool isAlmostZero(storm::ClnRationalNumber const& value);
template bool isAlmostOne(storm::ClnRationalNumber const& value);
template bool isApproxEqual(storm::ClnRationalNumber const& a, storm::ClnRationalNumber const& b, storm::ClnRationalNumber const& precision, bool relative);
template bool isBetween(storm::ClnRationalNumber const& a, storm::ClnRationalNumber const& b, storm::ClnRationalNumber const& c, bool strict);
template storm::numbers::NumberTraits<ClnRationalNumber>::IntegerType convert(storm::numbers::NumberTraits<ClnRationalNumber>::IntegerType const& number);
template storm::ClnRationalNumber convert(storm::ClnRationalNumber const& number);
template storm::ClnRationalNumber simplify(storm::ClnRationalNumber value);
template std::pair<storm::ClnRationalNumber, storm::ClnRationalNumber> minmax(std::map<uint64_t, storm::ClnRationalNumber> const&);
template storm::ClnRationalNumber minimum(std::map<uint64_t, storm::ClnRationalNumber> const&);
template storm::ClnRationalNumber maximum(std::map<uint64_t, storm::ClnRationalNumber> const&);
template storm::ClnRationalNumber minimum(std::vector<storm::ClnRationalNumber> const&);
template storm::ClnRationalNumber maximum(std::vector<storm::ClnRationalNumber> const&);
template storm::ClnRationalNumber max(storm::ClnRationalNumber const& first, storm::ClnRationalNumber const& second);
template storm::ClnRationalNumber min(storm::ClnRationalNumber const& first, storm::ClnRationalNumber const& second);
template storm::ClnRationalNumber round(storm::ClnRationalNumber const& number);
template std::string to_string(storm::ClnRationalNumber const& value);
template uint64_t numDigits(const storm::ClnRationalNumber& number);
template uint64_t bitsize(storm::ClnIntegerNumber const& number);
#endif

#if defined(STORM_HAVE_GMP)
// Instantiations for (GMP) rational number.
template storm::GmpRationalNumber one();
template NumberTraits<storm::GmpRationalNumber>::IntegerType one();
template storm::GmpRationalNumber zero();
template NumberTraits<storm::GmpRationalNumber>::IntegerType zero();
template bool isZero(NumberTraits<storm::GmpRationalNumber>::IntegerType const& value);
template bool isConstant(storm::GmpRationalNumber const& value);
template bool isPositive(storm::GmpRationalNumber const& value);
template bool isNonNegative(storm::GmpRationalNumber const& value);
template bool isInfinity(storm::GmpRationalNumber const& value);
template bool isNan(storm::GmpRationalNumber const& value);
template bool isAlmostZero(storm::GmpRationalNumber const& value);
template bool isAlmostOne(storm::GmpRationalNumber const& value);
template bool isBetween(storm::GmpRationalNumber const&, storm::GmpRationalNumber const&, storm::GmpRationalNumber const&, bool);
template bool isApproxEqual(storm::GmpRationalNumber const& a, storm::GmpRationalNumber const& b, storm::GmpRationalNumber const& precision, bool relative);
template storm::numbers::NumberTraits<GmpRationalNumber>::IntegerType convert(storm::numbers::NumberTraits<GmpRationalNumber>::IntegerType const& number);
template storm::GmpRationalNumber convert(storm::GmpRationalNumber const& number);
template storm::GmpRationalNumber simplify(storm::GmpRationalNumber value);
template storm::GmpRationalNumber minimum(std::map<uint64_t, storm::GmpRationalNumber> const&);
template storm::GmpRationalNumber maximum(std::map<uint64_t, storm::GmpRationalNumber> const&);
template storm::GmpRationalNumber minimum(std::vector<storm::GmpRationalNumber> const&);
template storm::GmpRationalNumber maximum(std::vector<storm::GmpRationalNumber> const&);
template storm::GmpRationalNumber max(storm::GmpRationalNumber const& first, storm::GmpRationalNumber const& second);
template storm::GmpRationalNumber min(storm::GmpRationalNumber const& first, storm::GmpRationalNumber const& second);
template storm::GmpRationalNumber round(storm::GmpRationalNumber const& number);
template std::string to_string(storm::GmpRationalNumber const& value);
template uint64_t numDigits(const storm::GmpRationalNumber& number);
template uint64_t bitsize(storm::GmpIntegerNumber const& number);
#endif

// Instantiations for rational function.
template RationalFunction one();
template RationalFunction zero();

template bool isNan(RationalFunction const&);

// Instantiations for polynomials.
template Polynomial one();
template Polynomial zero();

// Instantiations for intervals.
template Interval one();
template Interval zero();
template bool isOne(Interval const& value);
template bool isZero(Interval const& value);
template bool isInfinity(Interval const& value);
template bool isAlmostZero(Interval const& value);
template bool isNonNegative(Interval const& value);
template bool isPositive(Interval const& value);
template bool isBetween(Interval const&, Interval const&, Interval const& value, bool);
template Interval convert(Interval const&);

template std::string to_string(storm::Interval const& value);

// Instantiations for rational intervals.
template RationalInterval one();
template RationalInterval zero();
template bool isOne(RationalInterval const& value);
template bool isZero(RationalInterval const& value);
template bool isInfinity(RationalInterval const& value);
template bool isAlmostZero(RationalInterval const& value);
template bool isNonNegative(RationalInterval const& value);
template bool isPositive(RationalInterval const& value);
template bool isBetween(RationalInterval const&, RationalInterval const&, RationalInterval const& value, bool);
template RationalInterval convert(RationalInterval const&);

template std::string to_string(storm::RationalInterval const& value);
}  // namespace numbers
}  // namespace storm
