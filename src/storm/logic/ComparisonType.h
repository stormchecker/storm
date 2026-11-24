#pragma once

#include <iosfwd>

#include "storm/utility/macros.h"

namespace storm {
namespace logic {
enum class ComparisonType { Less, LessEqual, Greater, GreaterEqual };

inline bool isStrict(ComparisonType t) {
    return (t == ComparisonType::Less || t == ComparisonType::Greater);
}

inline bool isLowerBound(ComparisonType t) {
    return (t == ComparisonType::Greater || t == ComparisonType::GreaterEqual);
}

inline ComparisonType invert(ComparisonType t) {
    switch (t) {
        case ComparisonType::Less:
            return ComparisonType::GreaterEqual;
        case ComparisonType::LessEqual:
            return ComparisonType::Greater;
        case ComparisonType::Greater:
            return ComparisonType::LessEqual;
        case ComparisonType::GreaterEqual:
            return ComparisonType::Less;
    }
    STORM_LOG_ASSERT(false, "Unknown ComparisonType.");
    // Wrong, but this code should not be reachable. Still, GCC issues a warning.
    return t;
}

inline ComparisonType invertPreserveStrictness(ComparisonType t) {
    switch (t) {
        case ComparisonType::Less:
            return ComparisonType::Greater;
        case ComparisonType::LessEqual:
            return ComparisonType::GreaterEqual;
        case ComparisonType::Greater:
            return ComparisonType::Less;
        case ComparisonType::GreaterEqual:
            return ComparisonType::LessEqual;
    }
    STORM_LOG_ASSERT(false, "Unknown ComparisonType.");
    // Wrong, but this code should not be reachable. Still, GCC issues a warning.
    return t;
}

std::ostream& operator<<(std::ostream& out, ComparisonType const& comparisonType);
}  // namespace logic
}  // namespace storm
