#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include "storm/utility/constants.h"

namespace storm::solver {

/*!
 * Sound lower and upper bounds on a solution vector. Both have the same size as the solution. They are tracked
 * independently, as an algorithm may establish only one of them; an unset bound means that side is not known.
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
     * Clears both bounds.
     */
    void clear() {
        lower = std::nullopt;
        upper = std::nullopt;
    }
};

}  // namespace storm::solver
