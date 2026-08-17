#pragma once

#include "storm/adapters/RationalNumberForward.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/logic/ComparisonType.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
struct Bound {
    Bound(ComparisonType comparisonType, storm::expressions::Expression const& threshold) : comparisonType(comparisonType), threshold(threshold) {
        // Intentionally left empty.
    }

    ComparisonType comparisonType;
    storm::expressions::Expression threshold;

    template<typename ValueType>
    bool isSatisfied(ValueType const& compareValue) const;

    storm::RationalNumber evaluateThresholdAsRational() const;

    template<typename ValueType>
    ValueType evaluateThresholdAs() const;

    bool isLowerBound() const {
        return comparisonType == ComparisonType::Greater || comparisonType == ComparisonType::GreaterEqual;
    }

    Bound getInvertedBound() const {
        return Bound(invert(comparisonType), threshold);
    }

    friend std::ostream& operator<<(std::ostream& out, Bound const& bound);
};

inline std::ostream& operator<<(std::ostream& out, Bound const& bound) {
    out << bound.comparisonType << bound.threshold;
    return out;
}
}  // namespace logic

}  // namespace storm
