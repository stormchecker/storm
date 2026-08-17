#include "BeliefExplorationBounds.h"

#include "storm/adapters/RationalNumberAdapter.h"

namespace storm {
namespace pomdp {
namespace storage {

template<typename ValueType>
ValueType PreprocessingPomdpValueBounds<ValueType>::getLowerBound(uint64_t scheduler_id, uint64_t const& state) {
    STORM_LOG_ASSERT(!lower.empty(), "Requested a lower bound but none were available.");
    return lower[scheduler_id][state];
}

template<typename ValueType>
ValueType PreprocessingPomdpValueBounds<ValueType>::getUpperBound(uint64_t scheduler_id, uint64_t const& state) {
    STORM_LOG_ASSERT(!upper.empty(), "Requested an upper bound but none were available.");
    return upper[scheduler_id][state];
}

template<typename ValueType>
ValueType PreprocessingPomdpValueBounds<ValueType>::getHighestLowerBound(uint64_t const& state) {
    STORM_LOG_ASSERT(!lower.empty(), "Requested a lower bound but none were available.");
    auto it = lower.begin();
    ValueType result = (*it)[state];
    for (++it; it != lower.end(); ++it) {
        result = std::max(result, (*it)[state]);
    }
    return result;
}

template<typename ValueType>
ValueType PreprocessingPomdpValueBounds<ValueType>::getSmallestUpperBound(uint64_t const& state) {
    STORM_LOG_ASSERT(!upper.empty(), "Requested an upper bound but none were available.");
    auto it = upper.begin();
    ValueType result = (*it)[state];
    for (++it; it != upper.end(); ++it) {
        result = std::min(result, (*it)[state]);
    }
    return result;
}

template<typename ValueType>
ValueType ExtremePOMDPValueBound<ValueType>::getValueForState(uint64_t const& state) {
    STORM_LOG_ASSERT(!values.empty(), "Requested an extreme bound but none were available.");
    return values[state];
}

template struct PreprocessingPomdpValueBounds<double>;
template struct PreprocessingPomdpValueBounds<storm::RationalNumber>;

template struct ExtremePOMDPValueBound<double>;
template struct ExtremePOMDPValueBound<storm::RationalNumber>;
}  // namespace storage
}  // namespace pomdp
}  // namespace storm