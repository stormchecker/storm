#include "storm/modelchecker/exploration/ExplorationInformation.h"

#include "storm/environment/exploration/ExplorationEnvironment.h"

#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace exploration_detail {

template<typename StateType, typename ValueType>
ExplorationInformation<StateType, ValueType>::ExplorationInformation(ExplorationEnvironment const& env, storm::OptimizationDirection const& direction,
                                                                     ActionType const& unexploredMarker)
    : unexploredMarker(unexploredMarker),
      optimizationDirection(direction),
      localPrecomputation(false),
      numberOfExplorationStepsUntilPrecomputation(100000),
      numberOfSampledPathsUntilPrecomputation(),
      nextStateHeuristic(storm::modelchecker::exploration_detail::NextStateHeuristic::DifferenceProbabilitySum) {
    localPrecomputation = env.getPrecomputationType() == storm::modelchecker::exploration_detail::PrecomputationType::Local;
    numberOfExplorationStepsUntilPrecomputation = env.getStepsUntilPrecomputation();
    if (env.getSampledPathsUntilPrecomputation()) {
        numberOfSampledPathsUntilPrecomputation = env.getSampledPathsUntilPrecomputation().get();
    }
    nextStateHeuristic = env.getNextStateHeuristic();
}

template<typename StateType, typename ValueType>
typename ExplorationInformation<StateType, ValueType>::const_iterator ExplorationInformation<StateType, ValueType>::findUnexploredState(
    StateType const& state) const {
    return unexploredStates.find(state);
}

template<typename StateType, typename ValueType>
typename ExplorationInformation<StateType, ValueType>::const_iterator ExplorationInformation<StateType, ValueType>::unexploredStatesEnd() const {
    return unexploredStates.end();
}

template<typename StateType, typename ValueType>
void ExplorationInformation<StateType, ValueType>::removeUnexploredState(const_iterator it) {
    unexploredStates.erase(it);
}

template<typename StateType, typename ValueType>
void ExplorationInformation<StateType, ValueType>::addUnexploredState(StateType const& stateId, storm::generator::CompressedState const& compressedState) {
    stateToRowGroupMapping.push_back(unexploredMarker);
    unexploredStates[stateId] = compressedState;
}

template<typename StateType, typename ValueType>
void ExplorationInformation<StateType, ValueType>::assignStateToRowGroup(StateType const& state, ActionType const& rowGroup) {
    stateToRowGroupMapping[state] = rowGroup;
}

template<typename StateType, typename ValueType>
StateType ExplorationInformation<StateType, ValueType>::assignStateToNextRowGroup(StateType const& state) {
    stateToRowGroupMapping[state] = rowGroupIndices.size() - 1;
    return stateToRowGroupMapping[state];
}

template<typename StateType, typename ValueType>
StateType ExplorationInformation<StateType, ValueType>::getNextRowGroup() const {
    return rowGroupIndices.size() - 1;
}

template<typename StateType, typename ValueType>
void ExplorationInformation<StateType, ValueType>::newRowGroup(ActionType const& action) {
    rowGroupIndices.push_back(action);
}

template<typename StateType, typename ValueType>
void ExplorationInformation<StateType, ValueType>::newRowGroup() {
    newRowGroup(matrix.size());
}

template<typename StateType, typename ValueType>
void ExplorationInformation<StateType, ValueType>::terminateCurrentRowGroup() {
    rowGroupIndices.push_back(matrix.size());
}

template<typename StateType, typename ValueType>
void ExplorationInformation<StateType, ValueType>::moveActionToBackOfMatrix(ActionType const& action) {
    matrix.emplace_back(std::move(matrix[action]));
}

template<typename StateType, typename ValueType>
StateType ExplorationInformation<StateType, ValueType>::getActionCount() const {
    return matrix.size();
}

template<typename StateType, typename ValueType>
std::size_t ExplorationInformation<StateType, ValueType>::getNumberOfUnexploredStates() const {
    return unexploredStates.size();
}

template<typename StateType, typename ValueType>
std::size_t ExplorationInformation<StateType, ValueType>::getNumberOfDiscoveredStates() const {
    return stateToRowGroupMapping.size();
}

template<typename StateType, typename ValueType>
StateType const& ExplorationInformation<StateType, ValueType>::getRowGroup(StateType const& state) const {
    return stateToRowGroupMapping[state];
}

template<typename StateType, typename ValueType>
StateType const& ExplorationInformation<StateType, ValueType>::getUnexploredMarker() const {
    return unexploredMarker;
}

template<typename StateType, typename ValueType>
bool ExplorationInformation<StateType, ValueType>::isUnexplored(StateType const& state) const {
    return stateToRowGroupMapping[state] == unexploredMarker;
}

template<typename StateType, typename ValueType>
bool ExplorationInformation<StateType, ValueType>::isTerminal(StateType const& state) const {
    return terminalStates.find(state) != terminalStates.end();
}

template<typename StateType, typename ValueType>
typename ExplorationInformation<StateType, ValueType>::ActionType const& ExplorationInformation<StateType, ValueType>::getStartRowOfGroup(
    StateType const& group) const {
    return rowGroupIndices[group];
}

template<typename StateType, typename ValueType>
std::size_t ExplorationInformation<StateType, ValueType>::getRowGroupSize(StateType const& group) const {
    return rowGroupIndices[group + 1] - rowGroupIndices[group];
}

template<typename StateType, typename ValueType>
bool ExplorationInformation<StateType, ValueType>::onlyOneActionAvailable(StateType const& group) const {
    return getRowGroupSize(group) == 1;
}

template<typename StateType, typename ValueType>
void ExplorationInformation<StateType, ValueType>::addTerminalState(StateType const& state) {
    terminalStates.insert(state);
}

template<typename StateType, typename ValueType>
std::vector<storm::storage::MatrixEntry<StateType, ValueType>>& ExplorationInformation<StateType, ValueType>::getRowOfMatrix(ActionType const& row) {
    return matrix[row];
}

template<typename StateType, typename ValueType>
std::vector<storm::storage::MatrixEntry<StateType, ValueType>> const& ExplorationInformation<StateType, ValueType>::getRowOfMatrix(
    ActionType const& row) const {
    return matrix[row];
}

template<typename StateType, typename ValueType>
void ExplorationInformation<StateType, ValueType>::addActionsToMatrix(std::size_t const& count) {
    matrix.resize(matrix.size() + count);
}

template<typename StateType, typename ValueType>
bool ExplorationInformation<StateType, ValueType>::maximize() const {
    return optimizationDirection == storm::OptimizationDirection::Maximize;
}

template<typename StateType, typename ValueType>
bool ExplorationInformation<StateType, ValueType>::minimize() const {
    return !maximize();
}

template<typename StateType, typename ValueType>
bool ExplorationInformation<StateType, ValueType>::performPrecomputationExcessiveExplorationSteps(
    std::size_t& numberExplorationStepsSinceLastPrecomputation) const {
    bool result = numberExplorationStepsSinceLastPrecomputation > numberOfExplorationStepsUntilPrecomputation;
    if (result) {
        numberExplorationStepsSinceLastPrecomputation = 0;
    }
    return result;
}

template<typename StateType, typename ValueType>
bool ExplorationInformation<StateType, ValueType>::performPrecomputationExcessiveSampledPaths(std::size_t& numberOfSampledPathsSinceLastPrecomputation) const {
    if (!numberOfSampledPathsUntilPrecomputation) {
        return false;
    } else {
        bool result = numberOfSampledPathsSinceLastPrecomputation > numberOfSampledPathsUntilPrecomputation.get();
        if (result) {
            numberOfSampledPathsSinceLastPrecomputation = 0;
        }
        return result;
    }
}

template<typename StateType, typename ValueType>
bool ExplorationInformation<StateType, ValueType>::useLocalPrecomputation() const {
    return localPrecomputation;
}

template<typename StateType, typename ValueType>
bool ExplorationInformation<StateType, ValueType>::useGlobalPrecomputation() const {
    return !useLocalPrecomputation();
}

template<typename StateType, typename ValueType>
storm::modelchecker::exploration_detail::NextStateHeuristic const& ExplorationInformation<StateType, ValueType>::getNextStateHeuristic() const {
    return nextStateHeuristic;
}

template<typename StateType, typename ValueType>
bool ExplorationInformation<StateType, ValueType>::useDifferenceProbabilitySumHeuristic() const {
    return nextStateHeuristic == storm::modelchecker::exploration_detail::NextStateHeuristic::DifferenceProbabilitySum;
}

template<typename StateType, typename ValueType>
bool ExplorationInformation<StateType, ValueType>::useProbabilityHeuristic() const {
    return nextStateHeuristic == storm::modelchecker::exploration_detail::NextStateHeuristic::Probability;
}

template<typename StateType, typename ValueType>
bool ExplorationInformation<StateType, ValueType>::useUniformHeuristic() const {
    return nextStateHeuristic == storm::modelchecker::exploration_detail::NextStateHeuristic::Uniform;
}

template<typename StateType, typename ValueType>
storm::OptimizationDirection const& ExplorationInformation<StateType, ValueType>::getOptimizationDirection() const {
    return optimizationDirection;
}

template<typename StateType, typename ValueType>
void ExplorationInformation<StateType, ValueType>::setOptimizationDirection(storm::OptimizationDirection const& direction) {
    optimizationDirection = direction;
}

template class ExplorationInformation<uint32_t, double>;
}  // namespace exploration_detail
}  // namespace modelchecker
}  // namespace storm
