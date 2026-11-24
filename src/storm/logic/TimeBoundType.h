#pragma once

#include <boost/optional.hpp>

#include "storm/logic/RewardAccumulation.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {

enum class TimeBoundType { Steps, Time, Reward };

class TimeBoundReference {
    TimeBoundType type;
    boost::optional<std::string> rewardName;
    boost::optional<RewardAccumulation> rewardAccumulation;

   public:
    explicit TimeBoundReference(TimeBoundType t) : type(t) {
        // For rewards, use the other constructor.
        STORM_LOG_ASSERT(t != TimeBoundType::Reward, "Unexpected Reward time bound type.");
    }

    explicit TimeBoundReference(boost::optional<std::string> const& rewardName = boost::none,
                                boost::optional<RewardAccumulation> rewardAccumulation = boost::none)
        : type(TimeBoundType::Reward), rewardName(rewardName), rewardAccumulation(rewardAccumulation) {
        STORM_LOG_ASSERT(rewardName.get_value_or("NO_REWARD_NAME_GIVEN") != "",
                         "Reward name should not be empty.");  // Empty reward name is reserved.
    }

    TimeBoundType const& getType() const {
        return type;
    }

    bool isStepBound() const {
        return type == TimeBoundType::Steps;
    }

    bool isTimeBound() const {
        return type == TimeBoundType::Time;
    }

    bool isRewardBound() const {
        return type == TimeBoundType::Reward;
    }

    std::string const& getRewardModelName() const {
        STORM_LOG_ASSERT(isRewardBound(), "Expected reward bound.");
        return rewardName.get();
    }

    std::string const& getRewardName() const {
        return getRewardModelName();
    }

    bool hasRewardModelName() const {
        return isRewardBound() && static_cast<bool>(rewardName);
    }

    boost::optional<std::string> const& getOptionalRewardModelName() const {
        STORM_LOG_ASSERT(isRewardBound(), "Expected reward bound.");
        return rewardName;
    }

    bool hasRewardAccumulation() const {
        return rewardAccumulation.is_initialized();
    }

    RewardAccumulation const& getRewardAccumulation() const {
        STORM_LOG_ASSERT(isRewardBound(), "Expected reward bound.");
        return rewardAccumulation.get();
    }

    boost::optional<RewardAccumulation> const& getOptionalRewardAccumulation() const {
        STORM_LOG_ASSERT(isRewardBound(), "Expected reward bound.");
        return rewardAccumulation;
    }
};

}  // namespace logic
}  // namespace storm
