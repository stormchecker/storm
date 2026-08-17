#pragma once

#include "storm/models/sparse/Pomdp.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace generator {
template<typename ValueType>
class BeliefSupportTracker {
    /**
     * Tracks the current belief support.
     * TODO refactor into tracker and generator.
     * @param pomdp
     */
   public:
    BeliefSupportTracker(storm::models::sparse::Pomdp<ValueType> const& pomdp);
    /**
     * The current belief support according to the tracker
     * @return
     */
    storm::storage::BitVector const& getCurrentBeliefSupport() const;
    /*!
     * Update current belief support state
     * @param action The action that was taken
     * @param observation The new (state) observation
     */
    void track(uint64_t action, uint64_t observation);
    /*!
     * Reset to initial state
     */
    void reset();

   private:
    storm::models::sparse::Pomdp<ValueType> const& pomdp;
    storm::storage::BitVector currentBeliefSupport;
};
}  // namespace generator
}  // namespace storm