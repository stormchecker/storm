#include "StateStorage.h"

namespace storm {
namespace storage {
namespace sparse {

template<typename StateType>
StateStorage<StateType>::StateStorage(uint64_t bitsPerState) : stateToId(bitsPerState, 100000), bitsPerState(bitsPerState) {
    // Intentionally left empty.
}

template<typename StateType>
uint_fast64_t StateStorage<StateType>::getNumberOfStates() const {
    return stateToId.size();
}

template struct StateStorage<uint32_t>;
template struct StateStorage<uint_fast64_t>;
}  // namespace sparse
}  // namespace storage
}  // namespace storm
