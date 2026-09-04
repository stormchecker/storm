#pragma once

#include <optional>

#include "storm/models/sparse/Pomdp.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"

namespace storm {
namespace pomdp {

/*!
 * This method tries to detect that the beliefmdp is finite.
 * If this returns true, the beliefmdp is certainly finite.
 * However, if this returns false, the beliefmdp might still be finite
 * It is assumed that the belief MDP is not further explored when reaching a targetstate
 */
template<typename ValueType>
bool detectFiniteBeliefMdp(storm::models::sparse::Pomdp<ValueType> const& pomdp, std::optional<storm::storage::BitVector> const& targetStates) {
    // All infinite paths of the POMDP (including the ones with prob. 0 ) either
    //  - reach a target state after finitely many steps or
    //  - after finitely many steps enter an SCC and do not leave it
    // Hence, any path of the belief MDP will at some point either reach a target state or stay in a set of POMDP SCCs.
    // Only in the latter case we can get infinitely many different belief states.
    // Below, we check whether all SCCs only consist of Dirac distributions.
    // If this is the case, no new belief states will be found at some point.

    // Get the SCC decomposition
    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.dropNaiveSccs();
    storm::storage::BitVector relevantStates;
    if (targetStates) {
        relevantStates = ~targetStates.value();
        options.subsystem(relevantStates);
    }
    storm::storage::StronglyConnectedComponentDecomposition<ValueType> sccs(pomdp.getTransitionMatrix(), options);

    // Check whether all choices that stay within an SCC have Dirac distributions
    for (auto const& scc : sccs) {
        for (auto const& sccState : scc) {
            for (uint64_t rowIndex = pomdp.getNondeterministicChoiceIndices()[sccState]; rowIndex < pomdp.getNondeterministicChoiceIndices()[sccState + 1];
                 ++rowIndex) {
                for (auto const& entry : pomdp.getTransitionMatrix().getRow(rowIndex)) {
                    if (!storm::numbers::isOne(entry.getValue()) && !storm::numbers::isZero(entry.getValue())) {
                        if (scc.containsState(entry.getColumn())) {
                            // There is a non-dirac choice that stays in the SCC.
                            // This could still mean that the belief MDP is finite
                            // e.g., if at some point the branches merge back to the same state
                            return false;
                        }
                    }
                }
            }
        }
    }

    return true;
}
}  // namespace pomdp
}  // namespace storm