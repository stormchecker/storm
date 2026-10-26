#pragma once
#include <cstdint>

#include "storm-pomdp/beliefs/utility/types.h"

namespace storm::pomdp::beliefs {

/*!
 * Constructs a belief
 * @tparam BeliefType the type of belief to construct
 */
template<typename BeliefType>
class BeliefBuilder {
   public:
    using ValueType = typename BeliefType::ValueType;

    /*!
     * Reserves space to build the belief more efficiently
     * @param size the number of states in the support of the belief
     * @note providing this is optional, but can improve performance
     */
    void reserve(uint64_t size);

    /*!
     * Adds a probability value to the given state.
     * If a value for the state already exists, its new value is the sum of the existing one and the given value.
     */
    void addValue(BeliefStateType const& state, ValueType const& value);

    /*!
     * Sets the observation of the belief.
     * @param observation the observation to set
     */
    void setObservation(BeliefObservationType const& observation);

    /*!
     * Normalizes the belief, i.e., if the sum of the values is not 1, it divides all values by the sum.
     * Hence, after calling this, the sum of the values is 1.
     * @pre the sum is must be strictly greater than 0.
     * @return the sum of the values (before normalization)
     */
    ValueType normalize();

    /*!
     * Builds the belief and returns it.
     * @pre the belief must be a valid probability distribution, i.e., all values are in [0,1] and sum up to 1. Furthermore, an observation must have been set
     * before calling this.
     * @note After calling this, the builder is in an invalid state. Before building another belief with this builder, the reset() method must be called.
     */
    BeliefType build();

    /*!
     * Resets the builder to its initial state.
     * After build() has been called, this method must be called before building another belief with this builder.
     */
    void reset();

   private:
    bool assertBelief();

    BeliefFlatMap<ValueType> data;
    BeliefObservationType obs{InvalidObservation};
};

}  // namespace storm::pomdp::beliefs