#include "storm/modelchecker/certificates/utility/ReachabilityRewardTransformer.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

namespace storm::modelchecker {

template<typename ValueType>
ReachabilityRewardTransformer<ValueType>::ReachabilityRewardTransformer(storm::storage::SparseMatrix<ValueType> const& transitionMatrix)
    : transitionMatrix(transitionMatrix), backwardTransitionCache(transitionMatrix) {
    // Intentionally left empty.
}

template<typename ValueType>
ReachabilityRewardTransformer<ValueType>::ReachabilityRewardTransformer(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                        storm::storage::BackwardTransitionCache<ValueType> backwardTransitionCache)
    : transitionMatrix(transitionMatrix), backwardTransitionCache(backwardTransitionCache) {
    // Intentionally left empty.
}

template<typename ValueType>
TotalRewardSpecification<ValueType> ReachabilityRewardTransformer<ValueType>::transform(StateSet const& targetStates,
                                                                                        std::vector<ValueType> const& actionRewards) {
    STORM_LOG_ASSERT(transitionMatrix.hasTrivialRowGrouping(), "Transition matrix has non-trivial row grouping but no optimization direction is given.");

    // As usual for reachability rewards, the reward is infinite if the target is not reached with probability one (even if all rewards are zero).
    StateSet allStates(transitionMatrix.getRowGroupCount(), true);
    auto infStates = storm::utility::graph::performProb1(backwardTransitionCache.get(), allStates, targetStates);
    infStates.complement();
    return TotalRewardSpecification<ValueType>{.terminalStates = targetStates | infStates,
                                               .terminalStateValues = {{TerminalStateValue<ValueType>::getPlusInfinity(), std::move(infStates)}},
                                               .terminalStatesUniversallyAlmostSurelyReached = true,
                                               .actionRewards = actionRewards};
}

template<typename ValueType>
TotalRewardSpecification<ValueType> ReachabilityRewardTransformer<ValueType>::transform(std::optional<storm::OptimizationDirection> dir,
                                                                                        StateSet const& targetStates,
                                                                                        std::vector<ValueType> const& actionRewards) {
    if (!dir.has_value()) {
        return transform(targetStates, actionRewards);
    }
    // As usual for reachability rewards, the reward is infinite if the target is not reached with probability one (even if all rewards are zero).
    // For minimizing (maximizing), this considers whether there is a scheduler (whether all schedulers) that reach the target with probability one.
    StateSet allStates(transitionMatrix.getRowGroupCount(), true);
    auto infStates = storm::solver::minimize(*dir) ? storm::utility::graph::performProb1E(transitionMatrix, transitionMatrix.getRowGroupIndices(),
                                                                                          backwardTransitionCache.get(), allStates, targetStates)
                                                   : storm::utility::graph::performProb1A(transitionMatrix, transitionMatrix.getRowGroupIndices(),
                                                                                          backwardTransitionCache.get(), allStates, targetStates);
    infStates.complement();
    return TotalRewardSpecification<ValueType>{.terminalStates = targetStates | infStates,
                                               .terminalStateValues = {{TerminalStateValue<ValueType>::getPlusInfinity(), std::move(infStates)}},
                                               .terminalStatesUniversallyAlmostSurelyReached = storm::solver::maximize(*dir),
                                               .actionRewards = actionRewards};
}

template class ReachabilityRewardTransformer<double>;
template class ReachabilityRewardTransformer<storm::RationalNumber>;

}  // namespace storm::modelchecker
