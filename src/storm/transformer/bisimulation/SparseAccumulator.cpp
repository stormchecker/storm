#include "storm/transformer/bisimulation/SparseAccumulator.h"

#include <utility>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::bisimulation {

template<typename ValueType>
SparseAccumulator<ValueType>::SparseAccumulator(uint64_t const numStates) : values(numStates, defaultValue()) {}

template<typename ValueType>
std::vector<ValueType> const& SparseAccumulator<ValueType>::getValues() const {
    return values;
}

template<typename ValueType>
std::vector<uint64_t> const& SparseAccumulator<ValueType>::getNonDefaultStates() const {
    return nonDefaultStates;
}

template<typename ValueType>
void SparseAccumulator<ValueType>::addValue(uint64_t const state, AddedValueType value) {
    if constexpr (std::is_same_v<ValueType, std::set<uint64_t>>) {
        if (values[state].empty()) {
            nonDefaultStates.push_back(state);
        }
        values[state].insert(value);
    } else {
        STORM_LOG_ASSERT(!storm::utility::isZero(value), "Did not expect adding 0 probability");
        if (storm::utility::isZero(values[state])) {
            nonDefaultStates.push_back(state);
            values[state] = std::move(value);
        } else {
            values[state] += std::move(value);
        }
    }
}

template<typename ValueType>
void SparseAccumulator<ValueType>::clear() {
    for (auto const& state : nonDefaultStates) {
        values[state] = defaultValue();
    }
    nonDefaultStates.clear();
}

template<typename ValueType>
ValueType SparseAccumulator<ValueType>::defaultValue() {
    if constexpr (std::is_same_v<ValueType, std::set<uint64_t>>) {
        return {};  // empty set
    } else {
        return storm::utility::zero<ValueType>();
    }
}

template class SparseAccumulator<double>;
template class SparseAccumulator<storm::RationalNumber>;
template class SparseAccumulator<storm::RationalFunction>;
template class SparseAccumulator<std::set<uint64_t>>;

}  // namespace storm::bisimulation
