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

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType, bool Deterministic, typename SolutionType>
SparseStepBoundedHorizonHelper<ValueType, Deterministic, SolutionType>::SparseStepBoundedHorizonHelper() {
    // Intentionally left empty.
}

template<typename ValueType, bool Deterministic, typename SolutionType>
std::vector<SolutionType> SparseStepBoundedHorizonHelper<ValueType, Deterministic, SolutionType>::compute(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
    uint64_t lowerBound, uint64_t upperBound, ModelCheckerHint const& hint) {
    std::vector<SolutionType> result(transitionMatrix.getRowGroupCount(), storm::utility::zero<SolutionType>());

    storm::solver::OptimizationDirection optimizationDirection;
    if constexpr (Deterministic) {
        optimizationDirection = OptimizationDirection::Maximize;
    } else {
        optimizationDirection = goal.direction();
    }

    // If we identify the states that have probability 0 of reaching the target states, we might be able to exclude them in the further analysis.
    // For the 'maybeStates' we definitely have to compute the values.
    // In the case of 'lowerBound != 0', the 'maybeStates' also include the psi states, as we need their outgoing transitions during computation.
    storm::storage::BitVector maybeStates = computeMaybeStates(goal, transitionMatrix, backwardTransitions, phiStates, psiStates, lowerBound, upperBound, hint);
    storm::storage::BitVector makeZeroColumns;

    // In the non-interval case for 'lowerBound != 0', we do not want the psi states to contribute twice to the computation. Note that they are already included
    // in the 'b' vector as a result of the one-step probabilities and are a part of the 'maybeStates', as we need them during the shifting.
    if constexpr (!storm::IsIntervalType<ValueType>) {
        if (lowerBound != 0) {
            makeZeroColumns = psiStates;
        }
    }

    STORM_LOG_INFO("Preprocessing: " << maybeStates.getNumberOfSetBits() << " maybe states with probability greater 0.");

    if (!maybeStates.empty()) {
        storm::storage::SparseMatrix<ValueType> submatrix;
        std::vector<ValueType> b;
        uint64_t subresultSize;

        // In case of interval models we do not incorporate the one-step probabilities to the target in 'b' when computing '<=' and '<'. Thus, we need to
        // perform 'upperBound + 1' multiplications in the interval case, instead of 'upperBound' in the non-interval case.
        bool bIncludesOneStepProbabilities = false;
        if constexpr (storm::IsIntervalType<ValueType>) {
            // For intervals, we cannot remove all non-maybe states, as that would lead to the upper probability of rows summing to below 1.
            submatrix = transitionMatrix.filterEntries(transitionMatrix.getRowFilter(maybeStates));

            storm::utility::vector::setAllValues(b, transitionMatrix.getRowFilter(psiStates));

            subresultSize = transitionMatrix.getRowGroupCount();
        } else {
            // We can eliminate the rows and columns from the original transition probability matrix that have probability 0.
            submatrix = transitionMatrix.getSubmatrix(true, maybeStates, maybeStates, false, makeZeroColumns);

            // Create the vector of one-step probabilities to go to target states.
            b = transitionMatrix.getConstrainedRowGroupSumVector(maybeStates, psiStates);
            bIncludesOneStepProbabilities = true;

            subresultSize = maybeStates.getNumberOfSetBits();
        }

        // Create the vector with which to multiply.
        std::vector<SolutionType> subresult(subresultSize);

        auto multiplier = storm::solver::MultiplierFactory<ValueType, SolutionType>().create(env, submatrix);
        if (lowerBound == 0) {  // Case '<=' and '<'
            multiplier->repeatedMultiplyAndReduce(env, optimizationDirection, subresult, &b, upperBound + (bIncludesOneStepProbabilities ? 0 : 1),
                                                  goal.getUncertaintyResolutionMode());
        } else {  // Case '[lowerBound, upperBound]'
            if constexpr (storm::IsIntervalType<ValueType>) {
                // Compute the robust one-step probabilities.
                std::vector<ValueType> emptyB(b.size(), storm::utility::zero<ValueType>());
                // Here, the 'b' vector contains '1' for psi-states, which we want to copy into subresult first.
                multiplier->multiplyAndReduce(env, optimizationDirection, subresult, &b, subresult, goal.getUncertaintyResolutionMode());
                // Next, we multiply once to obtain the one-step probabilities (with an empty 'b' vector, as we do not want any offset).
                multiplier->multiplyAndReduce(env, optimizationDirection, subresult, &emptyB, subresult, goal.getUncertaintyResolutionMode());

                if (upperBound == lowerBound) {
                    // Intentionally left empty, as we are already done after computing the one-step probabilities.
                } else if (upperBound > lowerBound) {
                    // Compute probabilities to hit target in [1 .. upperBound - lowerBound + 1] steps.
                    // We need to do this manually, as we cannot precompute the contribution of psi-states separately.
                    // In each Bellman backup, the optimizer has to choose one feasible row distribution that
                    // determines the probability mass for both psi states and the other successors.
                    uint64_t stepsInterval = upperBound - lowerBound + 1;

                    // Perform remaining steps of steps interval.
                    for (uint64_t step = 2; step <= stepsInterval; ++step) {
                        // Keep '1' for every psi state, as they are included in the submatrix and would otherwise be updated according to their outgoing
                        // transitions. However, for bounded reachability within a step-interval, reaching a psi state satisfies the objective.
                        // Thus, this '1' needs to be considered when resolving the uncertainty.
                        storm::utility::vector::setAllValues(subresult, transitionMatrix.getRowFilter(psiStates), storm::utility::one<SolutionType>());
                        multiplier->multiplyAndReduce(env, optimizationDirection, subresult, &emptyB, subresult, goal.getUncertaintyResolutionMode());
                    }
                } else {
                    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot compute step-bounded until with lowerBound > upperBound");
                }
            } else {
                multiplier->repeatedMultiplyAndReduce(env, optimizationDirection, subresult, &b, upperBound - lowerBound + 1,
                                                      goal.getUncertaintyResolutionMode());
            }

            if constexpr (storm::IsIntervalType<ValueType>) {
                // For lower-bounded step-interval queries, we also need the outgoing transitions of psi states during shifting.
                submatrix = transitionMatrix.filterEntries(transitionMatrix.getRowFilter(maybeStates));
            } else {
                // Here, we actually need the outgoing transitions of the psi states. Thus, we do not apply the 'makeZeroColumns'.
                submatrix = transitionMatrix.getSubmatrix(true, maybeStates, maybeStates, false);
            }

            // Shift result to hit target in [1 .. upperBound - lowerBound + 1] steps by 'lowerBound - 1'-steps.
            multiplier = storm::solver::MultiplierFactory<ValueType, SolutionType>().create(env, submatrix);
            b = std::vector<ValueType>(b.size(), storm::utility::zero<ValueType>());
            multiplier->repeatedMultiplyAndReduce(env, optimizationDirection, subresult, &b, lowerBound - 1, goal.getUncertaintyResolutionMode());
        }

        // Set the values of the resulting vector accordingly.
        storm::utility::vector::setVectorValues(result, maybeStates, subresult);
    }

    if (lowerBound == 0) {
        storm::utility::vector::setVectorValues(result, psiStates, storm::utility::one<SolutionType>());
    }

    return result;
}

template<typename ValueType, bool Deterministic, typename SolutionType>
storm::storage::BitVector SparseStepBoundedHorizonHelper<ValueType, Deterministic, SolutionType>::computeMaybeStates(
    storm::solver::SolveGoal<ValueType, SolutionType> const& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
    uint64_t lowerBound, uint64_t upperBound, ModelCheckerHint const& hint) {
    storm::storage::BitVector maybeStates;

    if (hint.isExplicitModelCheckerHint() && hint.template asExplicitModelCheckerHint<ValueType>().getComputeOnlyMaybeStates()) {
        maybeStates = hint.template asExplicitModelCheckerHint<ValueType>().getMaybeStates();
    } else {
        if constexpr (Deterministic) {  // DTMC
            maybeStates = storm::utility::graph::performProbGreater0(backwardTransitions, phiStates, psiStates, true, upperBound);
        } else {  // MDP
            if (goal.minimize()) {
                maybeStates = storm::utility::graph::performProbGreater0A(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions,
                                                                          phiStates, psiStates, true, upperBound);
            } else {
                maybeStates = storm::utility::graph::performProbGreater0E(backwardTransitions, phiStates, psiStates, true, upperBound);
            }
        }

        if (lowerBound == 0) {
            maybeStates &= ~psiStates;
        }
    }

    return maybeStates;
}

template class SparseStepBoundedHorizonHelper<double, false>;
template class SparseStepBoundedHorizonHelper<double, true>;
template class SparseStepBoundedHorizonHelper<storm::RationalNumber, false>;
template class SparseStepBoundedHorizonHelper<storm::RationalNumber, true>;
template class SparseStepBoundedHorizonHelper<storm::RationalFunction, true>;
template class SparseStepBoundedHorizonHelper<storm::Interval, false, double>;
template class SparseStepBoundedHorizonHelper<storm::Interval, true, double>;
template class SparseStepBoundedHorizonHelper<storm::RationalInterval, false, storm::RationalNumber>;
template class SparseStepBoundedHorizonHelper<storm::RationalInterval, true, storm::RationalNumber>;

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm