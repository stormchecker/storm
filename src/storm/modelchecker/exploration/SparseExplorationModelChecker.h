#pragma once

#include <random>

#include "storm/modelchecker/AbstractModelChecker.h"

#include "storm/storage/prism/Program.h"

#include "storm/generator/CompressedState.h"
#include "storm/generator/VariableInformation.h"

#include "storm/utility/ConstantsComparator.h"

namespace storm {

class Environment;

namespace storage {
class MaximalEndComponent;
template<typename V>
class SparseMatrix;
}  // namespace storage
namespace prism {
class Program;
}

namespace modelchecker {
namespace exploration_detail {
template<typename StateType, typename ValueType>
class StateGeneration;
template<typename StateType, typename ValueType>
class ExplorationInformation;
template<typename StateType, typename ValueType>
class Bounds;
template<typename StateType, typename ValueType>
struct Statistics;
}  // namespace exploration_detail

using namespace exploration_detail;

template<typename ModelType, typename StateType = uint32_t>
class SparseExplorationModelChecker : public AbstractModelChecker<ModelType> {
   public:
    typedef typename ModelType::ValueType ValueType;
    typedef StateType ActionType;
    typedef std::vector<std::pair<StateType, ActionType>> StateActionStack;

    SparseExplorationModelChecker(storm::prism::Program const& program);

    static bool canHandleStatic(CheckTask<storm::logic::Formula, ValueType> const& checkTask);

    virtual bool canHandle(CheckTask<storm::logic::Formula, ValueType> const& checkTask) const override;

    virtual std::unique_ptr<CheckResult> computeUntilProbabilities(Environment const& env,
                                                                   CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) override;

   private:
    std::tuple<StateType, ValueType, ValueType> performExploration(StateGeneration<StateType, ValueType>& stateGeneration,
                                                                   ExplorationInformation<StateType, ValueType>& explorationInformation) const;

    bool samplePathFromInitialState(StateGeneration<StateType, ValueType>& stateGeneration,
                                    ExplorationInformation<StateType, ValueType>& explorationInformation, StateActionStack& stack,
                                    Bounds<StateType, ValueType>& bounds, Statistics<StateType, ValueType>& stats) const;

    bool exploreState(StateGeneration<StateType, ValueType>& stateGeneration, StateType const& currentStateId,
                      storm::generator::CompressedState const& currentState, ExplorationInformation<StateType, ValueType>& explorationInformation,
                      Bounds<StateType, ValueType>& bounds, Statistics<StateType, ValueType>& stats) const;

    ActionType sampleActionOfState(StateType const& currentStateId, ExplorationInformation<StateType, ValueType> const& explorationInformation,
                                   Bounds<StateType, ValueType>& bounds) const;

    StateType sampleSuccessorFromAction(ActionType const& chosenAction, ExplorationInformation<StateType, ValueType> const& explorationInformation,
                                        Bounds<StateType, ValueType> const& bounds) const;

    bool performPrecomputation(StateActionStack const& stack, ExplorationInformation<StateType, ValueType>& explorationInformation,
                               Bounds<StateType, ValueType>& bounds, Statistics<StateType, ValueType>& stats) const;

    void collapseMec(storm::storage::MaximalEndComponent const& mec, std::vector<StateType> const& relevantStates,
                     storm::storage::SparseMatrix<ValueType> const& relevantStatesMatrix, ExplorationInformation<StateType, ValueType>& explorationInformation,
                     Bounds<StateType, ValueType>& bounds) const;

    void updateProbabilityBoundsAlongSampledPath(StateActionStack& stack, ExplorationInformation<StateType, ValueType> const& explorationInformation,
                                                 Bounds<StateType, ValueType>& bounds) const;

    void updateProbabilityOfAction(StateType const& state, ActionType const& action, ExplorationInformation<StateType, ValueType> const& explorationInformation,
                                   Bounds<StateType, ValueType>& bounds) const;

    std::pair<ValueType, ValueType> computeBoundsOfAction(ActionType const& action, ExplorationInformation<StateType, ValueType> const& explorationInformation,
                                                          Bounds<StateType, ValueType> const& bounds) const;
    ValueType computeBoundOverAllOtherActions(storm::OptimizationDirection const& direction, StateType const& state, ActionType const& action,
                                              ExplorationInformation<StateType, ValueType> const& explorationInformation,
                                              Bounds<StateType, ValueType> const& bounds) const;
    std::pair<ValueType, ValueType> computeBoundsOfState(StateType const& currentStateId,
                                                         ExplorationInformation<StateType, ValueType> const& explorationInformation,
                                                         Bounds<StateType, ValueType> const& bounds) const;
    ValueType computeLowerBoundOfAction(ActionType const& action, ExplorationInformation<StateType, ValueType> const& explorationInformation,
                                        Bounds<StateType, ValueType> const& bounds) const;
    ValueType computeUpperBoundOfAction(ActionType const& action, ExplorationInformation<StateType, ValueType> const& explorationInformation,
                                        Bounds<StateType, ValueType> const& bounds) const;

    std::pair<ValueType, ValueType> getLowestBounds(storm::OptimizationDirection const& direction) const;
    ValueType getLowestBound(storm::OptimizationDirection const& direction) const;
    std::pair<ValueType, ValueType> combineBounds(storm::OptimizationDirection const& direction, std::pair<ValueType, ValueType> const& bounds1,
                                                  std::pair<ValueType, ValueType> const& bounds2) const;

    // The program that defines the model to check.
    storm::prism::Program program;

    // The random number generator.
    mutable std::default_random_engine randomGenerator;

    // A comparator used to determine whether values are equal.
    storm::utility::ConstantsComparator<ValueType> comparator;
};
}  // namespace modelchecker
}  // namespace storm
