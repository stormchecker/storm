#pragma once

#include <optional>

#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/FilterType.h"
#include "storm/utility/ExtendedNumber.h"

namespace storm {
namespace modelchecker {

/*!
 * The outcome of aggregating a quantitative check result over the states it is for: the aggregate of the values,
 * together with an aggregate of the lower resp. upper bounds where the result carries them. The two bounds are
 * independently optional, exactly as the bounds on the individual values are.
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
     * Aggregates the values of this result with the given filter. Where this result carries sound bounds on its
     * values, the aggregate of those is reported alongside: every aggregation offered here is monotone in each
     * individual value, so aggregating the lower resp. upper bounds bounds the aggregate of the values.
     *
     * The default implementation reports no bounds, which is what a result that cannot carry any should do.
     *
     * @param filter One of MIN, MAX, SUM or AVG.
     */
    virtual AggregatedValue<ExtendedValueType> aggregate(FilterType filter) const;

    virtual bool isQuantitative() const override;
};
}  // namespace modelchecker
}  // namespace storm
