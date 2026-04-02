#include "storm/generator/DistributionEntry.h"
#include <cstdint>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/BitVector.h"

namespace storm::generator {

template<typename StateType, typename ValueType>
DistributionEntry<StateType, ValueType>::DistributionEntry() : state(0), value(0) {
    // Intentionally left empty.
}

template<typename StateType, typename ValueType>
DistributionEntry<StateType, ValueType>::DistributionEntry(StateType const& state, ValueType const& value) : state(state), value(value) {
    // Intentionally left empty.
}

template<typename StateType, typename ValueType>
StateType const& DistributionEntry<StateType, ValueType>::getState() const {
    return state;
}

template<typename StateType, typename ValueType>
ValueType const& DistributionEntry<StateType, ValueType>::getValue() const {
    return value;
}

template<typename StateType, typename ValueType>
void DistributionEntry<StateType, ValueType>::addToValue(ValueType const& value) {
    this->value += value;
}

template<typename StateType, typename ValueType>
void DistributionEntry<StateType, ValueType>::divide(ValueType const& value) {
    this->value /= value;
}

template class DistributionEntry<uint32_t, double>;
template class DistributionEntry<uint32_t, storm::RationalNumber>;
template class DistributionEntry<uint32_t, storm::RationalFunction>;
template class DistributionEntry<uint32_t, storm::Interval>;
template class DistributionEntry<uint32_t, storm::RationalInterval>;

template class DistributionEntry<storm::storage::BitVector, double>;
template class DistributionEntry<storm::storage::BitVector, storm::RationalNumber>;
template class DistributionEntry<storm::storage::BitVector, storm::RationalFunction>;
template class DistributionEntry<storm::storage::BitVector, storm::Interval>;
template class DistributionEntry<storm::storage::BitVector, storm::RationalInterval>;

}  // namespace storm::generator
