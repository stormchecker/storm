#pragma once

#include <optional>

#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/FilterType.h"
#include "storm/utility/ExtendedNumber.h"

namespace storm {
namespace modelchecker {

/*!
 * The aggregate of the values of a quantitative check result, together with the aggregate of each bound the
 * result carries.
 */
template<typename ValueType>
struct AggregatedValue {
    ValueType value;
    std::optional<ValueType> lower;
    std::optional<ValueType> upper;

    bool hasLower() const {
        return lower.has_value();
    }

    bool hasUpper() const {
        return upper.has_value();
    }
};

template<typename ValueType>
class QuantitativeCheckResult : public CheckResult {
   public:
    typedef storm::utility::ExtendedValueType<ValueType> ExtendedValueType;

    virtual ~QuantitativeCheckResult() = default;

    virtual std::unique_ptr<CheckResult> compareAgainstBound(storm::logic::ComparisonType comparisonType, ValueType const& bound) const;

    virtual void oneMinus() = 0;

    virtual ExtendedValueType getMin() const = 0;
    virtual ExtendedValueType getMax() const = 0;

    virtual ExtendedValueType average() const = 0;
    virtual ExtendedValueType sum() const = 0;

    /*!
     * Aggregates the values of this result with the given filter, and each bound it carries alongside. Every
     * aggregation offered here is monotone in each value, so aggregating a bound bounds the aggregate.
     *
     * @param filter One of MIN, MAX, SUM or AVG.
     */
    virtual AggregatedValue<ExtendedValueType> aggregate(FilterType filter) const;

    virtual bool isQuantitative() const override;
};
}  // namespace modelchecker
}  // namespace storm
