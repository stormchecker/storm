#pragma once

#include <string>
#include <vector>

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/modelchecker/cvar/CvarFormulaInformation.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/constants.h"
#include "storm/utility/logging.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {

template<typename ValueType>
struct WeightedReachabilityModelInformation {
    std::string rewardModelName;
    storm::storage::BitVector targetStates;
    std::vector<ValueType> terminalRewards;
};

template<typename SparseMdpModelType>
WeightedReachabilityModelInformation<typename SparseMdpModelType::ValueType> extractWeightedReachabilityModelInformation(
    SparseMdpModelType const& model, CvarFormulaInformation const& formulaInformation, storm::storage::BitVector const& targetStates) {
    using ValueType = typename SparseMdpModelType::ValueType;

    std::string rewardModelName = formulaInformation.rewardModelName ? formulaInformation.rewardModelName.get() : "";
    auto const& rewardModel = model.getRewardModel(rewardModelName);
    if (rewardModelName.empty()) {
        rewardModelName = model.getUniqueRewardModelName();
    }

    STORM_LOG_THROW(rewardModel.hasOnlyStateRewards(), storm::exceptions::InvalidPropertyException,
                    "CVaR queries currently only support weighted reachability with state-based terminal rewards.");

    std::vector<ValueType> const& stateRewards = rewardModel.getStateRewardVector();
    bool hasNonZeroTargetReward = false;
    for (uint64_t state = 0; state < stateRewards.size(); ++state) {
        if (targetStates[state]) {
            hasNonZeroTargetReward |= !storm::utility::isZero(stateRewards[state]);
        } else {
            STORM_LOG_THROW(storm::utility::isZero(stateRewards[state]), storm::exceptions::InvalidPropertyException,
                            "CVaR queries currently require terminal rewards, i.e. non-target states must have reward 0.");
        }
    }

    if (!hasNonZeroTargetReward) {
        STORM_LOG_WARN("All target states have terminal reward 0 in reward model '" << rewardModelName << "'.");
    }

    return {rewardModelName, targetStates, stateRewards};
}

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
