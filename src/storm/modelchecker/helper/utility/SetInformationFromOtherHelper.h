#pragma once

#include "storm/modelchecker/CheckTask.h"

namespace storm {
namespace modelchecker {
namespace helper {

/*!
 * Forwards relevant information stored in another helper to the given helper
 */
template<typename TargetHelperType, typename SourceHelperType>
void setInformationFromOtherHelperNondeterministic(
    TargetHelperType& targetHelper, SourceHelperType const& sourceHelperType,
    std::function<typename TargetHelperType::StateSet(typename SourceHelperType::StateSet const&)> const& stateSetTransformer) {
    // Relevancy of initial states.
    if (sourceHelperType.hasRelevantStates()) {
        targetHelper.setRelevantStates(stateSetTransformer(sourceHelperType.getRelevantStates()));
    }
    // Value threshold to which the result will be compared
    if (sourceHelperType.isValueThresholdSet()) {
        targetHelper.setValueThreshold(sourceHelperType.getValueThresholdComparisonType(),
                                       storm::numbers::convert<typename TargetHelperType::ValueType>(sourceHelperType.getValueThresholdValue()));
    }
    // Optimization direction
    if (sourceHelperType.isOptimizationDirectionSet()) {
        targetHelper.setOptimizationDirection(sourceHelperType.getOptimizationDirection());
    }
    // Scheduler Production
    targetHelper.setProduceScheduler(sourceHelperType.isProduceSchedulerSet());
}

/*!
 * Forwards relevant information stored in another helper to the given helper
 */
template<typename TargetHelperType, typename SourceHelperType>
void setInformationFromOtherHelperDeterministic(
    TargetHelperType& targetHelper, SourceHelperType const& sourceHelperType,
    std::function<typename TargetHelperType::StateSet(typename SourceHelperType::StateSet const&)> const& stateSetTransformer) {
    // Relevancy of initial states.
    if (sourceHelperType.hasRelevantStates()) {
        targetHelper.setRelevantStates(stateSetTransformer(sourceHelperType.getRelevantStates()));
    }
    // Value threshold to which the result will be compared
    if (sourceHelperType.isValueThresholdSet()) {
        targetHelper.setValueThreshold(sourceHelperType.getValueThresholdComparisonType(),
                                       storm::numbers::convert<typename TargetHelperType::ValueType>(sourceHelperType.getValueThresholdValue()));
    }
}
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm