#pragma once

#include <utility>
#include <vector>

#include "storm/solver/OptimizationDirection.h"

#include "storm/utility/constants.h"

namespace storm {
namespace modelchecker {
namespace exploration_detail {

template<typename StateType, typename ValueType>
class ExplorationInformation;

template<typename StateType, typename ValueType>
class Bounds {
   public:
    typedef StateType ActionType;

    std::pair<ValueType, ValueType> getBoundsForState(StateType const& state, ExplorationInformation<StateType, ValueType> const& explorationInformation) const;

    ValueType getLowerBoundForState(StateType const& state, ExplorationInformation<StateType, ValueType> const& explorationInformation) const;

    ValueType const& getLowerBoundForRowGroup(StateType const& rowGroup) const;

    ValueType getUpperBoundForState(StateType const& state, ExplorationInformation<StateType, ValueType> const& explorationInformation) const;

    ValueType const& getUpperBoundForRowGroup(StateType const& rowGroup) const;

    std::pair<ValueType, ValueType> const& getBoundsForAction(ActionType const& action) const;

    ValueType const& getLowerBoundForAction(ActionType const& action) const;

    ValueType const& getUpperBoundForAction(ActionType const& action) const;

    ValueType const& getBoundForAction(storm::OptimizationDirection const& direction, ActionType const& action) const;

    ValueType getDifferenceOfStateBounds(StateType const& state, ExplorationInformation<StateType, ValueType> const& explorationInformation) const;

    void initializeBoundsForNextState(std::pair<ValueType, ValueType> const& vals = std::pair<ValueType, ValueType>(storm::utility::zero<ValueType>(),
                                                                                                                    storm::utility::one<ValueType>()));

    void initializeBoundsForNextAction(std::pair<ValueType, ValueType> const& vals = std::pair<ValueType, ValueType>(storm::utility::zero<ValueType>(),
                                                                                                                     storm::utility::one<ValueType>()));

    void setLowerBoundForState(StateType const& state, ExplorationInformation<StateType, ValueType> const& explorationInformation, ValueType const& value);

    void setLowerBoundForRowGroup(StateType const& group, ValueType const& value);

    void setUpperBoundForState(StateType const& state, ExplorationInformation<StateType, ValueType> const& explorationInformation, ValueType const& value);

    void setUpperBoundForRowGroup(StateType const& group, ValueType const& value);

    void setBoundsForAction(ActionType const& action, std::pair<ValueType, ValueType> const& values);

    void setBoundsForState(StateType const& state, ExplorationInformation<StateType, ValueType> const& explorationInformation,
                           std::pair<ValueType, ValueType> const& values);

    void setBoundsForRowGroup(StateType const& rowGroup, std::pair<ValueType, ValueType> const& values);

    bool setLowerBoundOfStateIfGreaterThanOld(StateType const& state, ExplorationInformation<StateType, ValueType> const& explorationInformation,
                                              ValueType const& newLowerValue);

    bool setUpperBoundOfStateIfLessThanOld(StateType const& state, ExplorationInformation<StateType, ValueType> const& explorationInformation,
                                           ValueType const& newUpperValue);

   private:
    std::vector<std::pair<ValueType, ValueType>> boundsPerState;
    std::vector<std::pair<ValueType, ValueType>> boundsPerAction;
};

}  // namespace exploration_detail
}  // namespace modelchecker
}  // namespace storm
