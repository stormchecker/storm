#include "storm-pomdp/generator/BeliefSupportTracker.h"

#include "storm/adapters/RationalNumberAdapter.h"

namespace storm {
namespace generator {
template<typename ValueType>
BeliefSupportTracker<ValueType>::BeliefSupportTracker(storm::models::sparse::Pomdp<ValueType> const& pomdp)
    : pomdp(pomdp), currentBeliefSupport(pomdp.getInitialStates()) {}

template<typename ValueType>
storm::storage::BitVector const& BeliefSupportTracker<ValueType>::getCurrentBeliefSupport() const {
    return currentBeliefSupport;
}

template<typename ValueType>
void BeliefSupportTracker<ValueType>::track(uint64_t action, uint64_t observation) {
    storm::storage::BitVector newBeliefSupport(pomdp.getNumberOfStates());
    for (uint64_t oldState : currentBeliefSupport) {
        uint64_t row = pomdp.getTransitionMatrix().getRowGroupIndices()[oldState] + action;
        for (auto const& successor : pomdp.getTransitionMatrix().getRow(row)) {
            STORM_LOG_ASSERT(!storm::utility::isZero(successor.getValue()), "Expected non-zero successor probability.");
            if (pomdp.getObservation(successor.getColumn()) == observation) {
                newBeliefSupport.set(successor.getColumn(), true);
            }
        }
    }
    currentBeliefSupport = newBeliefSupport;
}

template<typename ValueType>
void BeliefSupportTracker<ValueType>::reset() {
    currentBeliefSupport = pomdp.getInitialStates();
}

template class BeliefSupportTracker<double>;
template class BeliefSupportTracker<storm::RationalNumber>;

}  // namespace generator
}  // namespace storm