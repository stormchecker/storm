#pragma once

namespace storm {
namespace solver {
namespace stateelimination {

/*!
 * An enum that contains all available state elimination orders.
 */
enum class EliminationOrder { Forward, ForwardReversed, Backward, BackwardReversed, Random, StaticPenalty, DynamicPenalty, RegularExpression };

}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
