#pragma once

#include <map>
#include <string>
#include <vector>

#include "storm/storage/BoostTypes.h"
#include "storm/storage/prism/StateActionReward.h"
#include "storm/storage/prism/StateReward.h"
#include "storm/storage/prism/TransitionReward.h"

namespace storm {
namespace prism {
class RewardModel : public LocatedInformation {
   public:
    /*!
     * Creates a reward model with the given name, state and transition rewards.
     *
     * @param rewardModelName The name of the reward model.
     * @param stateRewards A vector of state-based rewards.
     * @param stateActionRewards A vector of state-action-based rewards.
     * @param transitionRewards A vector of transition-based rewards.
     * @param filename The filename in which the reward model is defined.
     * @param lineNumber The line number in which the reward model is defined.
     */
    RewardModel(std::string const& rewardModelName, std::vector<storm::prism::StateReward> const& stateRewards,
                std::vector<storm::prism::StateActionReward> const& stateActionRewards, std::vector<storm::prism::TransitionReward> const& transitionRewards,
                std::string const& filename = "", uint_fast64_t lineNumber = 0);

    // Create default implementations of constructors/assignment.
    RewardModel() = default;
    RewardModel(RewardModel const& other) = default;
    RewardModel& operator=(RewardModel const& other) = default;
    RewardModel(RewardModel&& other) = default;
    RewardModel& operator=(RewardModel&& other) = default;

    /*!
     * Retrieves the name of the reward model.
     *
     * @return The name of the reward model.
     */
    std::string const& getName() const;

    /*!
     * Checks whether the reward model is empty, i.e. contains no state or transition rewards.
     *
     * @return True iff the reward model is empty.
     */
    bool empty() const;

    /*!
     * Retrieves whether there are any state rewards.
     *
     * @return True iff there are any state rewards.
     */
    bool hasStateRewards() const;

    /*!
     * Retrieves all state rewards associated with this reward model.
     *
     * @return The state rewards associated with this reward model.
     */
    std::vector<storm::prism::StateReward> const& getStateRewards() const;

    /*!
     * Retrieves whether there are any state-action rewards.
     *
     * @return True iff there are any state-action rewards.
     */
    bool hasStateActionRewards() const;

    /*!
     * Retrieves all state-action rewards associated with this reward model.
     *
     * @return The state-action rewards associated with this reward model.
     */
    std::vector<storm::prism::StateActionReward> const& getStateActionRewards() const;

    /*!
     * Retrieves whether there are any transition rewards.
     *
     * @return True iff there are any transition rewards.
     */
    bool hasTransitionRewards() const;

    /*!
     * Retrieves all transition rewards associated with this reward model.
     *
     * @return The transition rewards associated with this reward model.
     */
    std::vector<storm::prism::TransitionReward> const& getTransitionRewards() const;

    /*!
     * Substitutes all variables in the reward model according to the given map.
     *
     * @param substitution The substitution to perform.
     * @return The resulting reward model.
     */
    RewardModel substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;

    RewardModel labelUnlabelledCommands(std::vector<std::pair<uint64_t, std::string>> const& newActionNames) const;

    /*!
     * Checks whether any of the given variables only appear in the expressions defining the reward value.
     *
     * @param undefinedConstantVariables A set of variables that may only appear in the expressions defining the
     * reward values.
     * @return True iff the given variables only appear in the expressions defining the reward value.
     */
    bool containsVariablesOnlyInRewardValueExpressions(std::set<storm::expressions::Variable> const& undefinedConstantVariables) const;

    /*!
     * Restricts all action-related rewards of the reward model to the ones with an action index in the provided set.
     *
     * @param actionIndicesToKeep The set of action indices to keep.
     * @return The resulting reward model.
     */
    RewardModel restrictActionRelatedRewards(storm::storage::FlatSet<uint_fast64_t> const& actionIndicesToKeep) const;

    friend std::ostream& operator<<(std::ostream& stream, RewardModel const& rewardModel);

   private:
    // The name of the reward model.
    std::string rewardModelName;

    // The state-based rewards associated with this reward model.
    std::vector<storm::prism::StateReward> stateRewards;

    // The state-action-based rewards associated with this reward model.
    std::vector<storm::prism::StateActionReward> stateActionRewards;

    // The transition-based rewards associated with this reward model.
    std::vector<storm::prism::TransitionReward> transitionRewards;
};

}  // namespace prism
}  // namespace storm
