#pragma once

#include <limits>
#include <unordered_map>
#include <vector>

#include <boost/optional.hpp>

#include "storm/solver/OptimizationDirection.h"

#include "storm/generator/CompressedState.h"

#include "storm/storage/BoostTypes.h"
#include "storm/storage/SparseMatrix.h"

#include "storm/settings/modules/ExplorationSettings.h"

namespace storm {
namespace modelchecker {
namespace exploration_detail {
template<typename StateType, typename ValueType>
class ExplorationInformation {
   public:
    typedef StateType ActionType;
    typedef storm::storage::FlatSet<StateType> StateSet;
    typedef std::unordered_map<StateType, storm::generator::CompressedState> IdToStateMap;
    typedef typename IdToStateMap::const_iterator const_iterator;
    typedef std::vector<std::vector<storm::storage::MatrixEntry<StateType, ValueType>>> MatrixType;

    ExplorationInformation(storm::OptimizationDirection const& direction, ActionType const& unexploredMarker = std::numeric_limits<ActionType>::max());

    const_iterator findUnexploredState(StateType const& state) const;

    const_iterator unexploredStatesEnd() const;

    void removeUnexploredState(const_iterator it);

    void addUnexploredState(StateType const& stateId, storm::generator::CompressedState const& compressedState);

    void assignStateToRowGroup(StateType const& state, ActionType const& rowGroup);

    StateType assignStateToNextRowGroup(StateType const& state);

    StateType getNextRowGroup() const;

    void newRowGroup(ActionType const& action);

    void newRowGroup();

    void terminateCurrentRowGroup();

    void moveActionToBackOfMatrix(ActionType const& action);

    StateType getActionCount() const;

    std::size_t getNumberOfUnexploredStates() const;

    std::size_t getNumberOfDiscoveredStates() const;

    StateType const& getRowGroup(StateType const& state) const;

    StateType const& getUnexploredMarker() const;

    bool isUnexplored(StateType const& state) const;

    bool isTerminal(StateType const& state) const;

    ActionType const& getStartRowOfGroup(StateType const& group) const;

    std::size_t getRowGroupSize(StateType const& group) const;

    bool onlyOneActionAvailable(StateType const& group) const;

    void addTerminalState(StateType const& state);

    std::vector<storm::storage::MatrixEntry<StateType, ValueType>>& getRowOfMatrix(ActionType const& row);

    std::vector<storm::storage::MatrixEntry<StateType, ValueType>> const& getRowOfMatrix(ActionType const& row) const;

    void addActionsToMatrix(std::size_t const& count);

    bool maximize() const;

    bool minimize() const;

    bool performPrecomputationExcessiveExplorationSteps(std::size_t& numberExplorationStepsSinceLastPrecomputation) const;

    bool performPrecomputationExcessiveSampledPaths(std::size_t& numberOfSampledPathsSinceLastPrecomputation) const;

    bool useLocalPrecomputation() const;

    bool useGlobalPrecomputation() const;

    storm::settings::modules::ExplorationSettings::NextStateHeuristic const& getNextStateHeuristic() const;

    bool useDifferenceProbabilitySumHeuristic() const;

    bool useProbabilityHeuristic() const;

    bool useUniformHeuristic() const;

    storm::OptimizationDirection const& getOptimizationDirection() const;

    void setOptimizationDirection(storm::OptimizationDirection const& direction);

   private:
    MatrixType matrix;
    std::vector<StateType> rowGroupIndices;

    std::vector<StateType> stateToRowGroupMapping;
    StateType unexploredMarker;
    IdToStateMap unexploredStates;

    storm::OptimizationDirection optimizationDirection;
    StateSet terminalStates;

    bool localPrecomputation;
    std::size_t numberOfExplorationStepsUntilPrecomputation;
    boost::optional<std::size_t> numberOfSampledPathsUntilPrecomputation;

    storm::settings::modules::ExplorationSettings::NextStateHeuristic nextStateHeuristic;
};
}  // namespace exploration_detail
}  // namespace modelchecker
}  // namespace storm
