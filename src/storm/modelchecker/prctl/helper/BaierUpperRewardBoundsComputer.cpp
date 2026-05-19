#include "storm/modelchecker/prctl/helper/BaierUpperRewardBoundsComputer.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm::modelchecker::helper {

template<typename ValueType>
BaierUpperRewardBoundsComputer<ValueType>::BaierUpperRewardBoundsComputer(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                          storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                          std::vector<ValueType> const& oneStepTargetProbabilities,
                                                                          std::function<uint64_t(uint64_t)> const& stateToScc)
    : transitionMatrix(transitionMatrix),
      backwardTransitions(&backwardTransitions),
      stateToScc(stateToScc),
      oneStepTargetProbabilities(oneStepTargetProbabilities) {
    // Intentionally left empty.
}

template<typename ValueType>
BaierUpperRewardBoundsComputer<ValueType>::BaierUpperRewardBoundsComputer(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                          std::vector<ValueType> const& oneStepTargetProbabilities,
                                                                          std::function<uint64_t(uint64_t)> const& stateToScc)
    : transitionMatrix(transitionMatrix), backwardTransitions(nullptr), stateToScc(stateToScc), oneStepTargetProbabilities(oneStepTargetProbabilities) {
    // Intentionally left empty.
}

template<typename ValueType>
std::vector<ValueType> BaierUpperRewardBoundsComputer<ValueType>::computeUpperBoundOnExpectedVisitingTimes(
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType> const& oneStepTargetProbabilities) {
    return computeUpperBoundOnExpectedVisitingTimes(transitionMatrix, transitionMatrix.transpose(true), oneStepTargetProbabilities);
}

template<typename ValueType>
std::vector<ValueType> BaierUpperRewardBoundsComputer<ValueType>::computeUpperBoundOnExpectedVisitingTimes(
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
    std::vector<ValueType> const& oneStepTargetProbabilities) {
    std::vector<uint64_t> stateToScc =
        storm::storage::StronglyConnectedComponentDecomposition<ValueType>(transitionMatrix).computeStateToSccIndexMap(transitionMatrix.getRowGroupCount());
    return computeUpperBoundOnExpectedVisitingTimes(transitionMatrix, backwardTransitions, oneStepTargetProbabilities,
                                                    [&stateToScc](uint64_t s) { return stateToScc[s]; });
}

template<typename ValueType>
std::vector<ValueType> BaierUpperRewardBoundsComputer<ValueType>::computeUpperBoundOnExpectedVisitingTimes(
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
    std::vector<ValueType> const& oneStepTargetProbabilities, std::function<uint64_t(uint64_t)> const& stateToScc) {
    // This first computes for every state s a non-zero lower bound d_s for the probability that starting at s, we never reach s again
    // An upper bound on the expected visiting times is given by 1/d_s
    // More precisely, we maintain a set of processed states.
    // Given a  processed states s, let T be the union of the target states and the states processed *before* s.
    // Then, the value d_s for s is a lower bound for the probability that from the next step on we always stay in T.
    // Since T does not contain s, d_s is thus also a lower bound for the probability that we never reach s again.
    // Very roughly, the procedure can be seen as a quantitative variant of 'performProb1A'.
    // Note: We slightly deviate from the description of Baier et al.  http://doi.org/10.1007/978-3-319-63387-9_8.
    // While they only consider processed states from a previous iteration step, we immediately consider them once they are processed

    auto const numStates = transitionMatrix.getRowGroupCount();
    assert(transitionMatrix.getRowCount() == oneStepTargetProbabilities.size());
    assert(backwardTransitions.getRowCount() == numStates);
    assert(backwardTransitions.getColumnCount() == numStates);
    auto const& rowGroupIndices = transitionMatrix.getRowGroupIndices();

    // Initialize the 'valid' choices.
    // A choice is valid iff it goes to processed states with non-zero probability.
    // Initially, mark all choices as valid that have non-zero probability to go to the target states *or* to a different Scc.
    auto validChoices = storm::utility::vector::filterGreaterZero(oneStepTargetProbabilities);
    for (uint64_t state = 0; state < numStates; ++state) {
        auto const scc = stateToScc(state);
        for (auto rowIndex = rowGroupIndices[state], rowEnd = rowGroupIndices[state + 1]; rowIndex < rowEnd; ++rowIndex) {
            auto const row = transitionMatrix.getRow(rowIndex);
            if (std::any_of(row.begin(), row.end(), [&stateToScc, &scc](auto const& entry) { return scc != stateToScc(entry.getColumn()); })) {
                validChoices.set(rowIndex, true);
            }
        }
    }

    // Vector that holds the result.
    std::vector<ValueType> result(numStates, storm::utility::one<ValueType>());
    // The states that we already have assigned a value for.
    storm::storage::BitVector processedStates(numStates, false);

    // Auxiliary function that checks if the given row is valid, i.e. has a transition to a different SCC or to an already processed state.
    // Since a once valid choice can never become invalid, we cache valid choices
    auto isValidChoice = [&transitionMatrix, &validChoices, &processedStates](uint64_t const& choice) {
        if (validChoices.get(choice)) {
            return true;
        }
        auto row = transitionMatrix.getRow(choice);
        // choices that lead to different SCCs already have been marked as valid above.
        // Hence, we don't have to check for SCCs anymore.
        if (std::any_of(row.begin(), row.end(), [&processedStates](auto const& entry) { return processedStates.get(entry.getColumn()); })) {
            validChoices.set(choice, true);  // cache for next time
            return true;
        } else {
            return false;
        }
    };

    // Auxiliary function that computes the value of a given row
    auto getChoiceValue = [&transitionMatrix, &oneStepTargetProbabilities, &processedStates, &stateToScc, &result](uint64_t const& rowIndex,
                                                                                                                   uint64_t const& sccIndex) {
        ValueType rowValue = oneStepTargetProbabilities[rowIndex];
        for (auto const& entry : transitionMatrix.getRow(rowIndex)) {
            if (auto successorState = entry.getColumn(); sccIndex != stateToScc(successorState)) {
                rowValue += entry.getValue();  // * 1
            } else if (processedStates.get(successorState)) {
                rowValue += entry.getValue() * result[successorState];
            }
        }
        return rowValue;
    };

    // For efficiency, we maintain a set of candidateStates that satisfy some necessary conditions for being processed.
    // A candidateState s is unprocessed, but has a processed successor state s' such that s' was processed *after* the previous time we checked candidate s.
    storm::storage::BitVector candidateStates(numStates, true);

    // We usually get the best performance by processing states in inverted order.
    // This is because in the sparse engine the states are explored with a BFS/DFS
    uint64_t unprocessedEnd = numStates;
    auto candidateStateIt = candidateStates.rbegin();
    auto const candidateStateItEnd = candidateStates.rend();
    while (true) {
        // Assert invariant: all states with index >= unprocessedEnd are processed and state (unprocessedEnd - 1) is not processed
        STORM_LOG_ASSERT(processedStates.getNextUnsetIndex(unprocessedEnd) == processedStates.size(), "Invalid index for last unexplored state");
        STORM_LOG_ASSERT(candidateStates.isSubsetOf(~processedStates), "");
        uint64_t const state = *candidateStateIt;
        auto const group = transitionMatrix.getRowGroupIndices(state);
        if (std::all_of(group.begin(), group.end(), [&isValidChoice](auto const& choice) { return isValidChoice(choice); })) {
            // Compute the state value
            storm::utility::Minimum<ValueType> minimalStateValue;
            auto const scc = stateToScc(state);
            for (auto choice : group) {
                minimalStateValue &= getChoiceValue(choice, scc);
            }
            result[state] = *minimalStateValue;
            processedStates.set(state);
            if (state == unprocessedEnd - 1) {
                unprocessedEnd = processedStates.getStartOfOneSequenceBefore(unprocessedEnd - 1);
                if (unprocessedEnd == 0) {
                    STORM_LOG_ASSERT(processedStates.full(), "Expected all states to be processed");
                    break;
                }
            }
            // Iterate through predecessors that might become valid in the next run
            for (auto const& predEntry : backwardTransitions.getRow(state)) {
                if (!processedStates.get(predEntry.getColumn())) {
                    candidateStates.set(predEntry.getColumn());
                }
            }
        }
        // state is no longer a candidate
        candidateStates.set(state, false);
        // Get next candidate state
        ++candidateStateIt;
        if (candidateStateIt == candidateStateItEnd) {
            // wrap around
            candidateStateIt = candidateStates.rbegin(unprocessedEnd);
            // If there are no new candidates, we likely have mecs consisting of unprocessed states. For those, there is no upper bound on the EVTs.
            STORM_LOG_THROW(candidateStateIt != candidateStateItEnd, storm::exceptions::InvalidOperationException,
                            "Unable to compute finite upper bounds for visiting times: No more candidates.");
        }
    }

    // Transform the d_t to an upper bound for zeta(t) (i.e. the expected number of visits of t
    storm::utility::vector::applyPointwise(result, result, [](ValueType const& r) -> ValueType { return storm::utility::one<ValueType>() / r; });
    return result;
}

template<typename ValueType>
typename BaierUpperRewardBoundsComputer<ValueType>::Bounds BaierUpperRewardBoundsComputer<ValueType>::computeTotalRewardBounds(
    std::vector<ValueType> const& rewards) {
    STORM_LOG_TRACE("Computing upper reward bounds using variant-2 of Baier et al.");

    // Ensure backward transitions are available.
    storm::storage::SparseMatrix<ValueType> computedBackwardTransitions;
    if (!backwardTransitions) {
        computedBackwardTransitions = transitionMatrix.transpose(true);
    }
    auto const& backwardTransRef = backwardTransitions ? *backwardTransitions : computedBackwardTransitions;

    // Ensure SCCs are available.
    std::vector<uint64_t> computedStateToSccVec;
    std::function<uint64_t(uint64_t)> stateToSccFct;
    if (this->stateToScc) {
        stateToSccFct = this->stateToScc;
    } else {
        // If no stateToScc function is given, we compute the SCCs from the transition matrix.
        computedStateToSccVec =
            storm::storage::StronglyConnectedComponentDecomposition<ValueType>(transitionMatrix).computeStateToSccIndexMap(transitionMatrix.getRowGroupCount());
        stateToSccFct = [&computedStateToSccVec](uint64_t s) { return computedStateToSccVec[s]; };
    }

    auto expVisits = computeUpperBoundOnExpectedVisitingTimes(transitionMatrix, backwardTransRef, oneStepTargetProbabilities, stateToSccFct);

    Bounds result{.lower = storm::utility::zero<ValueType>(), .upper = storm::utility::zero<ValueType>()};
    for (uint64_t state = 0; state < expVisits.size(); ++state) {
        // Get the maximum / minimum reward that can be collected in the state.
        // We distinguish between choices that surely exit the current SCC and those that do not.
        // The former can only be taken at most once so we do not have to multiply with the upper bound on the expected visits.
        auto const currScc = stateToSccFct(state);
        storm::utility::Maximum<ValueType> maxReward(storm::utility::zero<ValueType>()), maxRewardStay;
        storm::utility::Minimum<ValueType> minReward(storm::utility::zero<ValueType>()), minRewardStay;
        // By starting the maxRewards with zero, negative rewards are essentially ignored which is necessary to provide a valid upper bound
        for (auto rowIndex : transitionMatrix.getRowGroupIndices(state)) {
            auto const row = transitionMatrix.getRow(rowIndex);
            bool const exitingChoice =
                std::all_of(row.begin(), row.end(), [&stateToSccFct, &currScc](auto const& entry) { return currScc != stateToSccFct(entry.getColumn()); });
            if (exitingChoice) {
                maxReward &= rewards[rowIndex];
                minReward &= rewards[rowIndex];
            } else {
                maxRewardStay &= rewards[rowIndex];
                minRewardStay &= rewards[rowIndex];
            }
        }
        if (!maxRewardStay.empty()) {
            maxReward &= (*maxRewardStay * expVisits[state]);
        }
        result.upper += *maxReward;
        if (!minRewardStay.empty()) {
            minReward &= (*minRewardStay * expVisits[state]);
        }
        result.lower += *minReward;
    }

    STORM_LOG_TRACE("Baier algorithm for reward bound computation (variant 2) computed bounds [" << result.lower << ", " << result.upper << "].");
    return result;
}

template class BaierUpperRewardBoundsComputer<double>;
template class BaierUpperRewardBoundsComputer<storm::RationalNumber>;
}  // namespace storm::modelchecker::helper
