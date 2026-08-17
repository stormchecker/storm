#pragma once

#include "storm/storage/Distribution.h"

#include "storm/utility/ConstantsComparator.h"
#include "storm/utility/constants.h"

namespace storm {

namespace storage {

template<typename ValueType, typename StateType = uint32_t>
class DistributionWithReward : public Distribution<ValueType, StateType> {
   public:
    /*!
     * Creates an empty distribution.
     */
    DistributionWithReward(ValueType const& reward = storm::utility::zero<ValueType>());

    DistributionWithReward(DistributionWithReward const& other) = default;
    DistributionWithReward& operator=(DistributionWithReward const& other) = default;
    DistributionWithReward(DistributionWithReward&& other) = default;
    DistributionWithReward& operator=(DistributionWithReward&& other) = default;

    /*!
     * Checks whether the two distributions specify the same probabilities to go to the same states.
     *
     * @param other The distribution with which the current distribution is to be compared.
     * @return True iff the two distributions are equal.
     */
    bool equals(DistributionWithReward<ValueType, StateType> const& other, storm::utility::ConstantsComparator<ValueType> const& comparator) const;

    bool less(DistributionWithReward<ValueType, StateType> const& other, storm::utility::ConstantsComparator<ValueType> const& comparator) const;

    /*!
     * Sets the reward of this distribution.
     */
    void setReward(ValueType const& reward);

    /*!
     * Retrieves the reward of this distribution.
     */
    ValueType const& getReward() const;

   private:
    ValueType reward;
};

}  // namespace storage
}  // namespace storm
