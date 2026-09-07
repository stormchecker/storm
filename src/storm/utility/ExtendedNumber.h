#pragma once

#include <functional>
#include <limits>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::utility {

/*!
 * A value of ValueType extended with the two infinities -infinity and +infinity.
 *
 * This is intended for value types that cannot represent infinity themselves, i.e., those for which
 * NumberTraits<ValueType>::HasInfinity is false. Types that do have their own infinity (such as double) should be used
 * directly instead; the ExtendedValueType alias below picks the right one.
 *
 * The arithmetic follows the usual conventions for the extended reals. The forms that are not defined there
 * (infinity - infinity, 0 * infinity, and infinity / infinity) throw an InvalidOperationException rather than yielding
 * a NaN, since the exact value types have no NaN to yield.
 *
 * ExtendedNumber is meant for the boundaries of a computation: check results, bounds that are handed around as scalars,
 * and values that are converted between value types. It is deliberately not meant to be used as the value type of a
 * transition matrix or of a solver, where the tag would be paid for in the innermost loops.
 *
 * The operators are hidden friends so that a finite ValueType converts implicitly on either side of them, which lets
 * code that mixes extended and plain values read the way it did before.
 */
template<typename ValueType>
class ExtendedNumber {
   public:
    enum class Kind { NegativeInfinity, Finite, PositiveInfinity };

    /*!
     * Creates a finite value that is zero.
     */
    ExtendedNumber() : kind(Kind::Finite), value(storm::utility::zero<ValueType>()) {
        // Intentionally left empty
    }

    /*!
     * Creates a finite value. This conversion is deliberately implicit so that finite values can be used where an
     * extended value is expected.
     */
    ExtendedNumber(ValueType const& value) : kind(Kind::Finite), value(value) {
        // Intentionally left empty
    }

    ExtendedNumber(ValueType&& value) : kind(Kind::Finite), value(std::move(value)) {
        // Intentionally left empty
    }

    ExtendedNumber(ExtendedNumber const&) = default;
    ExtendedNumber(ExtendedNumber&&) = default;
    ExtendedNumber& operator=(ExtendedNumber const&) = default;
    ExtendedNumber& operator=(ExtendedNumber&&) = default;
    ~ExtendedNumber() = default;

    /*!
     * @return +infinity
     */
    static ExtendedNumber infinity() {
        return ExtendedNumber(Kind::PositiveInfinity);
    }

    /*!
     * @return -infinity
     */
    static ExtendedNumber negativeInfinity() {
        return ExtendedNumber(Kind::NegativeInfinity);
    }

    /*!
     * @return true if this is neither +infinity nor -infinity
     */
    bool isFinite() const {
        return kind == Kind::Finite;
    }

    /*!
     * @return true if this is +infinity or -infinity
     */
    bool isInfinite() const {
        return kind != Kind::Finite;
    }

    /*!
     * @return true if this is +infinity
     */
    bool isPositiveInfinity() const {
        return kind == Kind::PositiveInfinity;
    }

    /*!
     * @return true if this is -infinity
     */
    bool isNegativeInfinity() const {
        return kind == Kind::NegativeInfinity;
    }

    /*!
     * @pre this is finite
     * @return the finite value
     */
    ValueType const& getFinite() const {
        STORM_LOG_ASSERT(isFinite(), "Tried to get the finite value of " << *this << ".");
        return value;
    }

    /*!
     * @pre this is finite
     * @return the finite value
     */
    ValueType& getFinite() {
        STORM_LOG_ASSERT(isFinite(), "Tried to get the finite value of " << *this << ".");
        return value;
    }

    friend bool operator==(ExtendedNumber const& first, ExtendedNumber const& second) {
        if (first.kind != second.kind) {
            return false;
        }
        return !first.isFinite() || first.value == second.value;
    }

    friend bool operator!=(ExtendedNumber const& first, ExtendedNumber const& second) {
        return !(first == second);
    }

    friend bool operator<(ExtendedNumber const& first, ExtendedNumber const& second) {
        if (first.kind != second.kind) {
            return first.kind < second.kind;
        }
        return first.isFinite() && first.value < second.value;
    }

    friend bool operator<=(ExtendedNumber const& first, ExtendedNumber const& second) {
        return !(second < first);
    }

    friend bool operator>(ExtendedNumber const& first, ExtendedNumber const& second) {
        return second < first;
    }

    friend bool operator>=(ExtendedNumber const& first, ExtendedNumber const& second) {
        return !(first < second);
    }

    /*!
     * The comparisons against a plain value. Without these the plain operand is converted to an ExtendedNumber first,
     * which copies it -- and for a value type whose copy allocates, such as a GMP rational, that is a heap allocation
     * per comparison. These read the payload directly instead. They are an exact match where the converting ones need
     * a user defined conversion, so they are preferred wherever they apply and change the meaning of nothing.
     */
    friend bool operator==(ExtendedNumber const& first, ValueType const& second) {
        return first.isFinite() && first.value == second;
    }

    friend bool operator==(ValueType const& first, ExtendedNumber const& second) {
        return second.isFinite() && first == second.value;
    }

    friend bool operator!=(ExtendedNumber const& first, ValueType const& second) {
        return !(first == second);
    }

    friend bool operator!=(ValueType const& first, ExtendedNumber const& second) {
        return !(first == second);
    }

    friend bool operator<(ExtendedNumber const& first, ValueType const& second) {
        // Every finite value is above -infinity and below +infinity.
        return first.isFinite() ? first.value < second : first.isNegativeInfinity();
    }

    friend bool operator<(ValueType const& first, ExtendedNumber const& second) {
        return second.isFinite() ? first < second.value : second.isPositiveInfinity();
    }

    friend bool operator<=(ExtendedNumber const& first, ValueType const& second) {
        return !(second < first);
    }

    friend bool operator<=(ValueType const& first, ExtendedNumber const& second) {
        return !(second < first);
    }

    friend bool operator>(ExtendedNumber const& first, ValueType const& second) {
        return second < first;
    }

    friend bool operator>(ValueType const& first, ExtendedNumber const& second) {
        return second < first;
    }

    friend bool operator>=(ExtendedNumber const& first, ValueType const& second) {
        return !(first < second);
    }

    friend bool operator>=(ValueType const& first, ExtendedNumber const& second) {
        return !(first < second);
    }

    ExtendedNumber operator-() const {
        switch (kind) {
            case Kind::PositiveInfinity:
                return negativeInfinity();
            case Kind::NegativeInfinity:
                return infinity();
            default:
                return ExtendedNumber(-value);
        }
    }

    ExtendedNumber const& operator+() const {
        return *this;
    }

    friend ExtendedNumber operator+(ExtendedNumber const& first, ExtendedNumber const& second) {
        if (first.isFinite() && second.isFinite()) {
            return ExtendedNumber(first.value + second.value);
        }
        if (first.isInfinite() && second.isInfinite()) {
            STORM_LOG_THROW(first.kind == second.kind, storm::exceptions::InvalidOperationException, "Tried to compute " << first << " + " << second << ".");
            return first;
        }
        return first.isInfinite() ? first : second;
    }

    friend ExtendedNumber operator-(ExtendedNumber const& first, ExtendedNumber const& second) {
        return first + (-second);
    }

    friend ExtendedNumber operator*(ExtendedNumber const& first, ExtendedNumber const& second) {
        if (first.isFinite() && second.isFinite()) {
            return ExtendedNumber(first.value * second.value);
        }
        // At least one operand is infinite, so the result is determined by the signs unless the other operand is zero.
        int const signs = first.sign() * second.sign();
        STORM_LOG_THROW(signs != 0, storm::exceptions::InvalidOperationException, "Tried to compute " << first << " * " << second << ".");
        return signs > 0 ? infinity() : negativeInfinity();
    }

    friend ExtendedNumber operator/(ExtendedNumber const& first, ExtendedNumber const& second) {
        STORM_LOG_THROW(!(first.isInfinite() && second.isInfinite()), storm::exceptions::InvalidOperationException,
                        "Tried to compute " << first << " / " << second << ".");
        if (second.isInfinite()) {
            // A finite value divided by an infinite one.
            return ExtendedNumber(storm::utility::zero<ValueType>());
        }
        int const divisorSign = second.sign();
        STORM_LOG_THROW(divisorSign != 0, storm::exceptions::InvalidOperationException, "Tried to compute " << first << " / " << second << ".");
        if (first.isFinite()) {
            return ExtendedNumber(first.value / second.value);
        }
        return (first.sign() * divisorSign) > 0 ? infinity() : negativeInfinity();
    }

    ExtendedNumber& operator+=(ExtendedNumber const& other) {
        return *this = *this + other;
    }

    ExtendedNumber& operator-=(ExtendedNumber const& other) {
        return *this = *this - other;
    }

    ExtendedNumber& operator*=(ExtendedNumber const& other) {
        return *this = *this * other;
    }

    ExtendedNumber& operator/=(ExtendedNumber const& other) {
        return *this = *this / other;
    }

    friend std::ostream& operator<<(std::ostream& out, ExtendedNumber const& number) {
        switch (number.kind) {
            case Kind::PositiveInfinity:
                out << "inf";
                break;
            case Kind::NegativeInfinity:
                out << "-inf";
                break;
            default:
                out << number.value;
                break;
        }
        return out;
    }

   private:
    explicit ExtendedNumber(Kind kind) : kind(kind), value(storm::utility::zero<ValueType>()) {
        // Intentionally left empty
    }

    /*!
     * @return 1 if this is positive, -1 if this is negative, and 0 if this is zero.
     */
    int sign() const {
        switch (kind) {
            case Kind::PositiveInfinity:
                return 1;
            case Kind::NegativeInfinity:
                return -1;
            default:
                if (storm::utility::isZero(value)) {
                    return 0;
                }
                return value < storm::utility::zero<ValueType>() ? -1 : 1;
        }
    }

    Kind kind;

    /// The finite value. Only meaningful if kind is Finite; it is kept at zero otherwise so that copies and comparisons
    /// of infinite values do not depend on leftover data.
    ValueType value;
};

/*!
 * The type to use for values that may be infinite. For value types that have their own infinity, this is the value type
 * itself, so that in particular double keeps using its IEEE infinity and pays nothing for this.
 */
template<typename ValueType>
using ExtendedValueType = std::conditional_t<storm::NumberTraits<ValueType>::HasInfinity, ValueType, ExtendedNumber<ValueType>>;

namespace detail {
template<typename T>
struct IsExtendedNumber : std::false_type {};

template<typename T>
struct IsExtendedNumber<ExtendedNumber<T>> : std::true_type {};

/// The finite value type underlying a (possibly extended) type.
template<typename T>
struct FiniteValueType {
    typedef T type;
};

template<typename T>
struct FiniteValueType<ExtendedNumber<T>> {
    typedef T type;
};
}  // namespace detail

/*!
 * The finite value type underlying a (possibly extended) type. This is what a solver or a matrix operates on once the
 * infinite entries have been split off.
 */
template<typename ValueType>
using FiniteValueType = typename detail::FiniteValueType<ValueType>::type;

/*!
 * @return +infinity, expressed in whichever type is used to extend ValueType with infinities.
 */
template<typename ValueType>
ExtendedValueType<ValueType> positiveInfinity() {
    if constexpr (detail::IsExtendedNumber<ExtendedValueType<ValueType>>::value) {
        return ExtendedValueType<ValueType>::infinity();
    } else {
        return storm::utility::infinity<ValueType>();
    }
}

/*!
 * @return -infinity, expressed in whichever type is used to extend ValueType with infinities.
 */
template<typename ValueType>
ExtendedValueType<ValueType> negativeInfinity() {
    if constexpr (detail::IsExtendedNumber<ExtendedValueType<ValueType>>::value) {
        return ExtendedValueType<ValueType>::negativeInfinity();
    } else {
        return -storm::utility::infinity<ValueType>();
    }
}

/*!
 * @pre the value is finite
 * @return the finite value, whether or not the type it is held in is an extended one. This is for the places that read
 * a single entry out of a result they know to be finite, where copying the whole vector would be wasteful.
 */
template<typename ValueType>
ValueType const& getFinite(ExtendedNumber<ValueType> const& value) {
    return value.getFinite();
}

template<typename ValueType>
    requires(!detail::IsExtendedNumber<ValueType>::value)
ValueType const& getFinite(ValueType const& value) {
    return value;
}

/*!
 * @return whether the value is neither of the two infinities, whether or not the type it is held in is an extended one.
 */
template<typename ValueType>
bool isFinite(ExtendedNumber<ValueType> const& value) {
    return value.isFinite();
}

template<typename ValueType>
    requires(!detail::IsExtendedNumber<ValueType>::value)
bool isFinite(ValueType const& value) {
    if constexpr (storm::NumberTraits<ValueType>::HasInfinity) {
        // A NaN is not infinite, but it is not a finite value either: the guards built on this predicate are there to
        // keep a value that is not a number out of a computation, and a NaN is exactly that.
        return !storm::utility::isInfinity(value) && !storm::utility::isInfinity(ValueType(-value)) && !storm::utility::isNan(value);
    } else {
        return true;
    }
}

/*!
 * The counterpart of widen for a vector all of whose values are finite. The values are taken over rather than copied.
 * @pre none of the values is infinite
 */
template<typename ValueType>
std::vector<ValueType> narrowFinite(std::vector<ExtendedValueType<ValueType>>&& values) {
    if constexpr (std::is_same_v<ExtendedValueType<ValueType>, ValueType>) {
        return std::move(values);
    } else {
        std::vector<ValueType> result;
        result.reserve(values.size());
        for (auto& value : values) {
            STORM_LOG_THROW(value.isFinite(), storm::exceptions::InvalidOperationException, "Tried to narrow " << value << " to a type that cannot hold it.");
            result.push_back(std::move(value.getFinite()));
        }
        return result;
    }
}

/*!
 * The counterpart of widen for a vector all of whose values are finite.
 * @pre none of the values is infinite
 */
template<typename ValueType>
std::vector<ValueType> narrowFinite(std::vector<ExtendedValueType<ValueType>> const& values) {
    if constexpr (std::is_same_v<ExtendedValueType<ValueType>, ValueType>) {
        return values;
    } else {
        std::vector<ValueType> result;
        result.reserve(values.size());
        for (auto const& value : values) {
            STORM_LOG_THROW(value.isFinite(), storm::exceptions::InvalidOperationException, "Tried to narrow " << value << " to a type that cannot hold it.");
            result.push_back(value.getFinite());
        }
        return result;
    }
}

/*!
 * The counterpart of widen for a vector, falling back to the given value wherever a value is infinite. The values are
 * taken over rather than copied.
 */
template<typename ValueType>
std::vector<ValueType> narrowFinite(std::vector<ExtendedValueType<ValueType>>&& values, ValueType const& defaultValue) {
    std::vector<ValueType> result;
    result.reserve(values.size());
    for (auto& value : values) {
        if constexpr (std::is_same_v<ExtendedValueType<ValueType>, ValueType>) {
            result.push_back(storm::utility::isFinite(value) ? std::move(value) : defaultValue);
        } else {
            result.push_back(value.isFinite() ? std::move(value.getFinite()) : defaultValue);
        }
    }
    return result;
}

/*!
 * Narrows an extended value back into the plain value type, for the interfaces that cannot hold an infinite one --
 * a coordinate of a polytope, say. A plain type that has an infinity of its own keeps the value; one that has none
 * has nothing to narrow an infinite value to, so this throws rather than inventing a number for it.
 */
template<typename ValueType>
ValueType narrow(ExtendedValueType<ValueType> const& value) {
    if constexpr (detail::IsExtendedNumber<ExtendedValueType<ValueType>>::value) {
        STORM_LOG_THROW(!value.isInfinite(), storm::exceptions::NotSupportedException,
                        "There is no representation of " << value << " in the value type it would have to be narrowed to.");
        return value.getFinite();
    } else {
        return value;
    }
}

/*!
 * Widens a vector of finite values into the extended value type. Used where a computation that cannot produce an
 * infinite value feeds an interface that can hold one.
 */
template<typename ValueType>
std::vector<ExtendedValueType<ValueType>> widen(std::vector<ValueType>&& values) {
    if constexpr (std::is_same_v<ExtendedValueType<ValueType>, ValueType>) {
        return std::move(values);
    } else {
        std::vector<ExtendedValueType<ValueType>> result;
        result.reserve(values.size());
        for (auto& value : values) {
            result.push_back(ExtendedValueType<ValueType>(std::move(value)));
        }
        return result;
    }
}

/*!
 * Recognises the value that storm::utility::infinity still yields for the value types that have no infinity of their
 * own -- the literal 100000000000 -- and turns it into a real infinity.
 *
 * This is a bridge for the parts of Storm that still produce that sentinel, most of all the decision diagram leaves. It
 * inherits the sentinel's weaknesses, so it is not a place to build on; it disappears together with the sentinel.
 */
template<typename ValueType>
    requires(!std::is_reference_v<ValueType>)
ExtendedValueType<ValueType> fromSentinel(ValueType&& value) {
    STORM_LOG_DEPRECATED(
        "storm::utility::fromSentinel, which exists only for as long as parts of Storm still produce the sentinel that storm::utility::infinity yields.");
    if (storm::utility::isInfinity(value)) {
        return storm::utility::positiveInfinity<ValueType>();
    }
    // The constraint above rules out a reference type, so this forward is a move.
    return std::forward<ValueType>(value);
}

/*!
 * The vector form. It builds the sentinel once instead of once per element: for a value type whose infinity is a
 * number rather than a bit pattern, constructing it allocates, and doing that per element costs more than the
 * widening itself. The values are taken over rather than copied.
 */
template<typename ValueType>
std::vector<ExtendedValueType<ValueType>> fromSentinel(std::vector<ValueType>&& values) {
    STORM_LOG_DEPRECATED(
        "storm::utility::fromSentinel, which exists only for as long as parts of Storm still produce the sentinel that storm::utility::infinity yields.");
    if constexpr (std::is_same_v<ExtendedValueType<ValueType>, ValueType>) {
        return std::move(values);
    } else {
        std::vector<ExtendedValueType<ValueType>> result;
        result.reserve(values.size());
        ValueType const sentinel = storm::utility::infinity<ValueType>();
        for (auto& value : values) {
            if (value == sentinel) {
                result.push_back(storm::utility::positiveInfinity<ValueType>());
            } else {
                result.push_back(std::move(value));
            }
        }
        return result;
    }
}

template<typename ValueType>
ExtendedValueType<ValueType> fromSentinel(ValueType const& value) {
    STORM_LOG_DEPRECATED(
        "storm::utility::fromSentinel, which exists only for as long as parts of Storm still produce the sentinel that storm::utility::infinity yields.");
    if (storm::utility::isInfinity(value)) {
        return storm::utility::positiveInfinity<ValueType>();
    }
    return value;
}

/*!
 * The counterpart of fromSentinel: hands an extended value back to a part of Storm that still expects the sentinel,
 * most of all a decision diagram leaf. The sentinel has no negative infinity, so that direction is rejected rather than
 * silently turned into something else.
 */
template<typename ValueType>
ValueType toSentinel(ExtendedValueType<ValueType> const& value) {
    STORM_LOG_DEPRECATED(
        "storm::utility::toSentinel, which exists only for as long as parts of Storm still expect the sentinel that storm::utility::infinity yields.");
    if constexpr (detail::IsExtendedNumber<ExtendedValueType<ValueType>>::value) {
        if (value.isInfinite()) {
            STORM_LOG_THROW(value.isPositiveInfinity(), storm::exceptions::NotSupportedException, "There is no representation of " << value << " here.");
            return storm::utility::infinity<ValueType>();
        }
        return value.getFinite();
    } else {
        return value;
    }
}

/*!
 * The generic storm::utility::zero, one and infinity are declared for every value type but defined only for the ones
 * that are explicitly instantiated. These overloads are more constrained than those declarations, so they are picked for
 * the extended types and the extended types alone.
 */
template<typename ValueType>
    requires(detail::IsExtendedNumber<ValueType>::value)
ValueType zero() {
    return ValueType(storm::utility::zero<FiniteValueType<ValueType>>());
}

template<typename ValueType>
    requires(detail::IsExtendedNumber<ValueType>::value)
ValueType one() {
    return ValueType(storm::utility::one<FiniteValueType<ValueType>>());
}

template<typename ValueType>
    requires(detail::IsExtendedNumber<ValueType>::value)
ValueType infinity() {
    return ValueType::infinity();
}

/*!
 * @return true if the given value is +infinity. This mirrors the meaning that isInfinity has for the value types that
 * bring their own infinity, where -infinity is not reported either.
 */
template<typename ValueType>
bool isInfinity(ExtendedNumber<ValueType> const& number) {
    return number.isPositiveInfinity();
}

/*!
 * @return true if the given value is -infinity, whether or not the type it is held in is an extended one. This is the
 * counterpart of isInfinity for the places that have to tell the two infinities apart, such as writing a result out.
 */
template<typename ValueType>
bool isNegativeInfinity(ExtendedNumber<ValueType> const& number) {
    return number.isNegativeInfinity();
}

template<typename ValueType>
    requires(!detail::IsExtendedNumber<ValueType>::value)
bool isNegativeInfinity(ValueType const& value) {
    if constexpr (storm::NumberTraits<ValueType>::HasInfinity) {
        return storm::utility::isInfinity(ValueType(-value));
    } else {
        return false;
    }
}

template<typename ValueType>
bool isZero(ExtendedNumber<ValueType> const& number) {
    return number.isFinite() && storm::utility::isZero(number.getFinite());
}

template<typename ValueType>
bool isOne(ExtendedNumber<ValueType> const& number) {
    return number.isFinite() && storm::utility::isOne(number.getFinite());
}

template<typename ValueType>
bool isConstant(ExtendedNumber<ValueType> const& number) {
    return number.isInfinite() || storm::utility::isConstant(number.getFinite());
}

template<typename ValueType>
ExtendedNumber<ValueType> abs(ExtendedNumber<ValueType> const& number) {
    if (number.isInfinite()) {
        return ExtendedNumber<ValueType>::infinity();
    }
    return ExtendedNumber<ValueType>(storm::utility::abs(number.getFinite()));
}

/*!
 * Converts a possibly infinite value to another type, which may itself be an ExtendedNumber or a type that has its own
 * infinity. This is what lets an infinite value cross value types without every call site restating what infinity means
 * in the source and in the target type.
 */
template<typename TargetType, typename SourceType>
TargetType convertNumber(ExtendedNumber<SourceType> const& number) {
    if (number.isFinite()) {
        return TargetType(convertNumber<FiniteValueType<TargetType>, SourceType>(number.getFinite()));
    }
    if constexpr (detail::IsExtendedNumber<TargetType>::value) {
        return number.isPositiveInfinity() ? TargetType::infinity() : TargetType::negativeInfinity();
    } else {
        static_assert(storm::NumberTraits<TargetType>::HasInfinity, "Tried to convert an infinite value to a target type that cannot represent infinity.");
        return number.isPositiveInfinity() ? storm::utility::infinity<TargetType>() : -storm::utility::infinity<TargetType>();
    }
}

/*!
 * Converts a value into an extended one. If the source type has an infinity of its own it is carried over; otherwise
 * this is just the underlying conversion followed by the implicit constructor.
 */
template<typename TargetType, typename SourceType>
    requires(detail::IsExtendedNumber<TargetType>::value && !detail::IsExtendedNumber<SourceType>::value)
TargetType convertNumber(SourceType const& number) {
    if constexpr (storm::NumberTraits<SourceType>::HasInfinity) {
        // The source brings its own infinity, which the underlying conversion has no way of expressing.
        if (storm::utility::isInfinity(number)) {
            return TargetType::infinity();
        }
        if (storm::utility::isInfinity(SourceType(-number))) {
            return TargetType::negativeInfinity();
        }
    }
    return TargetType(convertNumber<FiniteValueType<TargetType>, SourceType>(number));
}

}  // namespace storm::utility

namespace storm {
/*!
 * The rational number type extended with the two infinities. A rational number has no infinity of its own, so this is
 * what a result of that value type is held in. It has a name because it is written often.
 */
using ExtendedRationalNumber = storm::utility::ExtendedValueType<RationalNumber>;

/*!
 * The rational function type extended with the two infinities. Like a rational number, a rational function has no
 * infinity of its own, so this is what a result of that value type is held in.
 */
using ExtendedRationalFunction = storm::utility::ExtendedValueType<RationalFunction>;

/*!
 * An extended number is exactly as exact as what it extends, and it is the type that does have infinity -- that is the
 * whole point of it.
 */
template<typename ValueType>
struct NumberTraits<storm::utility::ExtendedNumber<ValueType>> {
    static const bool SupportsExponential = NumberTraits<ValueType>::SupportsExponential;
    static const bool IsExact = NumberTraits<ValueType>::IsExact;
    static const bool HasInfinity = true;
};
}  // namespace storm

namespace std {
template<typename ValueType>
struct hash<storm::utility::ExtendedNumber<ValueType>> {
    size_t operator()(storm::utility::ExtendedNumber<ValueType> const& number) const {
        // The infinities have no payload to hash, so they get an arbitrary fixed value each.
        if (number.isPositiveInfinity()) {
            return 0x9e3779b9;
        }
        if (number.isNegativeInfinity()) {
            return 0x85ebca6b;
        }
        return std::hash<ValueType>()(number.getFinite());
    }
};

/*!
 * Reports the infinity that ExtendedNumber adds, so that the generic storm::utility::infinity and
 * storm::utility::isInfinity work on it without a special case. The remaining traits are inherited from the underlying
 * type where they still make sense.
 */
template<typename ValueType>
struct numeric_limits<storm::utility::ExtendedNumber<ValueType>> {
    typedef storm::utility::ExtendedNumber<ValueType> type;

    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = storm::NumberTraits<ValueType>::IsExact;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = false;

    static type infinity() {
        return type::infinity();
    }

    static type lowest() {
        return type(std::numeric_limits<ValueType>::lowest());
    }

    static type min() {
        return type(std::numeric_limits<ValueType>::min());
    }

    static type max() {
        return type(std::numeric_limits<ValueType>::max());
    }

    static type epsilon() {
        return type(std::numeric_limits<ValueType>::epsilon());
    }
};
}  // namespace std
