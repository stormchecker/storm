#include "storm/modelchecker/helper/finitehorizon/SparseNondeterministicStepBoundedHorizonHelper.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/IntervalForward.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/adapters/RationalNumberForward.h"
#include "storm/modelchecker/hints/ExplicitModelCheckerHint.h"
#include "storm/modelchecker/prctl/helper/SparseMdpEndComponentInformation.h"

#include "storm/models/sparse/StandardRewardModel.h"

#include "storm/storage/BitVector.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/solver/multiplier/Multiplier.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/utility/SignalHandler.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType, typename SolutionType>
SparseNondeterministicStepBoundedHorizonHelper<ValueType, SolutionType>::SparseNondeterministicStepBoundedHorizonHelper(
    /*storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::SparseMatrix<ValueType> const& backwardTransitions*/)
// transitionMatrix(transitionMatrix), backwardTransitions(backwardTransitions)
{
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
std::vector<SolutionType> SparseNondeterministicStepBoundedHorizonHelper<ValueType, SolutionType>::compute(
    Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
    uint64_t lowerBound, uint64_t upperBound, ModelCheckerHint const& hint) {
    std::vector<SolutionType> result(transitionMatrix.getRowGroupCount(), storm::utility::zero<SolutionType>());
    storm::storage::BitVector makeZeroColumns;

    // Determine the states that have 0 probability of reaching the target states.
    storm::storage::BitVector maybeStates;
    if (hint.isExplicitModelCheckerHint() && hint.template asExplicitModelCheckerHint<ValueType>().getComputeOnlyMaybeStates()) {
        maybeStates = hint.template asExplicitModelCheckerHint<ValueType>().getMaybeStates();
    } else {
        if (goal.minimize()) {
            maybeStates = storm::utility::graph::performProbGreater0A(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, phiStates,
                                                                      psiStates, true, upperBound);
        } else {
            maybeStates = storm::utility::graph::performProbGreater0E(backwardTransitions, phiStates, psiStates, true, upperBound);
        }
        if (lowerBound == 0) {
            maybeStates &= ~psiStates;
        } else {
            makeZeroColumns = psiStates;
        }
    }

    STORM_LOG_INFO("Preprocessing: " << maybeStates.getNumberOfSetBits() << " non-target states with probability greater 0.");

    if (!maybeStates.empty()) {
        storm::storage::SparseMatrix<ValueType> submatrix;
        std::vector<ValueType> b;
        uint64_t subresultSize;

        if constexpr (storm::IsIntervalType<ValueType>) {
            // For intervals, we cannot remove all non maybe states as that would lead to the upper probability of rows summing to below 1.
            // Instead we only drop all outgoing transitions of non maybe states.
            // See src/storm/modelchecker/prctl/helper/SparseMdpPrctlHelper.cpp:624 for more details.
            submatrix = transitionMatrix.filterEntries(transitionMatrix.getRowFilter(maybeStates));

            storm::utility::vector::setAllValues(b, transitionMatrix.getRowFilter(psiStates));

            subresultSize = transitionMatrix.getRowCount();
        } else {
            // We can eliminate the rows and columns from the original transition probability matrix that have probability 0.
            submatrix = transitionMatrix.getSubmatrix(true, maybeStates, maybeStates, false, makeZeroColumns);

            b = transitionMatrix.getConstrainedRowGroupSumVector(maybeStates, psiStates);
            subresultSize = maybeStates.getNumberOfSetBits();
        }

        // Create the vector with which to multiply.
        std::vector<SolutionType> subresult(subresultSize);

        auto multiplier = storm::solver::MultiplierFactory<ValueType, SolutionType>().create(env, submatrix);
        if (lowerBound == 0) {
            multiplier->repeatedMultiplyAndReduce(env, goal.direction(), subresult, &b, upperBound, goal.getUncertaintyResolutionMode());
        } else {
            multiplier->repeatedMultiplyAndReduce(env, goal.direction(), subresult, &b, upperBound - lowerBound + 1, goal.getUncertaintyResolutionMode());

            storm::storage::SparseMatrix<ValueType> submatrix;
            if constexpr (storm::IsIntervalType<ValueType>) {
                submatrix = transitionMatrix.filterEntries(transitionMatrix.getRowFilter(maybeStates));
            } else {
                submatrix = transitionMatrix.getSubmatrix(true, maybeStates, maybeStates, false);
            }

            auto multiplier = storm::solver::MultiplierFactory<ValueType, SolutionType>().create(env, submatrix);
            b = std::vector<ValueType>(b.size(), storm::utility::zero<ValueType>());
            multiplier->repeatedMultiplyAndReduce(env, goal.direction(), subresult, &b, lowerBound - 1, goal.getUncertaintyResolutionMode());
        }
        // Set the values of the resulting vector accordingly.
        storm::utility::vector::setVectorValues(result, maybeStates, subresult);
    }
    if (lowerBound == 0) {
        storm::utility::vector::setVectorValues(result, psiStates, storm::utility::one<SolutionType>());
    }
    return result;
}

template class SparseNondeterministicStepBoundedHorizonHelper<double>;
template class SparseNondeterministicStepBoundedHorizonHelper<storm::RationalNumber>;
template class SparseNondeterministicStepBoundedHorizonHelper<storm::Interval, double>;
template class SparseNondeterministicStepBoundedHorizonHelper<storm::RationalInterval, storm::RationalNumber>;
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm