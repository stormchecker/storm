#include "UniqueObservationStates.h"

namespace storm {
namespace analysis {

template<typename ValueType>
UniqueObservationStates<ValueType>::UniqueObservationStates(storm::models::sparse::Pomdp<ValueType> const& pomdp) : pomdp(pomdp) {}

template<typename ValueType>
storm::storage::BitVector UniqueObservationStates<ValueType>::analyse() const {
    storm::storage::BitVector seenOnce(pomdp.getNrObservations(), false);
    storm::storage::BitVector seenMoreThanOnce(pomdp.getNrObservations(), false);

    for (auto const& observation : pomdp.getObservations()) {
        if (seenOnce.get(observation)) {
            seenMoreThanOnce.set(observation);
        }
        seenOnce.set(observation);
    }

    storm::storage::BitVector uniqueObservation(pomdp.getNumberOfStates(), false);
    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        if (!seenMoreThanOnce.get(pomdp.getObservation(state))) {
            uniqueObservation.set(state);
        }
    }
    return uniqueObservation;
}

template class UniqueObservationStates<storm::RationalNumber>;

template class UniqueObservationStates<double>;
}  // namespace analysis
}  // namespace storm