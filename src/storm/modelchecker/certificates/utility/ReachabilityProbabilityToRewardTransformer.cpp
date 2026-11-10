#include "storm/modelchecker/certificates/utility/ReachabilityProbabilityToRewardTransformer.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

namespace storm::modelchecker {

template<typename ValueType>
ReachabilityProbabilityToRewardTransformer<ValueType>::ReachabilityProbabilityToRewardTransformer(
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix)
    : transitionMatrix(transitionMatrix), backwardTransitionCache(transitionMatrix) {
    // Intentionally left empty.
}

template<typename ValueType>
ReachabilityProbabilityToRewardTransformer<ValueType>::ReachabilityProbabilityToRewardTransformer(
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BackwardTransitionCache<ValueType> backwardTransitionCache)
    : transitionMatrix(transitionMatrix), backwardTransitionCache(backwardTransitionCache) {
    // Intentionally left empty.
}

template<typename ValueType>
TotalRewardSpecification<ValueType> ReachabilityProbabilityToRewardTransformer<ValueType>::transform(StateSet const& targetStates,
                                                                                                     storm::OptionalRef<StateSet const> constraintStates) {
    STORM_LOG_ASSERT(transitionMatrix.hasTrivialRowGrouping(), "Transition matrix has non-trivial row grouping but no optimization direction is given.");
    StateSet defaultConstraintStates;
    if (!constraintStates.has_value()) {
        defaultConstraintStates = StateSet(transitionMatrix.getRowGroupCount(), true);
        constraintStates.reset(defaultConstraintStates);
    }
    auto [prob01States, prob1States] = storm::utility::graph::performProb01(backwardTransitionCache.get(), constraintStates.value(), targetStates);
    prob01States |= prob1States;

    return TotalRewardSpecification<ValueType>{
        std::move(prob01States), {{TerminalStateValue<ValueType>::getOne(), std::move(prob1States)}}, true, storm::NullRef};
}

template<typename ValueType>
TotalRewardSpecification<ValueType> ReachabilityProbabilityToRewardTransformer<ValueType>::transform(std::optional<storm::OptimizationDirection> dir,
                                                                                                     StateSet const& targetStates,
                                                                                                     storm::OptionalRef<StateSet const> constraintStates) {
    if (!dir.has_value()) {
        return transform(targetStates, constraintStates);
    }
    StateSet defaultConstraintStates;
    if (!constraintStates.has_value()) {
        defaultConstraintStates = StateSet(transitionMatrix.getRowGroupCount(), true);
        constraintStates.reset(defaultConstraintStates);
    }

    auto [prob01States, prob1States] = storm::solver::minimize(*dir)
                                           ? storm::utility::graph::performProb01Min(transitionMatrix, transitionMatrix.getRowGroupIndices(),
                                                                                     backwardTransitionCache.get(), constraintStates.value(), targetStates)
                                           : storm::utility::graph::performProb01Max(transitionMatrix, transitionMatrix.getRowGroupIndices(),
                                                                                     backwardTransitionCache.get(), constraintStates.value(), targetStates);
    prob01States |= prob1States;
    return TotalRewardSpecification<ValueType>{
        std::move(prob01States), {{TerminalStateValue<ValueType>::getOne(), std::move(prob1States)}}, storm::solver::minimize(*dir), storm::NullRef};
}

template class ReachabilityProbabilityToRewardTransformer<double>;
template class ReachabilityProbabilityToRewardTransformer<storm::RationalNumber>;

}  // namespace storm::modelchecker
