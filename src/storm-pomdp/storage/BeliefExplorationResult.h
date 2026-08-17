#pragma once

#include <optional>

#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::pomdp::storage {
/**
 * Struct used to store the results of the model checker
 */
template<typename ValueType>
struct BeliefExplorationResult {
    BeliefExplorationResult(ValueType lower, ValueType upper) : lowerBound(lower), upperBound(upper) {};
    ValueType diff(bool relative = false) const {
        if (!(upperBound.has_value() && lowerBound.has_value())) {
            STORM_LOG_WARN("Either the upper or the lower bound is not set. Difference is undefined.");
            return storm::utility::infinity<ValueType>();
        }
        ValueType diff = *upperBound - *lowerBound;
        if (diff < storm::utility::zero<ValueType>()) {
            STORM_LOG_WARN_COND(diff >= storm::utility::convertNumber<ValueType>(1e-6),
                                "Upper bound '" << *upperBound << "' is smaller than lower bound '" << *lowerBound << "': Difference is " << diff << ".");
            diff = storm::utility::zero<ValueType>();
        }
        if (relative && !storm::utility::isZero(*upperBound)) {
            diff /= *upperBound;
        }
        return diff;
    };
    bool updateLowerBound(ValueType const& value) {
        if (value > lowerBound) {
            lowerBound = value;
            return true;
        }
        return false;
    };

    bool updateUpperBound(ValueType const& value) {
        if (value < upperBound) {
            upperBound = value;
            return true;
        }
        return false;
    };

    void removeLowerBound() {
        lowerBound = std::nullopt;
    };

    void removeUpperBound() {
        upperBound = std::nullopt;
    };

    std::optional<ValueType> lowerBound = std::nullopt;
    std::optional<ValueType> upperBound = std::nullopt;
};
}  // namespace storm::pomdp::storage
