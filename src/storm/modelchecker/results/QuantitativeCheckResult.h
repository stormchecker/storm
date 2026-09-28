#pragma once

#include "storm/modelchecker/results/CheckResult.h"
#include "storm/utility/ExtendedNumber.h"

namespace storm {
namespace modelchecker {
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

    virtual bool isQuantitative() const override;
};
}  // namespace modelchecker
}  // namespace storm
