#include "ExplicitQuantitativeResultMinMax.h"

#include "storm/adapters/RationalNumberAdapter.h"

namespace storm::gbar {
namespace abstraction {

template<typename ValueType>
ExplicitQuantitativeResultMinMax<ValueType>::ExplicitQuantitativeResultMinMax(uint64_t numberOfStates) : min(numberOfStates), max(numberOfStates) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQuantitativeResult<ValueType> const& ExplicitQuantitativeResultMinMax<ValueType>::getMin() const {
    return min;
}

template<typename ValueType>
ExplicitQuantitativeResult<ValueType>& ExplicitQuantitativeResultMinMax<ValueType>::getMin() {
    return min;
}

template<typename ValueType>
ExplicitQuantitativeResult<ValueType> const& ExplicitQuantitativeResultMinMax<ValueType>::getMax() const {
    return max;
}

template<typename ValueType>
ExplicitQuantitativeResult<ValueType>& ExplicitQuantitativeResultMinMax<ValueType>::getMax() {
    return max;
}

template<typename ValueType>
void ExplicitQuantitativeResultMinMax<ValueType>::setMin(ExplicitQuantitativeResult<ValueType>&& newMin) {
    min = std::move(newMin);
}

template<typename ValueType>
void ExplicitQuantitativeResultMinMax<ValueType>::setMax(ExplicitQuantitativeResult<ValueType>&& newMax) {
    max = std::move(newMax);
}

template<typename ValueType>
ExplicitQuantitativeResult<ValueType> const& ExplicitQuantitativeResultMinMax<ValueType>::get(storm::OptimizationDirection const& dir) const {
    if (dir == storm::OptimizationDirection::Minimize) {
        return this->getMin();
    } else {
        return this->getMax();
    }
}

template<typename ValueType>
ExplicitQuantitativeResult<ValueType>& ExplicitQuantitativeResultMinMax<ValueType>::get(storm::OptimizationDirection const& dir) {
    if (dir == storm::OptimizationDirection::Minimize) {
        return this->getMin();
    } else {
        return this->getMax();
    }
}

template class ExplicitQuantitativeResultMinMax<double>;
template class ExplicitQuantitativeResultMinMax<storm::RationalNumber>;
}  // namespace abstraction
}  // namespace storm::gbar
