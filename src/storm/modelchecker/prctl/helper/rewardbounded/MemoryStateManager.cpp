#include "storm/modelchecker/prctl/helper/rewardbounded/MemoryStateManager.h"

#include "storm/utility/macros.h"

#include "storm/exceptions/IllegalArgumentException.h"

namespace storm {
namespace modelchecker {
namespace helper {
namespace rewardbounded {

MemoryStateManager::MemoryStateManager(uint64_t dimensionCount)
    : dimensionCount(dimensionCount),
      dimensionBitMask(1ull),
      relevantBitsMask((1ull << dimensionCount) - 1),
      stateCount(dimensionBitMask << dimensionCount),
      dimensionsWithoutMemoryMask(0),
      upperMemoryStateBound(stateCount) {
    STORM_LOG_THROW(dimensionCount > 0, storm::exceptions::IllegalArgumentException, "Invoked MemoryStateManager with zero dimension count.");
    STORM_LOG_THROW(dimensionCount <= 64, storm::exceptions::IllegalArgumentException, "Invoked MemoryStateManager with too many dimensions.");
}

uint64_t const& MemoryStateManager::getDimensionCount() const {
    return dimensionCount;
}

uint64_t const& MemoryStateManager::getMemoryStateCount() const {
    return stateCount;
}

MemoryStateManager::MemoryState const& MemoryStateManager::getUpperMemoryStateBound() const {
    return upperMemoryStateBound;
}

void MemoryStateManager::setDimensionWithoutMemory(uint64_t dimension) {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked MemoryStateManager with zero dimension count.");
    STORM_LOG_ASSERT(dimension < dimensionCount, "Tried to set a dimension that is larger then the number of considered dimensions.");
    if (((dimensionBitMask << dimension) & dimensionsWithoutMemoryMask) == 0) {
        stateCount = (stateCount >> 1);
    }
    dimensionsWithoutMemoryMask |= (dimensionBitMask << dimension);
}

MemoryStateManager::MemoryState MemoryStateManager::getInitialMemoryState() const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked MemoryStateManager with zero dimension count.");
    return relevantBitsMask;
}

bool MemoryStateManager::isRelevantDimension(MemoryState const& state, uint64_t dimension) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked MemoryStateManager with zero dimension count.");
    STORM_LOG_ASSERT((state & dimensionsWithoutMemoryMask) == dimensionsWithoutMemoryMask, "Invalid memory state found.");
    return (state & (dimensionBitMask << dimension)) != 0;
}

void MemoryStateManager::setRelevantDimension(MemoryState& state, uint64_t dimension, bool value) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked MemoryStateManager with zero dimension count.");
    STORM_LOG_ASSERT(dimension < dimensionCount, "Tried to set a dimension that is larger then the number of considered dimensions.");
    STORM_LOG_ASSERT(((dimensionBitMask << dimension) & dimensionsWithoutMemoryMask) == 0,
                     "Tried to change a memory state for a dimension but the dimension is assumed to have no memory.");
    if (value) {
        state |= (dimensionBitMask << dimension);
    } else {
        state &= ~(dimensionBitMask << dimension);
    }
}

void MemoryStateManager::setRelevantDimensions(MemoryState& state, storm::storage::BitVector const& dimensions, bool value) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked MemoryStateManager with zero dimension count.");
    STORM_LOG_ASSERT(dimensions.size() == dimensionCount, "Invalid size of given bitset.");
    if (value) {
        for (uint64_t d : dimensions) {
            STORM_LOG_ASSERT(((dimensionBitMask << d) & dimensionsWithoutMemoryMask) == 0,
                             "Tried to set a dimension to 'relevant'-memory state but the dimension is assumed to have no memory.");
            state |= (dimensionBitMask << d);
        }
    } else {
        for (uint64_t d : dimensions) {
            STORM_LOG_ASSERT(((dimensionBitMask << d) & dimensionsWithoutMemoryMask) == 0,
                             "Tried to set a dimension to 'unrelevant'-memory state but the dimension is assumed to have no memory.");
            state &= ~(dimensionBitMask << d);
        }
    }
}

std::string MemoryStateManager::toString(MemoryState const& state) const {
    STORM_LOG_ASSERT(dimensionCount > 0, "Invoked EpochManager with zero dimension count.");
    std::string res = "[";
    res += (isRelevantDimension(state, 0) ? "1" : "0");
    for (uint64_t d = 1; d < dimensionCount; ++d) {
        res += ", ";
        res += (isRelevantDimension(state, d) ? "1" : "0");
    }
    res += "]";
    return res;
}
}  // namespace rewardbounded
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm