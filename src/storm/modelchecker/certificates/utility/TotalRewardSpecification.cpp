#include "storm/modelchecker/certificates/utility/TotalRewardSpecification.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::modelchecker {

template<typename ValueType>
TerminalStateValue<ValueType>::TerminalStateValue(ValueType value) : TerminalStateValue(value, false) {
    // Intentionally left empty.
}

template<typename ValueType>
TerminalStateValue<ValueType>::TerminalStateValue(ValueType value, bool infinite) : value(value), infinite(infinite) {  // TODO: handle non-finite floats
    // Intentionally left empty.
}

template<typename ValueType>
TerminalStateValue<ValueType> TerminalStateValue<ValueType>::getPlusInfinity() {
    return TerminalStateValue(storm::utility::one<ValueType>(), true);
}

template<typename ValueType>
TerminalStateValue<ValueType> TerminalStateValue<ValueType>::getMinusInfinity() {
    return TerminalStateValue(-storm::utility::one<ValueType>(), true);
}

template<typename ValueType>
TerminalStateValue<ValueType> TerminalStateValue<ValueType>::getOne() {
    return TerminalStateValue(storm::utility::one<ValueType>());
}

template<typename ValueType>
bool TerminalStateValue<ValueType>::isFinite() const {
    return !infinite;
}

template<typename ValueType>
bool TerminalStateValue<ValueType>::isOne() const {
    return isFinite() && storm::utility::isOne<ValueType>(value);
}

template<typename ValueType>
bool TerminalStateValue<ValueType>::isPositiveInfinity() const {
    return infinite && value > storm::utility::zero<ValueType>();
}

template<typename ValueType>
bool TerminalStateValue<ValueType>::isNegativeInfinity() const {
    return infinite && value <= storm::utility::zero<ValueType>();
}

template<typename ValueType>
ValueType TerminalStateValue<ValueType>::getFiniteValue() const {
    STORM_LOG_ASSERT(isFinite(), "Cannot get finite value from infinite terminal state value.");
    return value;
}

template class TerminalStateValue<double>;
template class TerminalStateValue<storm::RationalNumber>;

}  // namespace storm::modelchecker
