#pragma once

#include "storm/generator/CompressedState.h"
#include "storm/generator/PrismNextStateGenerator.h"

#include "storm/storage/sparse/StateStorage.h"

namespace storm {
namespace generator {
template<typename ValueType, typename StateType>
class PrismNextStateGenerator;
}

namespace modelchecker {
namespace exploration_detail {

template<typename StateType, typename ValueType>
class ExplorationInformation;

template<typename StateType, typename ValueType>
class StateGeneration {
   public:
    StateGeneration(storm::prism::Program const& program, ExplorationInformation<StateType, ValueType>& explorationInformation,
                    storm::expressions::Expression const& conditionStateExpression, storm::expressions::Expression const& targetStateExpression);

    void load(storm::generator::CompressedState const& state);

    std::vector<StateType> getInitialStates();

    storm::generator::StateBehavior<ValueType, StateType> expand();

    void computeInitialStates();

    StateType getFirstInitialState() const;

    std::size_t getNumberOfInitialStates() const;

    bool isConditionState() const;

    bool isTargetState() const;

   private:
    storm::generator::PrismNextStateGenerator<ValueType, StateType> generator;
    std::function<StateType(storm::generator::CompressedState const&)> stateToIdCallback;

    storm::storage::sparse::StateStorage<StateType> stateStorage;

    storm::expressions::Expression conditionStateExpression;
    storm::expressions::Expression targetStateExpression;
};

}  // namespace exploration_detail
}  // namespace modelchecker
}  // namespace storm
