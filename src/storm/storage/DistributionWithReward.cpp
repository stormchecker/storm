#include "storm/storage/DistributionWithReward.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/utility/ConstantsComparator.h"

namespace storm {
namespace storage {

template<typename ValueType, typename StateType>
DistributionWithReward<ValueType, StateType>::DistributionWithReward(ValueType const& reward) : Distribution<ValueType, StateType>(), reward(reward) {
    // Intentionally left empty.
}

template<typename ValueType, typename StateType>
bool DistributionWithReward<ValueType, StateType>::equals(DistributionWithReward<ValueType, StateType> const& other,
                                                          storm::utility::ConstantsComparator<ValueType> const& comparator) const {
    if (this->reward != other.reward) {
        return false;
    }
    return Distribution<ValueType, StateType>::equals(other, comparator);
}

template<typename ValueType, typename StateType>
bool DistributionWithReward<ValueType, StateType>::less(DistributionWithReward<ValueType, StateType> const& other,
                                                        storm::utility::ConstantsComparator<ValueType> const& comparator) const {
    if (comparator.isLess(this->reward, other.reward)) {
        return true;
    } else if (comparator.isLess(other.reward, this->reward)) {
        return false;
    } else {
        return Distribution<ValueType, StateType>::less(other, comparator);
    }
}

template<typename ValueType, typename StateType>
void DistributionWithReward<ValueType, StateType>::setReward(ValueType const& reward) {
    this->reward = reward;
}

template<typename ValueType, typename StateType>
ValueType const& DistributionWithReward<ValueType, StateType>::getReward() const {
    return reward;
}

template class DistributionWithReward<double>;

template class DistributionWithReward<storm::RationalNumber>;
template class DistributionWithReward<storm::RationalFunction>;

}  // namespace storage
}  // namespace storm
