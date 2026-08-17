#pragma once

#include <iostream>

namespace storm::analysis {
/*!
 * The results of monotonicity checking for a single Parameter Region
 */
enum class MonotonicityKind {
    Incr,     /*!< the result is monotonically increasing */
    Decr,     /*!< the result is monotonically decreasing */
    Constant, /*!< the result is constant */
    Not,      /*!< the result is not monotonic */
    Unknown   /*!< the monotonicity result is unknown */
};

std::ostream& operator<<(std::ostream& out, MonotonicityKind kind);

bool isMonotone(MonotonicityKind kind);

}  // namespace storm::analysis