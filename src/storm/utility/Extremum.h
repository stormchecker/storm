#pragma once

#include <limits>
#include <optional>
#include <type_traits>

#include "storm/solver/OptimizationDirection.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/NumberTraits.h"

namespace storm::utility {

/*!
 * Stores and manages an extremal (maximal or minimal) value
 */
template<storm::OptimizationDirection Dir, typename ValueType>
class Extremum {
   public:
    Extremum() = default;
    Extremum(ValueType const& value);
    Extremum(ValueType&& value);
    Extremum(Extremum const&) = default;
    Extremum(Extremum&&) = default;
    Extremum& operator=(Extremum const&) = default;
    Extremum& operator=(Extremum&&) = default;
    ~Extremum() = default;

    /*!
     * Sets the extremum to the given value
     * @return a reference to this
     */
    Extremum& operator=(ValueType const& value);

    /*!
     * Sets the extremum to the given value
     * @return a reference to this
     */
    Extremum& operator=(ValueType&& value);

    /*!
     * @param value
     * @return True if the provided value is strictly better (larger if we maximize; smaller if we minimize) than the stored value
     */
    bool better(ValueType const& value) const;

    /*!
     * @param value
     * @return True if the provided value is strictly better (larger if we maximize; smaller if we minimize) than the stored value
     */
    template<typename ExtendedType>
        requires(std::is_same_v<ExtendedType, storm::utility::ExtendedValueType<ValueType>> &&
                 !std::is_same_v<storm::utility::ExtendedValueType<ValueType>, ValueType>)
    bool better(ExtendedType const& value) const {
        if constexpr (storm::solver::minimize(Dir)) {
            return value < extremalValue;
        } else {
            static_assert(storm::solver::maximize(Dir));
            return value > extremalValue;
        }
    }

    /*!
     * Updates the stored value, if the given extremal value is better.
     * @param other
     * @return true if the extremum value of this changed
     */
    bool operator&=(Extremum const& other);

    /*!
     * Updates the stored value, if the given extremal value is better.
     * @param other
     * @return true if the extremum value of this changed
     */
    bool operator&=(Extremum&& other);

    /*!
     * Updates the stored value, if the given value is better.
     * @param other
     * @return true if the extremum value of this changed
     */
    bool operator&=(ValueType const& value);

    /*!
     * Updates the stored value, if the given value is better.
     * @param other
     * @return true if the extremum value of this changed
     */
    bool operator&=(ValueType&& value);

    /*!
     * @return true if the stored value is the extremum over the empty set
     */
    bool empty() const;

    /*!
     * @pre the extremal value is finite, i.e., not empty.
     * @return the stored extremal value
     */
    ValueType const& operator*() const;

    /*!
     * @pre the extremal value is finite, i.e., not empty.
     * @return the stored extremal value
     */
    ValueType& operator*();

    /*!
     * @return the stored extremal value as an optional. Returns std::nullopt if this is empty
     */
    std::optional<ValueType> getOptionalValue() const;

    /*!
     * @return the stored extremal value, including an infinite one
     */
    storm::utility::ExtendedValueType<ValueType> const& getExtendedValue() const;

    /*!
     * Updates the stored value, if the given value is better.
     * @return true if the extremum value of this changed
     */
    template<typename ExtendedType>
        requires(std::is_same_v<ExtendedType, storm::utility::ExtendedValueType<ValueType>> &&
                 !std::is_same_v<storm::utility::ExtendedValueType<ValueType>, ValueType>)
    bool operator&=(ExtendedType const& value) {
        if (better(value)) {
            extremalValue = value;
            return true;
        }
        return false;
    }

    /*!
     * Forgets the extremal value so that this represents the extremum over an empty set.
     */
    void reset();

   private:
    /// True if ValueType brings its own infinity, in which case the value is stored in ValueType itself.
    static bool const StoresPlainValues = std::is_same_v<storm::utility::ExtendedValueType<ValueType>, ValueType>;
    static_assert(!StoresPlainValues || std::numeric_limits<ValueType>::has_infinity, "NumberTraits claims an infinity that numeric_limits cannot provide.");

    /// @return the value an extremum over an empty set has.
    static storm::utility::ExtendedValueType<ValueType> baseValue() {
        if constexpr (StoresPlainValues) {
            // Taken from numeric_limits rather than from storm::utility so that it stays a compile time constant.
            if constexpr (storm::solver::minimize(Dir)) {
                return std::numeric_limits<ValueType>::infinity();
            } else {
                static_assert(storm::solver::maximize(Dir));
                return -std::numeric_limits<ValueType>::infinity();
            }
        } else {
            if constexpr (storm::solver::minimize(Dir)) {
                return storm::utility::ExtendedValueType<ValueType>::infinity();
            } else {
                static_assert(storm::solver::maximize(Dir));
                return storm::utility::ExtendedValueType<ValueType>::negativeInfinity();
            }
        }
    }

    storm::utility::ExtendedValueType<ValueType> extremalValue{baseValue()};
};

template<typename ValueType>
using Maximum = Extremum<storm::OptimizationDirection::Maximize, ValueType>;
template<typename ValueType>
using Minimum = Extremum<storm::OptimizationDirection::Minimize, ValueType>;

}  // namespace storm::utility