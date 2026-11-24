#pragma once

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/modelchecker/cvar/CvarMethod.h"
#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {
/*!
 * Selects the concrete CVaR backend induced by a query on a given
 * model and reward structure.
 *
 * Weighted reachability is the currently implemented LP-based terminal-reward
 * setting. SSP uses Pareto value iteration for accumulated costs until
 * reaching the goal.
 */
enum class CvarBackendKind { WeightedReachability, Ssp };

/*!
 * Selects the concrete CVaR backend to use.
 *
 * The selection can be overridden explicitly via the CVaR method setting.
 * Otherwise, classification stays conservative: state-action reward models are
 * routed to the SSP branch, while state-only reward models remain on the
 * weighted-reachability path until SSP preprocessing is introduced.
 */
template<typename SparseMdpModelType>
CvarBackendKind selectCvarBackend(SparseMdpModelType const& model, CvarQueryInformation const& queryInformation, storm::storage::BitVector const&,
                                  CvarMethod method) {
    std::string rewardModelName = queryInformation.rewardModelName.value_or("");
    auto const& rewardModel = model.getRewardModel(rewardModelName);
    if (rewardModelName.empty()) {
        rewardModelName = model.getUniqueRewardModelName();
    }

    if (method == CvarMethod::WeightedReachability) {
        STORM_LOG_THROW(!rewardModel.hasStateActionRewards() && !rewardModel.hasTransitionRewards(), storm::exceptions::InvalidPropertyException,
                        "The weighted-reachability CVaR method requires state-based terminal rewards only.");
        return CvarBackendKind::WeightedReachability;
    }

    STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotImplementedException,
                    "CVaR queries with transition rewards are not supported yet.");

    if (method == CvarMethod::SspParetoVi) {
        return CvarBackendKind::Ssp;
    }

    if (rewardModel.hasStateActionRewards()) {
        return CvarBackendKind::Ssp;
    }
    return CvarBackendKind::WeightedReachability;
}
}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
