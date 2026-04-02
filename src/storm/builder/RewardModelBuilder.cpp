#include "storm/builder/RewardModelBuilder.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/macros.h"

namespace storm {
namespace builder {

template<typename ValueType>
RewardModelBuilder<ValueType>::RewardModelBuilder(RewardModelInformation const& rewardModelInformation)
    : rewardModelName(rewardModelInformation.getName()),
      stateRewards(rewardModelInformation.hasStateRewards()),
      stateActionRewards(rewardModelInformation.hasStateActionRewards()),
      stateRewardVector(),
      stateActionRewardVector() {
    STORM_LOG_THROW(!rewardModelInformation.hasTransitionRewards(), storm::exceptions::InvalidArgumentException, "Unable to treat transition rewards.");
}

template<typename ValueType>
storm::models::sparse::StandardRewardModel<ValueType> RewardModelBuilder<ValueType>::build(uint_fast64_t rowCount, uint_fast64_t, uint_fast64_t rowGroupCount) {
    std::optional<std::vector<ValueType>> optionalStateRewardVector;
    if (hasStateRewards()) {
        stateRewardVector.resize(rowGroupCount);
        optionalStateRewardVector = std::move(stateRewardVector);
    }

    std::optional<std::vector<ValueType>> optionalStateActionRewardVector;
    if (hasStateActionRewards()) {
        stateActionRewardVector.resize(rowCount);
        optionalStateActionRewardVector = std::move(stateActionRewardVector);
    }

    return storm::models::sparse::StandardRewardModel<ValueType>(std::move(optionalStateRewardVector), std::move(optionalStateActionRewardVector));
}

template<typename ValueType>
std::string const& RewardModelBuilder<ValueType>::getName() const {
    return rewardModelName;
}

template<typename ValueType>
void RewardModelBuilder<ValueType>::addStateReward(ValueType const& value) {
    stateRewardVector.push_back(value);
}

template<typename ValueType>
void RewardModelBuilder<ValueType>::addStateActionReward(ValueType const& value) {
    stateActionRewardVector.push_back(value);
}

template<typename ValueType>
bool RewardModelBuilder<ValueType>::hasStateRewards() const {
    return stateRewards;
}

template<typename ValueType>
bool RewardModelBuilder<ValueType>::hasStateActionRewards() const {
    return stateActionRewards;
}

template class RewardModelBuilder<double>;
template class RewardModelBuilder<storm::RationalNumber>;
template class RewardModelBuilder<storm::RationalFunction>;
template class RewardModelBuilder<storm::Interval>;
template class RewardModelBuilder<storm::RationalInterval>;

}  // namespace builder
}  // namespace storm
