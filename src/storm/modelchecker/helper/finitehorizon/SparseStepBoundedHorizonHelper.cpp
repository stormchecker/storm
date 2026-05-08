#include "storm/modelchecker/helper/finitehorizon/SparseStepBoundedHorizonHelper.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/adapters/RationalNumberForward.h"
#include "storm/modelchecker/hints/ExplicitModelCheckerHint.h"

#include "storm/models/sparse/StandardRewardModel.h"

#include "storm/storage/BitVector.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

#include "storm/solver/multiplier/Multiplier.h"
#include "storm/utility/SignalHandler.h"

namespace storm::modelchecker::helper {

namespace detail {
template<typename ValueType, typename SolutionType>
storm::storage::BitVector computeProbGreater0States(storm::solver::SolveGoal<ValueType, SolutionType> const& goal,
                                                    storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                    storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                    storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
                                                    uint64_t const upperBound, ModelCheckerHint const& hint) {
    if (hint.isExplicitModelCheckerHint() && hint.template asExplicitModelCheckerHint<ValueType>().getComputeOnlyMaybeStates()) {
        return hint.template asExplicitModelCheckerHint<ValueType>().getMaybeStates() | psiStates;
    } else {
        if (transitionMatrix.hasTrivialRowGrouping()) {  // DTMC
            return storm::utility::graph::performProbGreater0(backwardTransitions, phiStates, psiStates, true, upperBound);
        } else {  // MDP
            if (goal.minimize()) {
                return storm::utility::graph::performProbGreater0A(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, phiStates,
                                                                   psiStates, true, upperBound);
            } else {
                return storm::utility::graph::performProbGreater0E(backwardTransitions, phiStates, psiStates, true, upperBound);
            }
        }
    }
}
}  // namespace detail

template<typename ValueType, typename SolutionType>
SparseStepBoundedHorizonHelper<ValueType, SolutionType>::SparseStepBoundedHorizonHelper() {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
std::vector<SolutionType> SparseStepBoundedHorizonHelper<ValueType, SolutionType>::computeStepBoundedUntilProbabilities(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
    uint64_t const lowerBound, uint64_t const upperBound, ModelCheckerHint const& hint) {
    STORM_LOG_ASSERT(([&]() {
                         uint64_t const numStates = transitionMatrix.getRowGroupCount();
                         std::initializer_list<uint64_t> const r{transitionMatrix.getColumnCount(), backwardTransitions.getRowCount(),
                                                                 backwardTransitions.getColumnCount(), phiStates.size(), psiStates.size()};
                         return std::all_of(r.begin(), r.end(), [&numStates](auto i) { return i == numStates; });
                     }()),
                     "Inconsistent input dimensions.");
    STORM_LOG_ASSERT(transitionMatrix.hasTrivialRowGrouping() || goal.hasDirection(),
                     "Got a nondeterministic transition matrix but solve goal does not specify a direction.");
    STORM_LOG_ASSERT(!storm::IsIntervalType<ValueType> || storm::solver::isSet(goal.getUncertaintyResolutionMode()),
                     "Interval transition matrix given, but no uncertainty resolution mode is specified.");

    // Catch trivial case where lowerBound exceeds the upperBound
    if (lowerBound > upperBound) {
        return std::vector<SolutionType>(transitionMatrix.getRowGroupCount(), storm::utility::zero<SolutionType>());
    }

    storm::solver::OptimizationDirection const optimizationDirection =
        transitionMatrix.hasTrivialRowGrouping() ? OptimizationDirection::Maximize : goal.direction();

    // We identify states that must have probability 0 of reaching the target states to exclude them in the further analysis.
    storm::storage::BitVector const probGreater0States =
        detail::computeProbGreater0States<ValueType, SolutionType>(goal, transitionMatrix, backwardTransitions, phiStates, psiStates, upperBound, hint);
    STORM_LOG_INFO("Preprocessing step-bounded reachability probability computation: "
                   << probGreater0States.getNumberOfSetBits() << " states with probability greater 0. " << psiStates.getNumberOfSetBits() << " target states.");

    // We compute the values using matrix-vector multiplication in two phases.
    // The first phase computes the values for step epochs upperBound, upperBound-1, upperBound-2, ..., lowerBound.
    // In that phase, reaching a psiState incurs a probability of 1.
    // After the first phase, the result vector contains the probabilities for reaching Psi via Phi within upperBound-lowerBound steps.
    // The second phase computes the values for step epochs lowerBound-1, lowerBound-2, ..., 0.
    // In the second phase, psiStates are treated as any other state.

    // Allocate the solution vector for the iterations.
    std::vector<SolutionType> result;
    result.reserve(transitionMatrix.getRowGroupCount());  // x will later hold the final result for each state.

    // First phase: Apply upperBound-lowerBound many iterations
    // During this phase, we set probability 1 to all psiStates. The maybeStates are those for which we still need to compute a value.
    auto const firstPhaseMaybeStates = probGreater0States & ~psiStates;
    if constexpr (storm::IsIntervalType<ValueType>) {
        // For interval models, we do not remove non-maybestates for the analysis: We need to keep their incoming transition intervals so that we can pick
        // valid interval instantiations at predecessors of non-maybestates.
        // The result vector thus has one entry for each state.
        // We initialize the result with the probability of reaching psiStates in 0 steps.
        storm::utility::vector::setAllValues(result, psiStates, storm::utility::one<SolutionType>(), storm::utility::zero<SolutionType>());
        // Check if we actually need to do any iteration
        if (upperBound > lowerBound && firstPhaseMaybeStates.getNumberOfSetBits() > 0) {
            // For the iterations, we clear all outgoing transitions of non-maybe states.
            auto submatrix = transitionMatrix.filterEntries(transitionMatrix.getRowFilter(firstPhaseMaybeStates));
            // The `b` vector is used to set a constant value for the non-maybe states. That means it has to hold value 1 for all choices at psiStates.
            std::vector<ValueType> b;
            storm::utility::vector::setAllValues(b, transitionMatrix.getRowFilter(psiStates), storm::utility::one<ValueType>(),
                                                 storm::utility::zero<ValueType>());
            // Perform the iterations for the first phase
            auto multiplier = storm::solver::MultiplierFactory<ValueType, SolutionType>().create(env, std::move(submatrix));
            multiplier->repeatedMultiplyAndReduce(env, optimizationDirection, result, &b, upperBound - lowerBound, goal.getUncertaintyResolutionMode());
        }
    } else {
        // For non-interval models, we can consider a proper subsystem consisting only of maybe states. That means, the solution vector only has entries for
        // each maybeState. Initially, (when doing 0 steps), all maybeStates have value 0
        result.assign(firstPhaseMaybeStates.getNumberOfSetBits(), storm::utility::zero<SolutionType>());
        // Check if we actually need to do any iteration
        if (upperBound > lowerBound && firstPhaseMaybeStates.getNumberOfSetBits() > 0) {
            // Create the subsystem that only consists of maybe states.
            auto submatrix = transitionMatrix.getSubmatrix(true, firstPhaseMaybeStates, firstPhaseMaybeStates, false);
            // The 'b' vector contains for each choice the probabilities to reach psiStates within one step via that choice
            auto const b = transitionMatrix.getConstrainedRowGroupSumVector(firstPhaseMaybeStates, psiStates);
            // Perform the iterations for the first phase
            auto multiplier = storm::solver::MultiplierFactory<ValueType, SolutionType>().create(env, std::move(submatrix));
            multiplier->repeatedMultiplyAndReduce(env, optimizationDirection, result, &b, upperBound - lowerBound);
        }
    }

    // Second phase: Apply lowerBound many iterations
    auto const secondPhaseMaybeStates = probGreater0States & phiStates;
    if (lowerBound != 0 && !secondPhaseMaybeStates.empty()) {
        // In the second phase, we compute values for states those states where the value is not known to be zero.
        // Note that there might be (psiStates & ~phiStates)-states which have value 1 in the first phase but must get value 0 at the end of this phase.
        if constexpr (storm::IsIntervalType<ValueType>) {
            // As in the first phase, we build a submatrix with all states. Now we do not include outgoing transitions for all states that must have value zero
            auto submatrix = transitionMatrix.filterEntries(transitionMatrix.getRowFilter(secondPhaseMaybeStates));
            // Perform the iterations for the second phase. We do not need to set a `b` vector, as the non-maybeStates in the second phase must get value zero.
            // Note that (psiStates & ~phiStates)-states have value 1 right now, but will have value 0 after the first and any subsequent iteration.
            auto multiplier = storm::solver::MultiplierFactory<ValueType, SolutionType>().create(env, std::move(submatrix));
            multiplier->repeatedMultiplyAndReduce(env, optimizationDirection, result, nullptr, lowerBound, goal.getUncertaintyResolutionMode());
        } else {
            // Create the subsystem that only consists of maybe states.
            // In contrast to the first phase, we add (psiStates & phiStates) to the set of maybe states.
            // That means we have to enlarge our solution vector and insert probability 1 for those newly added states, as this is the value of those states
            // towards the end of the first phase.
            auto firstPhaseFilter = firstPhaseMaybeStates % secondPhaseMaybeStates;  // indicates those states that were already present before
            storm::utility::vector::blowUpVectorInPlace(result, firstPhaseFilter, storm::utility::one<SolutionType>());
            // Create a submatrix that only consists of maybeStates for the second phase
            auto submatrix = transitionMatrix.getSubmatrix(true, secondPhaseMaybeStates, secondPhaseMaybeStates, false);
            // Perform the iterations for the second phase.
            auto multiplier = storm::solver::MultiplierFactory<ValueType, SolutionType>().create(env, std::move(submatrix));
            uint64_t numIterations = lowerBound;
            // For the very first iteration, we might need to add the probability of reaching a (psiStates & ~phiStates) state in one step.
            if (auto const excludedPsiStates = psiStates & ~phiStates; !excludedPsiStates.empty()) {
                auto const b = transitionMatrix.getConstrainedRowGroupSumVector(secondPhaseMaybeStates, excludedPsiStates);
                multiplier->multiplyAndReduce(env, optimizationDirection, result, &b, result);
                --numIterations;
            }
            multiplier->repeatedMultiplyAndReduce(env, optimizationDirection, result, nullptr, numIterations);
            // Finally, we blow up the solution vector once more to also incorporate values for the non-maybe states, which all have value zero.
            storm::utility::vector::blowUpVectorInPlace(result, secondPhaseMaybeStates, storm::utility::zero<SolutionType>());
        }
    } else {
        // If there is no second phase, we still need to blow up the solution vector in case it refers to the reduced system
        if (!storm::IsIntervalType<ValueType>) {
            storm::utility::vector::blowUpVectorInPlace(result, firstPhaseMaybeStates, storm::utility::zero<SolutionType>());
            storm::utility::vector::setVectorValues(result, psiStates, storm::utility::one<SolutionType>());
        }
    }

    return result;
}

template class SparseStepBoundedHorizonHelper<double>;
template class SparseStepBoundedHorizonHelper<storm::RationalNumber>;
template class SparseStepBoundedHorizonHelper<storm::RationalFunction>;
template class SparseStepBoundedHorizonHelper<storm::Interval, double>;
template class SparseStepBoundedHorizonHelper<storm::RationalInterval, storm::RationalNumber>;

}  // namespace storm::modelchecker::helper