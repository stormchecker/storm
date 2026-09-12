#pragma once

#include <set>
#include <string>

namespace storm::bisimulation {
struct PreservationInformation {
    std::set<std::string> preservedStateLabels, preservedChoiceLabels, preservedRewardModels;
};
}  // namespace storm::bisimulation