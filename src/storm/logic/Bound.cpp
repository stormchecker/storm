#include "storm/logic/Bound.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/utility/ExtendedNumber.h"

namespace storm::logic {

template<typename ValueType>
bool Bound::isSatisfied(ValueType const& compareValue) const {
    ValueType thresholdAsValueType = evaluateThresholdAs<ValueType>();
    switch (comparisonType) {
        case ComparisonType::Greater:
            return compareValue > thresholdAsValueType;
        case ComparisonType::GreaterEqual:
            return compareValue >= thresholdAsValueType;
        case ComparisonType::Less:
            return compareValue < thresholdAsValueType;
        case ComparisonType::LessEqual:
            return compareValue <= thresholdAsValueType;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Unknown ComparisonType.");
}

storm::RationalNumber Bound::evaluateThresholdAsRational() const {
    return threshold.evaluateAsRational();
}

template<typename ValueType>
ValueType Bound::evaluateThresholdAs() const {
    return storm::utility::convertNumber<ValueType>(evaluateThresholdAsRational());
}

template bool Bound::isSatisfied(double const& compareValue) const;
template double Bound::evaluateThresholdAs() const;

template bool Bound::isSatisfied(storm::RationalNumber const& compareValue) const;
template storm::RationalNumber Bound::evaluateThresholdAs() const;
template bool Bound::isSatisfied(storm::utility::ExtendedNumber<storm::RationalNumber> const& compareValue) const;
template storm::utility::ExtendedNumber<storm::RationalNumber> Bound::evaluateThresholdAs() const;

#if STORM_RATIONAL_NUMBER_DIFFERS_FROM_COEFFICIENT
template bool Bound::isSatisfied(storm::RationalFunctionCoefficient const& compareValue) const;
template storm::RationalFunctionCoefficient Bound::evaluateThresholdAs() const;
template bool Bound::isSatisfied(storm::utility::ExtendedNumber<storm::RationalFunctionCoefficient> const& compareValue) const;
template storm::utility::ExtendedNumber<storm::RationalFunctionCoefficient> Bound::evaluateThresholdAs() const;
#endif

template storm::RationalFunction Bound::evaluateThresholdAs() const;

}  // namespace storm::logic
