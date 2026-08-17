#include "storm/storage/prism/RewardModel.h"

#include "storm/utility/macros.h"

namespace storm {
namespace prism {
RewardModel::RewardModel(std::string const& rewardModelName, std::vector<storm::prism::StateReward> const& stateRewards,
                         std::vector<storm::prism::StateActionReward> const& stateActionRewards,
                         std::vector<storm::prism::TransitionReward> const& transitionRewards, std::string const& filename, uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber),
      rewardModelName(rewardModelName),
      stateRewards(stateRewards),
      stateActionRewards(stateActionRewards),
      transitionRewards(transitionRewards) {
    // Nothing to do here.
}

std::string const& RewardModel::getName() const {
    return this->rewardModelName;
}

bool RewardModel::empty() const {
    return !this->hasStateRewards() && !this->hasTransitionRewards();
}

bool RewardModel::hasStateRewards() const {
    return !this->stateRewards.empty();
}

std::vector<storm::prism::StateReward> const& RewardModel::getStateRewards() const {
    return this->stateRewards;
}

bool RewardModel::hasStateActionRewards() const {
    return !this->stateActionRewards.empty();
}

std::vector<storm::prism::StateActionReward> const& RewardModel::getStateActionRewards() const {
    return this->stateActionRewards;
}

bool RewardModel::hasTransitionRewards() const {
    return !this->transitionRewards.empty();
}

std::vector<storm::prism::TransitionReward> const& RewardModel::getTransitionRewards() const {
    return this->transitionRewards;
}

RewardModel RewardModel::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    std::vector<StateReward> newStateRewards;
    newStateRewards.reserve(this->getStateRewards().size());
    for (auto const& stateReward : this->getStateRewards()) {
        newStateRewards.emplace_back(stateReward.substitute(substitution));
    }

    std::vector<StateActionReward> newStateActionRewards;
    newStateActionRewards.reserve(this->getStateRewards().size());
    for (auto const& stateActionReward : this->getStateActionRewards()) {
        newStateActionRewards.emplace_back(stateActionReward.substitute(substitution));
    }

    std::vector<TransitionReward> newTransitionRewards;
    newTransitionRewards.reserve(this->getTransitionRewards().size());
    for (auto const& transitionReward : this->getTransitionRewards()) {
        newTransitionRewards.emplace_back(transitionReward.substitute(substitution));
    }
    return RewardModel(this->getName(), newStateRewards, newStateActionRewards, newTransitionRewards, this->getFilename(), this->getLineNumber());
}

bool RewardModel::containsVariablesOnlyInRewardValueExpressions(std::set<storm::expressions::Variable> const& undefinedConstantVariables) const {
    for (auto const& stateReward : this->getStateRewards()) {
        if (stateReward.getStatePredicateExpression().containsVariable(undefinedConstantVariables)) {
            return false;
        }
    }
    for (auto const& stateActionReward : this->getStateActionRewards()) {
        if (stateActionReward.getStatePredicateExpression().containsVariable(undefinedConstantVariables)) {
            return false;
        }
    }
    for (auto const& transitionReward : this->getTransitionRewards()) {
        if (transitionReward.getSourceStatePredicateExpression().containsVariable(undefinedConstantVariables)) {
            return false;
        }
        if (transitionReward.getTargetStatePredicateExpression().containsVariable(undefinedConstantVariables)) {
            return false;
        }
    }
    return true;
}

RewardModel RewardModel::restrictActionRelatedRewards(storm::storage::FlatSet<uint_fast64_t> const& actionIndicesToKeep) const {
    std::vector<StateActionReward> newStateActionRewards;
    for (auto const& stateActionReward : this->getStateActionRewards()) {
        if (actionIndicesToKeep.find(stateActionReward.getActionIndex()) != actionIndicesToKeep.end()) {
            newStateActionRewards.emplace_back(stateActionReward);
        }
    }

    std::vector<TransitionReward> newTransitionRewards;
    for (auto const& transitionReward : this->getTransitionRewards()) {
        if (actionIndicesToKeep.find(transitionReward.getActionIndex()) != actionIndicesToKeep.end()) {
            newTransitionRewards.emplace_back(transitionReward);
        }
    }

    return RewardModel(this->getName(), this->getStateRewards(), newStateActionRewards, newTransitionRewards, this->getFilename(), this->getLineNumber());
}

RewardModel RewardModel::labelUnlabelledCommands(std::vector<std::pair<uint64_t, std::string>> const& newActions) const {
    std::vector<StateActionReward> newStateActionRewards;
    std::vector<TransitionReward> newTransitionRewards;

    for (auto const& reward : getStateActionRewards()) {
        if (reward.getActionIndex() == 0) {
            for (auto const& newAction : newActions) {
                newStateActionRewards.emplace_back(newAction.first, newAction.second, reward.getStatePredicateExpression(), reward.getRewardValueExpression(),
                                                   reward.getFilename(), reward.getLineNumber());
            }
        } else {
            newStateActionRewards.push_back(reward);
        }
    }

    STORM_LOG_ASSERT(transitionRewards.empty(), "Expected empty transition rewards.");  // Not implemented.

    return RewardModel(this->getName(), this->getStateRewards(), newStateActionRewards, newTransitionRewards, this->getFilename(), this->getLineNumber());
}

std::ostream& operator<<(std::ostream& stream, RewardModel const& rewardModel) {
    stream << "rewards";
    if (rewardModel.getName() != "") {
        stream << " \"" << rewardModel.getName() << "\"";
    }
    stream << '\n';
    for (auto const& reward : rewardModel.getStateRewards()) {
        stream << reward << '\n';
    }
    for (auto const& reward : rewardModel.getStateActionRewards()) {
        stream << reward << '\n';
    }
    for (auto const& reward : rewardModel.getTransitionRewards()) {
        stream << reward << '\n';
    }
    stream << "endrewards\n";
    return stream;
}

}  // namespace prism
}  // namespace storm
