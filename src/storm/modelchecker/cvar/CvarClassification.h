#pragma once

#include "storm/modelchecker/cvar/CvarFormulaInformation.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace modelchecker {
namespace cvar {
/*!
 * Classifies the embedded CVaR query at the formula level.
 *
 * This is intentionally separate from the concrete problem kind below:
 * multiple concrete problem kinds may share the same surface query syntax.
 */
enum class CvarQueryKind {
    ReachabilityReward
};

/*!
 * Classifies the concrete solver problem induced by a CVaR query on a given
 * model and reward structure.
 *
 * Weighted reachability is the currently implemented LP-based terminal-reward
 * setting. SSP will be used by the future value-iteration implementation for
 * accumulated state-action costs until reaching the goal.
 */
enum class CvarProblemKind {
    WeightedReachability,
    Ssp
};

/*!
 * Determines the formula-level CVaR query kind.
 *
 * The current front-end only admits reachability reward CVaR queries, but this
 * explicit classification provides the extension point for future CVaR query
 * families.
 */
inline CvarQueryKind classifyCvarQuery(CvarFormulaInformation const&) {
    return CvarQueryKind::ReachabilityReward;
}

/*!
 * Classifies the concrete CVaR problem kind to use.
 *
 * This first version is intentionally conservative and preserves the current
 * behavior by routing all supported CVaR queries through the existing weighted
 * reachability implementation. The SSP branch will be enabled in follow-up
 * commits once its preprocessing and solver path are introduced.
 */
template<typename SparseMdpModelType>
CvarProblemKind classifyCvarProblem(SparseMdpModelType const&, CvarFormulaInformation const&, CvarQueryKind, storm::storage::BitVector const&) {
    return CvarProblemKind::WeightedReachability;
}
} // namespace cvar
} // namespace modelchecker
} // namespace storm
