#pragma once

#include "storm/models/sparse/Pomdp.h"
#include "storm/utility/graph.h"

namespace storm {
namespace pomdp {
namespace qualitative {
template<typename ValueType>
bool isLookaheadRequired(storm::models::sparse::Pomdp<ValueType> const& pomdp, storm::storage::BitVector const& targetStates,
                         storm::storage::BitVector const& surelyReachSinkStates) {
    if (storm::utility::graph::checkIfECWithChoiceExists(pomdp.getTransitionMatrix(), pomdp.getBackwardTransitions(), ~targetStates & ~surelyReachSinkStates,
                                                         storm::storage::BitVector(pomdp.getNumberOfChoices(), true))) {
        STORM_LOG_DEBUG("Lookahead (possibly) required.");
        return true;
    } else {
        STORM_LOG_DEBUG("No lookahead required.");
        return false;
    }
}
}  // namespace qualitative
}  // namespace pomdp
}  // namespace storm
