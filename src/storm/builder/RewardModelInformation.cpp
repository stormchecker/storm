#include "RewardModelInformation.h"

namespace storm {
namespace builder {

RewardModelInformation::RewardModelInformation(std::string const& name, bool stateRewards, bool stateActionRewards, bool transitionRewards)
    : name(name), stateRewards(stateRewards), stateActionRewards(stateActionRewards), transitionRewards(transitionRewards) {
    // Intentionally left empty.
}

std::string const& RewardModelInformation::getName() const {
    return name;
}

bool RewardModelInformation::hasStateRewards() const {
    return stateRewards;
}

bool RewardModelInformation::hasStateActionRewards() const {
    return stateActionRewards;
}

bool RewardModelInformation::hasTransitionRewards() const {
    return transitionRewards;
}

void RewardModelInformation::setHasStateRewards() {
    stateRewards = true;
}

void RewardModelInformation::setHasStateActionRewards() {
    stateActionRewards = true;
}

void RewardModelInformation::setHasTransitionRewards() {
    transitionRewards = true;
}

}  // namespace builder
}  // namespace storm
