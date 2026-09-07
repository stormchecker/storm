#include "StateGeneration.h"

#include "storm/modelchecker/exploration/ExplorationInformation.h"
#include "storm/storage/expressions/ExpressionEvaluator.h"

namespace storm {
namespace modelchecker {
namespace exploration_detail {

template<typename StateType, typename ValueType>
StateGeneration<StateType, ValueType>::StateGeneration(storm::prism::Program const& program,
                                                       ExplorationInformation<StateType, ValueType>& explorationInformation,
                                                       storm::expressions::Expression const& conditionStateExpression,
                                                       storm::expressions::Expression const& targetStateExpression)
    : generator(program),
      stateStorage(generator.getStateSize()),
      conditionStateExpression(conditionStateExpression),
      targetStateExpression(targetStateExpression) {
    stateToIdCallback = [&explorationInformation, this](storm::generator::CompressedState const& state) -> StateType {
        StateType newIndex = stateStorage.getNumberOfStates();

        // Check, if the state was already registered.
        std::pair<StateType, std::size_t> actualIndexBucketPair = stateStorage.stateToId.findOrAddAndGetBucket(state, newIndex);

        if (actualIndexBucketPair.first == newIndex) {
            explorationInformation.addUnexploredState(newIndex, state);
        }

        return actualIndexBucketPair.first;
    };
}

template<typename StateType, typename ValueType>
void StateGeneration<StateType, ValueType>::load(storm::generator::CompressedState const& state) {
    generator.load(state);
}

template<typename StateType, typename ValueType>
std::vector<StateType> StateGeneration<StateType, ValueType>::getInitialStates() {
    return stateStorage.initialStateIndices;
}

template<typename StateType, typename ValueType>
storm::generator::StateBehavior<ValueType, StateType> StateGeneration<StateType, ValueType>::expand() {
    return generator.expand(stateToIdCallback);
}

template<typename StateType, typename ValueType>
bool StateGeneration<StateType, ValueType>::isConditionState() const {
    return generator.satisfies(conditionStateExpression);
}

template<typename StateType, typename ValueType>
bool StateGeneration<StateType, ValueType>::isTargetState() const {
    return generator.satisfies(targetStateExpression);
}

template<typename StateType, typename ValueType>
void StateGeneration<StateType, ValueType>::computeInitialStates() {
    stateStorage.initialStateIndices = generator.getInitialStates(stateToIdCallback);
}

template<typename StateType, typename ValueType>
StateType StateGeneration<StateType, ValueType>::getFirstInitialState() const {
    return stateStorage.initialStateIndices.front();
}

template<typename StateType, typename ValueType>
std::size_t StateGeneration<StateType, ValueType>::getNumberOfInitialStates() const {
    return stateStorage.initialStateIndices.size();
}

template class StateGeneration<uint32_t, double>;
}  // namespace exploration_detail
}  // namespace modelchecker
}  // namespace storm
