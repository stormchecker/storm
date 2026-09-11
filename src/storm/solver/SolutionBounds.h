#pragma once

#include <optional>
#include <vector>

namespace storm::solver {

/*!
 * Sound lower and upper bounds on a solution vector. Both have the same size as the solution. They are tracked
 * independently, as an algorithm may establish only one of them; an unset bound means that side is not known.
 */
template<typename ValueType>
struct SolutionBounds {
    std::optional<std::vector<ValueType>> lower;
    std::optional<std::vector<ValueType>> upper;

    /*
     * Returns true if the lower bound is set.
     */
    bool hasLower() const {
        return lower.has_value();
    }

    /*
     * Returns true if the upper bound is set.
     */
    bool hasUpper() const {
        return upper.has_value();
    }

    /*
     * Returns true if at least one of the bounds is set.
     */
    bool hasAny() const {
        return hasLower() || hasUpper();
    }

    /*
     * Clears both bounds.
     */
    void clear() {
        lower = std::nullopt;
        upper = std::nullopt;
    }
};

}  // namespace storm::solver
