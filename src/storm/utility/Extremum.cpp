#include "storm/utility/Extremum.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/macros.h"

namespace storm::utility {

template<storm::OptimizationDirection Dir, typename ValueType>
Extremum<Dir, ValueType>::Extremum(ValueType const& value) : data({value}) {
    if constexpr (!SupportsInfinity) {
        data.empty = false;
    }
}

template<storm::OptimizationDirection Dir, typename ValueType>
Extremum<Dir, ValueType>::Extremum(ValueType&& value) : data({std::move(value)}) {
    if constexpr (!SupportsInfinity) {
        data.empty = false;
    }
}

template<storm::OptimizationDirection Dir, typename ValueType>
Extremum<Dir, ValueType>& Extremum<Dir, ValueType>::operator=(ValueType const& value) {
    data.value = value;
    if constexpr (!SupportsInfinity) {
        data.empty = false;
    }
    return *this;
}

template<storm::OptimizationDirection Dir, typename ValueType>
Extremum<Dir, ValueType>& Extremum<Dir, ValueType>::operator=(ValueType&& value) {
    data.value = std::move(value);
    if constexpr (!SupportsInfinity) {
        data.empty = false;
    }
    return *this;
}

template<storm::OptimizationDirection Dir, typename ValueType>
bool Extremum<Dir, ValueType>::better(ValueType const& value) const {
    if constexpr (!SupportsInfinity) {
        if (data.empty) {
            return true;
        }
    }
    if constexpr (storm::solver::minimize(Dir)) {
        return value < data.value;
    } else {
        static_assert(storm::solver::maximize(Dir));
        return value > data.value;
    }
}

template<storm::OptimizationDirection Dir, typename ValueType>
bool Extremum<Dir, ValueType>::operator&=(Extremum const& other) {
    if (other.empty()) {
        return false;
    }
    return *this &= *other;
}

template<storm::OptimizationDirection Dir, typename ValueType>
bool Extremum<Dir, ValueType>::operator&=(Extremum&& other) {
    if (other.empty()) {
        return false;
    }
    return *this &= std::move(*other);
}

template<storm::OptimizationDirection Dir, typename ValueType>
bool Extremum<Dir, ValueType>::operator&=(ValueType const& value) {
    if (better(value)) {
        data.value = value;
        if constexpr (!SupportsInfinity) {
            data.empty = false;
        }
        return true;
    }
    return false;
}

template<storm::OptimizationDirection Dir, typename ValueType>
bool Extremum<Dir, ValueType>::operator&=(ValueType&& value) {
    if (better(value)) {
        data.value = std::move(value);
        if constexpr (!SupportsInfinity) {
            data.empty = false;
        }
        return true;
    }
    return false;
}

template<storm::OptimizationDirection Dir, typename ValueType>
bool Extremum<Dir, ValueType>::empty() const {
    if constexpr (SupportsInfinity) {
        return data.value == data.baseValue();
    } else {
        return data.empty;
    }
}

template<storm::OptimizationDirection Dir, typename ValueType>
ValueType const& Extremum<Dir, ValueType>::operator*() const {
    STORM_LOG_ASSERT(!empty(), "Tried to get empty extremum.");
    return data.value;
}

template<storm::OptimizationDirection Dir, typename ValueType>
ValueType& Extremum<Dir, ValueType>::operator*() {
    STORM_LOG_ASSERT(!empty(), "Tried to get empty extremum.");
    return data.value;
}

template<storm::OptimizationDirection Dir, typename ValueType>
std::optional<ValueType> Extremum<Dir, ValueType>::getOptionalValue() const {
    if (empty()) {
        return {};
    } else {
        return data.value;
    }
}

template<storm::OptimizationDirection Dir, typename ValueType>
void Extremum<Dir, ValueType>::reset() {
    if constexpr (SupportsInfinity) {
        data.value = data.baseValue();
    } else {
        data.empty = true;
    }
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