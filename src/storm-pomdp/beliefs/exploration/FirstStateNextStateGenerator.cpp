#include "storm-pomdp/beliefs/exploration/FirstStateNextStateGenerator.h"
#include "storm-pomdp/beliefs/storage/Belief.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/utility/macros.h"

namespace storm::pomdp::beliefs {

template<typename PomdpType, typename BeliefType>
FirstStateNextStateGenerator<PomdpType, BeliefType>::FirstStateNextStateGenerator(PomdpType const& pomdp) : pomdp(pomdp) {
    // Intentionally left empty.
}

template<typename PomdpType, typename BeliefType>
void FirstStateNextStateGenerator<PomdpType, BeliefType>::setRewardModel(std::string const& rewardModelName) {
    auto const& rewardModel = pomdp.getRewardModel(rewardModelName);
    actionRewards = rewardModel.getTotalRewardVector(pomdp.getTransitionMatrix());
}

template<typename PomdpType, typename BeliefType>
bool FirstStateNextStateGenerator<PomdpType, BeliefType>::hasRewardModel() const {
    return !actionRewards.empty();
}

template<typename PomdpType, typename BeliefType>
void FirstStateNextStateGenerator<PomdpType, BeliefType>::unsetRewardModel() {
    actionRewards.clear();
}

template<typename PomdpType, typename BeliefType>
BeliefType FirstStateNextStateGenerator<PomdpType, BeliefType>::computeInitialBelief() const {
    STORM_LOG_ASSERT(pomdp.getInitialStates().getNumberOfSetBits() == 1, "Only a single initial state is supported, but the given POMDP contains "
                                                                             << pomdp.getInitialStates().getNumberOfSetBits() << " initial states.");
    BeliefStateType const init = *pomdp.getInitialStates().begin();
    BeliefBuilder<BeliefType> builder;
    builder.addValue(init, storm::utility::one<typename BeliefType::ValueType>());
    builder.setObservation(pomdp.getObservation(init));
    return builder.build();
}

template<typename PomdpType, typename BeliefType>
uint64_t FirstStateNextStateGenerator<PomdpType, BeliefType>::getBeliefNumberOfActions(BeliefType const& belief) const {
    auto result = pomdp.getTransitionMatrix().getRowGroupSize(belief.representativeState());
    // Assert consistency with other states in the support
    STORM_LOG_ASSERT(belief.allOf([&result, this](BeliefStateType const& state, typename BeliefType::ValueType const&) {
        return pomdp.getTransitionMatrix().getRowGroupSize(state) == result;
    }),
                     "Belief considers states with inconsistent number of choices.");
    return result;
}

template<typename PomdpType, typename BeliefType>
typename PomdpType::ValueType FirstStateNextStateGenerator<PomdpType, BeliefType>::getBeliefActionReward(BeliefType const& belief,
                                                                                                         uint64_t const& localActionIndex) const {
    using PomdpValueType = typename PomdpType::ValueType;
    using BeliefValueType = typename BeliefType::ValueType;

    STORM_LOG_ASSERT(hasRewardModel(), "Requested a reward although no reward model was specified.");
    STORM_LOG_ASSERT(localActionIndex < getBeliefNumberOfActions(belief), "Invalid action index " << localActionIndex << ".");
    auto result = storm::utility::zero<PomdpValueType>();
    auto const& actionIndices = pomdp.getTransitionMatrix().getRowGroupIndices();
    belief.forEach([&localActionIndex, &actionIndices, &result, this](BeliefStateType const& state, BeliefValueType const& val) {
        uint64_t const actionIndex = actionIndices[state] + localActionIndex;
        result += storm::utility::convertNumber<PomdpValueType>(val) * this->actionRewards[actionIndex];
    });
    return result;
}

template<typename PomdpType, typename BeliefType>
std::set<std::string> FirstStateNextStateGenerator<PomdpType, BeliefType>::getBeliefActionChoiceLabels(BeliefType const& belief,
                                                                                                       uint64_t const& localActionIndex) const {
    STORM_LOG_ASSERT(pomdp.hasChoiceLabeling(), "Requested a choice label although no choice labeling was specified in the POMDP.");
    STORM_LOG_ASSERT(localActionIndex < getBeliefNumberOfActions(belief), "Invalid action index " << localActionIndex << ".");
    return pomdp.getChoiceLabeling().getLabelsOfChoice(pomdp.getTransitionMatrix().getRowGroupIndices().at(belief.representativeState()) + localActionIndex);
}

template class FirstStateNextStateGenerator<storm::models::sparse::Pomdp<double>, Belief<double>>;
template class FirstStateNextStateGenerator<storm::models::sparse::Pomdp<storm::RationalNumber>, Belief<double>>;
template class FirstStateNextStateGenerator<storm::models::sparse::Pomdp<double>, Belief<storm::RationalNumber>>;
template class FirstStateNextStateGenerator<storm::models::sparse::Pomdp<storm::RationalNumber>, Belief<storm::RationalNumber>>;

}  // namespace storm::pomdp::beliefs