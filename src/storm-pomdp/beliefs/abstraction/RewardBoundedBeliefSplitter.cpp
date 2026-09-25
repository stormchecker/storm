#include "storm-pomdp/beliefs/abstraction/RewardBoundedBeliefSplitter.h"

#include "storm-pomdp/beliefs/storage/Belief.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/utility/macros.h"

namespace storm::pomdp::beliefs {

template<typename RewardValueType, typename PomdpType, typename BeliefType>
RewardBoundedBeliefSplitter<RewardValueType, PomdpType, BeliefType>::RewardBoundedBeliefSplitter(PomdpType const& pomdp) : pomdp(pomdp) {}

template<typename RewardValueType, typename PomdpType, typename BeliefType>
void RewardBoundedBeliefSplitter<RewardValueType, PomdpType, BeliefType>::setRewardModel(std::string const& rewardModelName) {
    setRewardModels({rewardModelName});
}

template<typename RewardValueType, typename PomdpType, typename BeliefType>
void RewardBoundedBeliefSplitter<RewardValueType, PomdpType, BeliefType>::setRewardModels(std::vector<std::string> const& rewardModelNames) {
    actionRewardVectors.assign(pomdp.getNumberOfChoices(), {});
    for (auto const& rewardModelName : rewardModelNames) {
        auto const& rewardModel = pomdp.getRewardModel(rewardModelName);
        STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotSupportedException,
                        "POMDPs with transition rewards are currently not supported.");
        for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
            for (auto const& choice : pomdp.getTransitionMatrix().getRowGroupIndices(state)) {
                auto val = rewardModel.hasStateActionRewards() ? rewardModel.getStateActionReward(choice) : storm::utility::zero<PomdpValueType>();
                if (rewardModel.hasStateRewards()) {
                    val += rewardModel.getStateReward(state);
                }
                actionRewardVectors[choice].push_back(storm::utility::convertNumber<RewardValueType>(val));
            }
        }
    }
}

template<typename RewardValueType, typename PomdpType, typename BeliefType>
void RewardBoundedBeliefSplitter<RewardValueType, PomdpType, BeliefType>::unsetRewardModels() {
    actionRewardVectors.clear();
}

template<typename RewardValueType, typename PomdpType, typename BeliefType>
std::size_t RewardBoundedBeliefSplitter<RewardValueType, PomdpType, BeliefType>::getNumberOfSetRewardModels() const {
    if (actionRewardVectors.empty()) {
        return 0;
    } else {
        return actionRewardVectors.front().size();
    }
}

template class RewardBoundedBeliefSplitter<double, storm::models::sparse::Pomdp<double>, Belief<double>>;
template class RewardBoundedBeliefSplitter<double, storm::models::sparse::Pomdp<double>, Belief<storm::RationalNumber>>;
template class RewardBoundedBeliefSplitter<double, storm::models::sparse::Pomdp<storm::RationalNumber>, Belief<double>>;
template class RewardBoundedBeliefSplitter<double, storm::models::sparse::Pomdp<storm::RationalNumber>, Belief<storm::RationalNumber>>;
template class RewardBoundedBeliefSplitter<storm::RationalNumber, storm::models::sparse::Pomdp<double>, Belief<double>>;
template class RewardBoundedBeliefSplitter<storm::RationalNumber, storm::models::sparse::Pomdp<double>, Belief<storm::RationalNumber>>;
template class RewardBoundedBeliefSplitter<storm::RationalNumber, storm::models::sparse::Pomdp<storm::RationalNumber>, Belief<double>>;
template class RewardBoundedBeliefSplitter<storm::RationalNumber, storm::models::sparse::Pomdp<storm::RationalNumber>, Belief<storm::RationalNumber>>;

}  // namespace storm::pomdp::beliefs