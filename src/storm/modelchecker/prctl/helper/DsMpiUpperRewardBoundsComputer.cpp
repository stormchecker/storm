#include "storm/modelchecker/prctl/helper/DsMpiUpperRewardBoundsComputer.h"

#include "storm-config.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/ConsecutiveUint64DynamicPriorityQueue.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/StateType.h"
#include "storm/utility/ConstantsComparator.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType>
DsMpiDtmcUpperRewardBoundsComputer<ValueType>::DsMpiDtmcUpperRewardBoundsComputer(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                  std::vector<ValueType> const& rewards,
                                                                                  std::vector<ValueType> const& oneStepTargetProbabilities)
    : transitionMatrix(transitionMatrix),
      originalRewards(rewards),
      originalOneStepTargetProbabilities(oneStepTargetProbabilities),
      backwardTransitions(transitionMatrix.transpose()),
      p(transitionMatrix.getRowGroupCount()),
      w(transitionMatrix.getRowGroupCount()),
      rewards(rewards),
      targetProbabilities(oneStepTargetProbabilities) {
    // Intentionally left empty.
}

template<typename ValueType>
std::vector<ValueType> DsMpiDtmcUpperRewardBoundsComputer<ValueType>::computeUpperBounds() {
    STORM_LOG_TRACE("Computing upper reward bounds using DS-MPI.");
    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    sweep();
    ValueType lambda = computeLambda();
    STORM_LOG_TRACE("DS-MPI computed lambda as " << lambda << ".");

    // Finally compute the upper bounds for the states.
    std::vector<ValueType> result(transitionMatrix.getRowGroupCount());
    auto one = storm::utility::one<ValueType>();
    for (storm::storage::sparse::state_type state = 0; state < result.size(); ++state) {
        result[state] = w[state] + (one - p[state]) * lambda;
    }

#ifndef NDEBUG
    ValueType max = storm::utility::zero<ValueType>();
    uint64_t nonZeroCount = 0;
    for (auto const& e : result) {
        if (!storm::utility::isZero(e)) {
            ++nonZeroCount;
            max = std::max(max, e);
        }
    }
    STORM_LOG_TRACE("DS-MPI computed " << nonZeroCount << " non-zero upper bounds and a maximal bound of " << max << ".");
#endif
    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    STORM_LOG_TRACE("Computed upper bounds on rewards in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");
    return result;
}

template<typename ValueType>
ValueType DsMpiDtmcUpperRewardBoundsComputer<ValueType>::computeLambda() const {
    ValueType lambda = storm::utility::zero<ValueType>();
    for (storm::storage::sparse::state_type state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
        lambda = std::max(lambda, computeLambdaForChoice(state));
    }
    return lambda;
}

template<typename ValueType>
ValueType DsMpiDtmcUpperRewardBoundsComputer<ValueType>::computeLambdaForChoice(uint64_t choice) const {
    ValueType localLambda = storm::utility::zero<ValueType>();
    uint64_t state = this->getStateForChoice(choice);

    // Check whether condition (I) or (II) applies.
    ValueType probSum = originalOneStepTargetProbabilities[choice];
    for (auto const& e : transitionMatrix.getRow(choice)) {
        probSum += e.getValue() * p[e.getColumn()];
    }

    if (p[state] < probSum) {
        STORM_LOG_TRACE("Condition (I) does apply for state " << state << " as " << p[state] << " < " << probSum << ".");
        // Condition (I) applies.
        localLambda = probSum - p[state];
        ValueType nominator = originalRewards[choice];
        for (auto const& e : transitionMatrix.getRow(choice)) {
            nominator += e.getValue() * w[e.getColumn()];
        }
        nominator -= w[state];
        localLambda = nominator / localLambda;
    } else {
        STORM_LOG_TRACE("Condition (I) does not apply for state " << state << std::setprecision(30) << " as " << probSum << " <= " << p[state] << ".");
        // Here, condition (II) automatically applies and as the resulting local lambda is 0, we
        // don't need to consider it.

#ifndef NDEBUG
        // Actually check condition (II).
        ValueType rewardSum = originalRewards[choice];
        for (auto const& e : transitionMatrix.getRow(choice)) {
            rewardSum += e.getValue() * w[e.getColumn()];
        }
        // The following is a bit of a hack but I'd prefer not getting the settings into this part of the code for such a simple check.
        storm::utility::ConstantsComparator<ValueType> cc(storm::utility::convertNumber<ValueType>(0.0001));
        STORM_LOG_WARN_COND(w[state] >= rewardSum || cc.isEqual(w[state], rewardSum),
                            "Expected condition (II) to hold in state " << state << ", but " << w[state] << " < " << rewardSum << ".");
        STORM_LOG_WARN_COND(cc.isEqual(probSum, p[state]),
                            "Expected condition (II) to hold in state " << state << ", but " << probSum << " != " << p[state] << ".");
#endif
    }

    return localLambda;
}

template<typename ValueType>
uint64_t DsMpiDtmcUpperRewardBoundsComputer<ValueType>::getStateForChoice(uint64_t choice) const {
    return choice;
}

template<typename ValueType>
class DsMpiDtmcPriorityLess {
   public:
    DsMpiDtmcPriorityLess(DsMpiDtmcUpperRewardBoundsComputer<ValueType> const& dsmpi) : dsmpi(dsmpi) {
        // Intentionally left empty.
    }

    bool operator()(storm::storage::sparse::state_type const& a, storm::storage::sparse::state_type const& b) {
        ValueType pa = dsmpi.targetProbabilities[a];
        ValueType pb = dsmpi.targetProbabilities[b];
        if (pa < pb) {
            return true;
        } else if (pa == pb) {
            return dsmpi.rewards[a] > dsmpi.rewards[b];
        }
        return false;
    }

   private:
    DsMpiDtmcUpperRewardBoundsComputer<ValueType> const& dsmpi;
};

template<typename ValueType>
void DsMpiDtmcUpperRewardBoundsComputer<ValueType>::sweep() {
    // Create a priority queue that allows for easy retrieval of the currently best state.
    storm::storage::ConsecutiveUint64DynamicPriorityQueue<DsMpiDtmcPriorityLess<ValueType>> queue(transitionMatrix.getRowCount(),
                                                                                                  DsMpiDtmcPriorityLess<ValueType>(*this));

    // Keep track of visited states.
    storm::storage::BitVector visited(p.size());

    while (!queue.empty()) {
        // Get first entry in queue.
        storm::storage::sparse::state_type currentState = queue.popTop();

        // Mark state as visited.
        visited.set(currentState);

        // Set weight and probability for the state.
        w[currentState] = rewards[currentState];
        p[currentState] = targetProbabilities[currentState];

        for (auto const& e : backwardTransitions.getRow(currentState)) {
            if (visited.get(e.getColumn())) {
                continue;
            }

            // Update reward/probability values.
            rewards[e.getColumn()] += e.getValue() * w[currentState];
            targetProbabilities[e.getColumn()] += e.getValue() * p[currentState];

            // Increase priority of element.
            queue.increase(e.getColumn());
        }
    }
}

template<typename ValueType>
DsMpiMdpUpperRewardBoundsComputer<ValueType>::DsMpiMdpUpperRewardBoundsComputer(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                std::vector<ValueType> const& rewards,
                                                                                std::vector<ValueType> const& oneStepTargetProbabilities)
    : DsMpiDtmcUpperRewardBoundsComputer<ValueType>(transitionMatrix, rewards, oneStepTargetProbabilities), policy(transitionMatrix.getRowCount()) {
    // Create a mapping from choices to states.
    // Also pick a choice in each state that maximizes the target probability and minimizes the reward.
    choiceToState.resize(transitionMatrix.getRowCount());
    for (uint64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
        uint64_t choice = transitionMatrix.getRowGroupIndices()[state];

        boost::optional<ValueType> minReward;
        ValueType maxProb = storm::utility::zero<ValueType>();

        for (uint64_t row = choice, endRow = transitionMatrix.getRowGroupIndices()[state + 1]; row < endRow; ++row) {
            choiceToState[row] = state;

            if (this->targetProbabilities[row] > maxProb) {
                maxProb = this->targetProbabilities[row];
                minReward = this->rewards[row];
                choice = row;
            } else if (this->targetProbabilities[row] == maxProb && (!minReward || minReward.get() > this->rewards[row])) {
                minReward = this->rewards[row];
                choice = row;
            }
        }

        setChoiceInState(state, choice);
    }
}

template<typename ValueType>
ValueType DsMpiMdpUpperRewardBoundsComputer<ValueType>::computeLambda() const {
    ValueType lambda = storm::utility::zero<ValueType>();
    for (storm::storage::sparse::state_type state = 0; state < this->transitionMatrix.getRowGroupCount(); ++state) {
        lambda = std::max(lambda, this->computeLambdaForChoice(this->getChoiceInState(state)));
    }
    return lambda;
}

template<typename ValueType>
uint64_t DsMpiMdpUpperRewardBoundsComputer<ValueType>::getStateForChoice(uint64_t choice) const {
    return choiceToState[choice];
}

template<typename ValueType>
class DsMpiMdpPriorityLess {
   public:
    DsMpiMdpPriorityLess(DsMpiMdpUpperRewardBoundsComputer<ValueType> const& dsmpi) : dsmpi(dsmpi) {
        // Intentionally left empty.
    }

    bool operator()(storm::storage::sparse::state_type const& a, storm::storage::sparse::state_type const& b) {
        uint64_t choiceA = dsmpi.getChoiceInState(a);
        uint64_t choiceB = dsmpi.getChoiceInState(b);

        ValueType pa = dsmpi.targetProbabilities[choiceA];
        ValueType pb = dsmpi.targetProbabilities[choiceB];
        if (pa < pb) {
            return true;
        } else if (pa == pb) {
            return dsmpi.rewards[choiceB] > dsmpi.rewards[choiceB];
        }
        return false;
    }

   private:
    DsMpiMdpUpperRewardBoundsComputer<ValueType> const& dsmpi;
};

template<typename ValueType>
void DsMpiMdpUpperRewardBoundsComputer<ValueType>::sweep() {
    // Create a priority queue that allows for easy retrieval of the currently best state.
    storm::storage::ConsecutiveUint64DynamicPriorityQueue<DsMpiMdpPriorityLess<ValueType>> queue(this->transitionMatrix.getRowGroupCount(),
                                                                                                 DsMpiMdpPriorityLess<ValueType>(*this));

    // Keep track of visited states.
    storm::storage::BitVector visited(this->transitionMatrix.getRowGroupCount());

    while (!queue.empty()) {
        // Get first entry in queue.
        storm::storage::sparse::state_type currentState = queue.popTop();

        // Mark state as visited.
        visited.set(currentState);

        // Set weight and probability for the state.
        uint64_t choiceInCurrentState = this->getChoiceInState(currentState);
        this->w[currentState] = this->rewards[choiceInCurrentState];
        this->p[currentState] = this->targetProbabilities[choiceInCurrentState];

        for (auto const& choiceEntry : this->backwardTransitions.getRow(currentState)) {
            uint64_t predecessor = this->getStateForChoice(choiceEntry.getColumn());
            if (visited.get(predecessor)) {
                continue;
            }

            // Update reward/probability values.
            this->rewards[choiceEntry.getColumn()] += choiceEntry.getValue() * this->w[currentState];
            this->targetProbabilities[choiceEntry.getColumn()] += choiceEntry.getValue() * this->p[currentState];

            // If the choice is not the one that is currently taken in the predecessor state, we might need
            // to update it.
            uint64_t currentChoiceInPredecessor = this->getChoiceInState(predecessor);
            if (currentChoiceInPredecessor != choiceEntry.getColumn()) {
                // Check whether the updates choice now becomes a better choice in the predecessor state.
                ValueType const& newTargetProbability = this->targetProbabilities[choiceEntry.getColumn()];
                ValueType const& newReward = this->rewards[choiceEntry.getColumn()];
                ValueType const& currentTargetProbability = this->targetProbabilities[this->getChoiceInState(predecessor)];
                ValueType const& currentReward = this->rewards[this->getChoiceInState(predecessor)];

                if (newTargetProbability > currentTargetProbability || (newTargetProbability == currentTargetProbability && newReward < currentReward)) {
                    setChoiceInState(predecessor, choiceEntry.getColumn());
                }
            }

            // Notify the priority of a potential increase of the priority of the element.
            queue.increase(predecessor);
        }
    }
}

template<typename ValueType>
uint64_t DsMpiMdpUpperRewardBoundsComputer<ValueType>::getChoiceInState(uint64_t state) const {
    return policy[state];
}

template<typename ValueType>
void DsMpiMdpUpperRewardBoundsComputer<ValueType>::setChoiceInState(uint64_t state, uint64_t choice) {
    policy[state] = choice;
}

template class DsMpiDtmcUpperRewardBoundsComputer<double>;
template class DsMpiMdpUpperRewardBoundsComputer<double>;

template class DsMpiDtmcUpperRewardBoundsComputer<storm::RationalNumber>;
template class DsMpiMdpUpperRewardBoundsComputer<storm::RationalNumber>;
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
