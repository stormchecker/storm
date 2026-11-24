#pragma once

#include <utility>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/OptionalRef.h"

namespace storm::modelchecker {

/*!
 * Specifies a total reward query: The goal is to compute (bounds on) the optimal total reward that is collected until a terminal state is reached.
 * The total reward is the sum of the rewards collected upon executing actions and the value of the terminal state that is reached (which can be infinite).
 *
 * This unified view allows to treat different queries with the same algorithms (e.g. for computing certificates):
 * - Reachability probabilities have no action rewards. Terminal states with value one are those from which the target is reached with probability one.
 *   The remaining terminal states have value zero and are those from which the target is not reachable.
 * - Reachability rewards collect the given action rewards. The target states are terminal states with value zero.
 *   The remaining terminal states have value +infinity and are those from which the target is not reached with probability one.
 */
template<typename ValueType>
struct TotalRewardSpecification {
    /// Those states that are terminal, i.e., no further reward is collected after reaching them
    storm::storage::BitVector terminalStates;
    /// Rewards collected upon entering terminal states (unmentioned terminal states have reward 0). The values can be infinite.
    /// The occurring state sets must be subsets of terminalStates and pairwise disjoint.
    std::vector<std::pair<storm::utility::ExtendedValueType<ValueType>, storm::storage::BitVector>> terminalStateValues;
    bool terminalStatesUniversallyAlmostSurelyReached;  /// If true, the terminal states are reached almost surely from every state under every scheduler
    storm::OptionalRef<std::vector<ValueType> const>
        actionRewards;  /// Rewards collected upon executing actions (if empty, rewards are only collected in terminal states)
};
}  // namespace storm::modelchecker
