#include "storm-dft/builder/DftExplorationHeuristic.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotImplementedException.h"

namespace storm::dft {
namespace builder {

template<>
double DFTExplorationHeuristicProbability<double>::getPriority() const {
    return probability;
}

template<typename ValueType>
double DFTExplorationHeuristicProbability<ValueType>::getPriority() const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Heuristic 'probability' does not work for this data type.");
}

template<>
double DFTExplorationHeuristicBoundDifference<double>::getPriority() const {
    double difference = lowerBound - upperBound;  // Lower bound is larger than upper bound
    difference = 2 * difference / (upperBound + lowerBound);
    return probability * difference;
}

template<typename ValueType>
double DFTExplorationHeuristicBoundDifference<ValueType>::getPriority() const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Heuristic 'bound difference' does not work for this data type.");
}

// Instantiate templates.
template class DFTExplorationHeuristicDepth<double>;
template class DFTExplorationHeuristicProbability<double>;
template class DFTExplorationHeuristicBoundDifference<double>;

template class DFTExplorationHeuristicDepth<storm::RationalFunction>;
template class DFTExplorationHeuristicProbability<storm::RationalFunction>;
template class DFTExplorationHeuristicBoundDifference<storm::RationalFunction>;

}  // namespace builder
}  // namespace storm::dft
