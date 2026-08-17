#include "storm/logic/Bound.h"

#include "storm/adapters/RationalFunctionAdapter.h"

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
#if defined(STORM_HAVE_CLN)
template bool Bound::isSatisfied(storm::ClnRationalNumber const& compareValue) const;
template storm::ClnRationalNumber Bound::evaluateThresholdAs() const;
#endif
#if defined(STORM_HAVE_GMP)
template bool Bound::isSatisfied(storm::GmpRationalNumber const& compareValue) const;
template storm::GmpRationalNumber Bound::evaluateThresholdAs() const;
#endif
template double Bound::evaluateThresholdAs() const;
template storm::RationalFunction Bound::evaluateThresholdAs() const;

}  // namespace storm::logic