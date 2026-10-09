#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <vector>

#include "storm/utility/constants.h"

namespace storm::solver {

/*!
 * Sound lower and upper bounds on a solution vector. Both have the same size as the solution. They are tracked
 * independently, as an algorithm may establish only one of them; an unset bound means that side is not known.
 * Soundness refers to the approximation error of the algorithm. If ValueType is not exact (e.g. double), rounding errors may occur.
 */
template<typename ValueType>
struct SolutionBounds {
    std::optional<std::vector<ValueType>> lower;
    std::optional<std::vector<ValueType>> upper;

    /*!
     * Returns true if the lower bound is set.
     */
    bool hasLower() const {
        return lower.has_value();
    }

    /*!
     * Returns true if the upper bound is set.
     */
    bool hasUpper() const {
        return upper.has_value();
    }

    /*!
     * Returns true if at least one of the bounds is set.
     */
    bool hasAny() const {
        return hasLower() || hasUpper();
    }

    /*!
     * Returns true if every one of the given values lies within the bounds that are set. Meant for assertions.
     */
    bool encloses(std::vector<ValueType> const& values) const {
        auto const isBoundedBy = [&values](std::optional<std::vector<ValueType>> const& bound, bool fromBelow) {
            if (!bound.has_value()) {
                return true;
            }
            if (bound->size() != values.size()) {
                return false;
            }
            for (uint64_t i = 0; i < values.size(); ++i) {
                if (fromBelow ? (*bound)[i] > values[i] : (*bound)[i] < values[i]) {
                    return false;
                }
            }
            return true;
        };
        return isBoundedBy(lower, true) && isBoundedBy(upper, false);
    }

    /*!
     * Returns true if both bounds are set and coincide.
     */
    bool isExact() const {
        return hasLower() && hasUpper() && *lower == *upper;
    }

    /*!
     * Returns true if both bounds are set and pin every value down to the given precision, i.e. if they are no
     * further apart than that precision allows.
     */
    bool isWithinPrecision(ValueType const& precision, bool relative) const {
        if (!hasLower() || !hasUpper()) {
            return false;
        }
        ValueType const zero = storm::utility::zero<ValueType>();
        for (uint64_t i = 0; i < lower->size(); ++i) {
            ValueType const& low = (*lower)[i];
            ValueType const& up = (*upper)[i];
            ValueType allowed = precision;
            if (relative) {
                // The enclosed value is at least as far from zero as the bound closer to it, unless the two
                // straddle zero, where only coinciding bounds say anything.
                allowed = (low <= zero && up >= zero) ? zero : precision * std::min(storm::utility::abs<ValueType>(low), storm::utility::abs<ValueType>(up));
            }
            // Negated, so that a width that is not a number is rejected.
            if (!(up - low <= allowed)) {
                return false;
            }
        }
        return true;
    }

    /*!
     * Sets both bounds to the given values, i.e. states that these values are known exactly.
     */
    void setExact(std::vector<ValueType> const& values) {
        lower = values;
        upper = values;
    }

    /*!
     * Turns bounds on a probability p into bounds on 1-p, i.e. the new lower bound is one minus the old upper bound and vice versa.
     */
    void invertProbabilityBounds() {
        auto const oneMinus = [](ValueType const& value) { return storm::utility::one<ValueType>() - value; };
        if (hasLower()) {
            std::ranges::transform(*lower, lower->begin(), oneMinus);
        }
        if (hasUpper()) {
            std::ranges::transform(*upper, upper->begin(), oneMinus);
        }
        std::swap(lower, upper);
    }

    /*!
     * Applies the given function to whichever of the bounds are set, e.g. to carry them along a transformation of
     * the solution vector they belong to.
     */
    template<typename Function>
    auto transform(Function const& function) const {
        SolutionBounds<typename std::invoke_result_t<Function, std::vector<ValueType> const&>::value_type> result;
        if (hasLower()) {
            result.lower = function(*lower);
        }
        if (hasUpper()) {
            result.upper = function(*upper);
        }
        return result;
    }

    /*!
     * Widens whichever bounds are set until they enclose the given values. A bound only ever moves away from the
     * solution that way, so it stays sound.
     */
    void widenTo(std::vector<ValueType> const& values) {
        if (hasLower()) {
            std::ranges::transform(*lower, values, lower->begin(), [](ValueType const& bound, ValueType const& value) { return std::min(bound, value); });
        }
        if (hasUpper()) {
            std::ranges::transform(*upper, values, upper->begin(), [](ValueType const& bound, ValueType const& value) { return std::max(bound, value); });
        }
    }

    /*!
     * Clears both bounds.
     */
    void clear() {
        lower = std::nullopt;
        upper = std::nullopt;
    }
};

}  // namespace storm::solver
