#include "storm-gamebased-ar/abstraction/ExplicitQuantitativeResult.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/macros.h"

namespace storm::gbar {
namespace abstraction {

template<typename ValueType>
ExplicitQuantitativeResult<ValueType>::ExplicitQuantitativeResult(uint64_t numberOfStates) : values(numberOfStates) {
    // Intentionally left empty.
}

template<typename ValueType>
ExplicitQuantitativeResult<ValueType>::ExplicitQuantitativeResult(std::vector<ValueType>&& values) : values(std::move(values)) {
    // Intentionally left empty.
}

template<typename ValueType>
std::vector<ValueType> const& ExplicitQuantitativeResult<ValueType>::getValues() const {
    return values;
}

template<typename ValueType>
std::vector<ValueType>& ExplicitQuantitativeResult<ValueType>::getValues() {
    return values;
}

template<typename ValueType>
void ExplicitQuantitativeResult<ValueType>::setValue(uint64_t state, ValueType const& value) {
    values[state] = value;
}

template<typename ValueType>
std::pair<ValueType, ValueType> ExplicitQuantitativeResult<ValueType>::getRange(storm::storage::BitVector const& states) const {
    STORM_LOG_THROW(!states.empty(), storm::exceptions::InvalidArgumentException, "Expected non-empty set of states.");

    auto stateIt = states.begin();
    std::pair<ValueType, ValueType> result = std::make_pair(values[*stateIt], values[*stateIt]);
    ++stateIt;

    while (stateIt != states.end()) {
        if (values[*stateIt] < result.first) {
            result.first = values[*stateIt];
        } else if (values[*stateIt] < result.first) {
            result.second = values[*stateIt];
        }

        ++stateIt;
    }

    return result;
}

template class ExplicitQuantitativeResult<double>;
template class ExplicitQuantitativeResult<storm::RationalNumber>;
}  // namespace abstraction
}  // namespace storm::gbar
