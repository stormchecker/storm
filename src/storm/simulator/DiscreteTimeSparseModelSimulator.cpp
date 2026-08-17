#include "storm/simulator/DiscreteTimeSparseModelSimulator.h"
#include "storm/models/sparse/Model.h"

namespace storm {
namespace simulator {
template<typename ValueType, typename RewardModelType>
DiscreteTimeSparseModelSimulator<ValueType, RewardModelType>::DiscreteTimeSparseModelSimulator(
    storm::models::sparse::Model<ValueType, RewardModelType> const& model)
    : model(model), currentState(*model.getInitialStates().begin()), zeroRewards(model.getNumberOfRewardModels(), storm::utility::zero<ValueType>()) {
    STORM_LOG_WARN_COND(model.getInitialStates().getNumberOfSetBits() == 1,
                        "The model has multiple initial states. This simulator assumes it starts from the initial state with the lowest index.");
    lastRewards = zeroRewards;
    uint64_t i = 0;
    for (auto const& rewModPair : model.getRewardModels()) {
        if (rewModPair.second.hasStateRewards()) {
            lastRewards[i] += rewModPair.second.getStateReward(currentState);
        }
        ++i;
    }
}

template<typename ValueType, typename RewardModelType>
void DiscreteTimeSparseModelSimulator<ValueType, RewardModelType>::setSeed(uint64_t seed) {
    generator = storm::utility::RandomProbabilityGenerator<ValueType>(seed);
}

template<typename ValueType, typename RewardModelType>
bool DiscreteTimeSparseModelSimulator<ValueType, RewardModelType>::randomStep() {
    // TODO random_uint is slow
    if (model.getTransitionMatrix().getRowGroupSize(currentState) == 0) {
        return false;
    }
    return step(generator.random_uint(0, model.getTransitionMatrix().getRowGroupSize(currentState) - 1));
}

template<typename ValueType, typename RewardModelType>
bool DiscreteTimeSparseModelSimulator<ValueType, RewardModelType>::step(uint64_t action) {
    // TODO lots of optimization potential.
    //  E.g., do not sample random numbers if there is only a single transition
    lastRewards = zeroRewards;
    ValueType probability = generator.random();
    STORM_LOG_ASSERT(action < model.getTransitionMatrix().getRowGroupSize(currentState), "Action index higher than number of actions.");
    uint64_t row = model.getTransitionMatrix().getRowGroupIndices()[currentState] + action;
    uint64_t i = 0;
    for (auto const& rewModPair : model.getRewardModels()) {
        if (rewModPair.second.hasStateActionRewards()) {
            lastRewards[i] += rewModPair.second.getStateActionReward(row);
        }
        ++i;
    }
    ValueType sum = storm::utility::zero<ValueType>();
    for (auto const& entry : model.getTransitionMatrix().getRow(row)) {
        sum += entry.getValue();
        if (sum >= probability) {
            currentState = entry.getColumn();
            i = 0;
            for (auto const& rewModPair : model.getRewardModels()) {
                if (rewModPair.second.hasStateRewards()) {
                    lastRewards[i] += rewModPair.second.getStateReward(currentState);
                }
                ++i;
            }
            return true;
        }
    }
    // This position should never be reached
    return false;
}

template<typename ValueType, typename RewardModelType>
uint64_t DiscreteTimeSparseModelSimulator<ValueType, RewardModelType>::getCurrentState() const {
    return currentState;
}

template<typename ValueType, typename RewardModelType>
bool DiscreteTimeSparseModelSimulator<ValueType, RewardModelType>::resetToInitial() {
    currentState = *model.getInitialStates().begin();
    lastRewards = zeroRewards;
    uint64_t i = 0;
    for (auto const& rewModPair : model.getRewardModels()) {
        if (rewModPair.second.hasStateRewards()) {
            lastRewards[i] += rewModPair.second.getStateReward(currentState);
        }
        ++i;
    }
    return true;
}

template<typename ValueType, typename RewardModelType>
std::vector<ValueType> const& DiscreteTimeSparseModelSimulator<ValueType, RewardModelType>::getLastRewards() const {
    return lastRewards;
}

template class DiscreteTimeSparseModelSimulator<double>;
template class DiscreteTimeSparseModelSimulator<storm::RationalNumber>;

}  // namespace simulator
}  // namespace storm
