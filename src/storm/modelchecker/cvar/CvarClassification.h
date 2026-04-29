#pragma once

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/modelchecker/cvar/CvarMethod.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {
/*!
 * Classifies the embedded CVaR query at the formula level.
 *
 * This is intentionally separate from the concrete problem kind below:
 * multiple concrete problem kinds may share the same surface query syntax.
 */
enum class CvarQueryKind { ReachabilityReward };

/*!
 * Classifies the concrete solver problem induced by a CVaR query on a given
 * model and reward structure.
 *
 * Weighted reachability is the currently implemented LP-based terminal-reward
 * setting. SSP will be used by the future value-iteration implementation for
 * accumulated state-action costs until reaching the goal.
 */
enum class CvarProblemKind { WeightedReachability, Ssp };

/*!
 * Determines the formula-level CVaR query kind.
 *
 * The current front-end only admits reachability reward CVaR queries, but this
 * explicit classification provides the extension point for future CVaR query
 * families.
 */
inline CvarQueryKind classifyCvarQuery(CvarQueryInformation const&) {
    return CvarQueryKind::ReachabilityReward;
}

/*!
 * Classifies the concrete CVaR problem kind to use.
 *
 * The selection can be overridden explicitly via the CVaR method setting.
 * Otherwise, classification stays conservative: state-action reward models are
 * routed to the SSP branch, while state-only reward models remain on the
 * weighted-reachability path until SSP preprocessing is introduced.
 */
template<typename SparseMdpModelType>
CvarProblemKind classifyCvarProblem(SparseMdpModelType const& model, CvarQueryInformation const& queryInformation, CvarQueryKind,
                                    storm::storage::BitVector const&, CvarMethod method) {
    std::string rewardModelName = queryInformation.rewardModelName ? queryInformation.rewardModelName.get() : "";
    auto const& rewardModel = model.getRewardModel(rewardModelName);
    if (rewardModelName.empty()) {
        rewardModelName = model.getUniqueRewardModelName();
    }

    if (method == CvarMethod::WeightedReachability) {
        STORM_LOG_THROW(!rewardModel.hasStateActionRewards() && !rewardModel.hasTransitionRewards(), storm::exceptions::InvalidPropertyException,
                        "The weighted-reachability CVaR method requires state-based terminal rewards only.");
        return CvarProblemKind::WeightedReachability;
    }

    STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotImplementedException,
                    "CVaR queries with transition rewards are not supported yet.");

    if (method == CvarMethod::SspParetoVi) {
        return CvarProblemKind::Ssp;
    }

    if (rewardModel.hasStateActionRewards()) {
        return CvarProblemKind::Ssp;
    }
    return CvarProblemKind::WeightedReachability;
}
}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
