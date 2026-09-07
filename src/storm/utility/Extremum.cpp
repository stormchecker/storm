#include "storm/utility/Extremum.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/macros.h"

namespace storm::utility {

template<storm::OptimizationDirection Dir, typename ValueType>
Extremum<Dir, ValueType>::Extremum(ValueType const& value) : extremalValue(value) {
    // Intentionally left empty
}

template<storm::OptimizationDirection Dir, typename ValueType>
Extremum<Dir, ValueType>::Extremum(ValueType&& value) : extremalValue(std::move(value)) {
    // Intentionally left empty
}

template<storm::OptimizationDirection Dir, typename ValueType>
Extremum<Dir, ValueType>& Extremum<Dir, ValueType>::operator=(ValueType const& value) {
    extremalValue = value;
    return *this;
}

template<storm::OptimizationDirection Dir, typename ValueType>
Extremum<Dir, ValueType>& Extremum<Dir, ValueType>::operator=(ValueType&& value) {
    extremalValue = std::move(value);
    return *this;
}

template<storm::OptimizationDirection Dir, typename ValueType>
bool Extremum<Dir, ValueType>::better(ValueType const& value) const {
    if constexpr (storm::solver::minimize(Dir)) {
        return value < extremalValue;
    } else {
        static_assert(storm::solver::maximize(Dir));
        return value > extremalValue;
    }
}

template<storm::OptimizationDirection Dir, typename ValueType>
bool Extremum<Dir, ValueType>::operator&=(Extremum const& other) {
    if (better(other.extremalValue)) {
        extremalValue = other.extremalValue;
        return true;
    }
    return false;
}

template<storm::OptimizationDirection Dir, typename ValueType>
bool Extremum<Dir, ValueType>::operator&=(Extremum&& other) {
    if (better(other.extremalValue)) {
        extremalValue = std::move(other.extremalValue);
        return true;
    }
    return false;
}

template<storm::OptimizationDirection Dir, typename ValueType>
bool Extremum<Dir, ValueType>::operator&=(ValueType const& value) {
    if (better(value)) {
        extremalValue = value;
        return true;
    }
    return false;
}

template<storm::OptimizationDirection Dir, typename ValueType>
bool Extremum<Dir, ValueType>::operator&=(ValueType&& value) {
    if (better(value)) {
        extremalValue = std::move(value);
        return true;
    }
    return false;
}

template<storm::OptimizationDirection Dir, typename ValueType>
bool Extremum<Dir, ValueType>::empty() const {
    if constexpr (StoresPlainValues) {
        return extremalValue == baseValue();
    } else {
        if constexpr (storm::solver::minimize(Dir)) {
            return extremalValue.isPositiveInfinity();
        } else {
            static_assert(storm::solver::maximize(Dir));
            return extremalValue.isNegativeInfinity();
        }
    }
}

template<storm::OptimizationDirection Dir, typename ValueType>
ValueType const& Extremum<Dir, ValueType>::operator*() const {
    STORM_LOG_ASSERT(!empty(), "Tried to get empty extremum.");
    if constexpr (StoresPlainValues) {
        return extremalValue;
    } else {
        return extremalValue.getFinite();
    }
}

template<storm::OptimizationDirection Dir, typename ValueType>
ValueType& Extremum<Dir, ValueType>::operator*() {
    STORM_LOG_ASSERT(!empty(), "Tried to get empty extremum.");
    if constexpr (StoresPlainValues) {
        return extremalValue;
    } else {
        return extremalValue.getFinite();
    }
}

template<storm::OptimizationDirection Dir, typename ValueType>
std::optional<ValueType> Extremum<Dir, ValueType>::getOptionalValue() const {
    if (empty()) {
        return {};
    }
    return **this;
}

template<storm::OptimizationDirection Dir, typename ValueType>
storm::utility::ExtendedValueType<ValueType> const& Extremum<Dir, ValueType>::getExtendedValue() const {
    return extremalValue;
}

template<storm::OptimizationDirection Dir, typename ValueType>
void Extremum<Dir, ValueType>::reset() {
    extremalValue = baseValue();
}

template class Extremum<storm::OptimizationDirection::Minimize, double>;
template class Extremum<storm::OptimizationDirection::Maximize, double>;

#if defined(STORM_HAVE_CLN)
template class Extremum<storm::OptimizationDirection::Minimize, storm::ClnRationalNumber>;
template class Extremum<storm::OptimizationDirection::Maximize, storm::ClnRationalNumber>;
#endif
#if defined(STORM_HAVE_GMP)
template class Extremum<storm::OptimizationDirection::Minimize, storm::GmpRationalNumber>;
template class Extremum<storm::OptimizationDirection::Maximize, storm::GmpRationalNumber>;
#endif

}  // namespace storm::utility