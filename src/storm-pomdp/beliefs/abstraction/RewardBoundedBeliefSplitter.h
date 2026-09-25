#pragma once

#include <string>
#include <vector>

#include "storm-pomdp/beliefs/storage/BeliefBuilder.h"
#include "storm-pomdp/beliefs/utility/types.h"
#include "storm/utility/constants.h"

namespace storm::pomdp::beliefs {

/*!
 * Abstracts a given belief by splitting it into sub-beliefs based on the (state-action) reward of the POMDP.
 * Multiple reward models can be set.
 * States in the support of the given belief where all action rewards are equal will be grouped together.
 * When applying this as a pre-abstraction, this intuitively means that the collected rewards are observable.
 */
template<typename RewardValueType, typename PomdpType, typename BeliefType>
class RewardBoundedBeliefSplitter {
   public:
    using PomdpValueType = typename PomdpType::ValueType;
    using BeliefValueType = typename BeliefType::ValueType;
    using RewardVectorType = std::vector<RewardValueType>;

    /** Creates a splitter for @p pomdp. The POMDP must outlive the splitter. */
    RewardBoundedBeliefSplitter(PomdpType const& pomdp);
    /** Selects one reward model; an empty name selects Storm's default reward model. */
    void setRewardModel(std::string const& rewardModelName = "");
    /** Selects the reward models used to form reward vectors. */
    void setRewardModels(std::vector<std::string> const& rewardModelNames);
    /** Clears the selected reward models and all generated reward-vector observation indices. */
    void unsetRewardModels();
    /** @return the number of selected reward models. */
    std::size_t getNumberOfSetRewardModels() const;

    /**
     * Splits @p belief into normalized sub-beliefs with equal reward vectors for @p localActionIndex.
     *
     * The callback receives a sub-belief, its probability mass, a stable reward-vector observation index, and the
     * reward vector itself.
     */
    template<typename ExpandCallback>
    void abstract(BeliefType const& belief, uint64_t localActionIndex, ExpandCallback const& callback) {
        // gather the occurring reward vectors and build the sub-beliefs
        std::map<RewardVectorType, BeliefBuilder<BeliefType>> splitBeliefs;
        belief.forEach([&localActionIndex, &splitBeliefs, this](BeliefStateType const& state, BeliefValueType const& beliefValue) {
            auto const globalActionIndex = pomdp.getTransitionMatrix().getRowGroupIndices()[state] + localActionIndex;
            auto const& rewVector = actionRewardVectors[globalActionIndex];
            splitBeliefs[rewVector].addValue(state, beliefValue);
        });

        for (auto& [rewardVector, builder] : splitBeliefs) {
            BeliefActionObservationType const freshIndex = rewardVectorToIndex.size();
            auto const rewVectorIndex = rewardVectorToIndex.emplace(rewardVector, freshIndex).first->second;
            builder.setObservation(belief.observation());
            if (splitBeliefs.size() == 1u) {
                // Fix the distribution to diminish numerical issues a bit
                callback(builder.build(), storm::utility::one<BeliefValueType>(), rewVectorIndex, rewardVector);
            } else {
                auto val = builder.normalize();
                callback(builder.build(), std::move(val), rewVectorIndex, rewardVector);
            }
        }
    }

   private:
    PomdpType const& pomdp;
    std::vector<RewardVectorType> actionRewardVectors;
    std::map<RewardVectorType, BeliefActionObservationType> rewardVectorToIndex;
};
}  // namespace storm::pomdp::beliefs
