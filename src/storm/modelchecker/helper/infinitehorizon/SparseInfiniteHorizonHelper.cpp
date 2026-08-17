#include "storm/modelchecker/helper/infinitehorizon/SparseInfiniteHorizonHelper.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/environment/solver/LongRunAverageSolverEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/utility/ProgressMeasurement.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType, bool Nondeterministic>
SparseInfiniteHorizonHelper<ValueType, Nondeterministic>::SparseInfiniteHorizonHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix)
    : _transitionMatrix(transitionMatrix),
      _markovianStates(nullptr),
      _exitRates(nullptr),
      _backwardTransitions(nullptr),
      _longRunComponentDecomposition(nullptr) {
    // Intentionally left empty.
}

template<typename ValueType, bool Nondeterministic>
SparseInfiniteHorizonHelper<ValueType, Nondeterministic>::SparseInfiniteHorizonHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                      storm::storage::BitVector const& markovianStates,
                                                                                      std::vector<ValueType> const& exitRates)
    : _transitionMatrix(transitionMatrix),
      _markovianStates(&markovianStates),
      _exitRates(&exitRates),
      _backwardTransitions(nullptr),
      _longRunComponentDecomposition(nullptr) {
    // Intentionally left empty.
}

template<typename ValueType, bool Nondeterministic>
SparseInfiniteHorizonHelper<ValueType, Nondeterministic>::SparseInfiniteHorizonHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                      std::vector<ValueType> const& exitRates)
    : _transitionMatrix(transitionMatrix),
      _markovianStates(nullptr),
      _exitRates(&exitRates),
      _backwardTransitions(nullptr),
      _longRunComponentDecomposition(nullptr) {
    // Intentionally left empty.
}

template<typename ValueType, bool Nondeterministic>
void SparseInfiniteHorizonHelper<ValueType, Nondeterministic>::provideBackwardTransitions(storm::storage::SparseMatrix<ValueType> const& backwardTransitions) {
    STORM_LOG_WARN_COND(_backwardTransitions == nullptr, "Backwards transitions were provided but they were already computed or provided before.");
    _backwardTransitions = &backwardTransitions;
}

template<typename ValueType, bool Nondeterministic>
void SparseInfiniteHorizonHelper<ValueType, Nondeterministic>::provideLongRunComponentDecomposition(
    storm::storage::Decomposition<LongRunComponentType> const& decomposition) {
    STORM_LOG_WARN_COND(_longRunComponentDecomposition == nullptr,
                        "Long Run Component Decomposition was provided but it was already computed or provided before.");
    _longRunComponentDecomposition = &decomposition;
}

template<typename ValueType, bool Nondeterministic>
std::vector<ValueType> SparseInfiniteHorizonHelper<ValueType, Nondeterministic>::computeLongRunAverageProbabilities(
    Environment const& env, storm::storage::BitVector const& psiStates) {
    return computeLongRunAverageValues(
        env, [&psiStates](uint64_t stateIndex) { return psiStates.get(stateIndex) ? storm::utility::one<ValueType>() : storm::utility::zero<ValueType>(); },
        [](uint64_t) { return storm::utility::zero<ValueType>(); });
}

template<typename ValueType, bool Nondeterministic>
std::vector<ValueType> SparseInfiniteHorizonHelper<ValueType, Nondeterministic>::computeLongRunAverageRewards(
    Environment const& env, storm::models::sparse::StandardRewardModel<ValueType> const& rewardModel) {
    ValueGetter stateRewardsGetter;
    if (rewardModel.hasStateRewards()) {
        stateRewardsGetter = [&rewardModel](uint64_t stateIndex) { return rewardModel.getStateReward(stateIndex); };
    } else {
        stateRewardsGetter = [](uint64_t) { return storm::utility::zero<ValueType>(); };
    }
    ValueGetter actionRewardsGetter;
    if (rewardModel.hasStateActionRewards() || rewardModel.hasTransitionRewards()) {
        if (rewardModel.hasTransitionRewards()) {
            actionRewardsGetter = [&](uint64_t globalChoiceIndex) {
                return rewardModel.getStateActionAndTransitionReward(globalChoiceIndex, this->_transitionMatrix);
            };
        } else {
            actionRewardsGetter = [&](uint64_t globalChoiceIndex) { return rewardModel.getStateActionReward(globalChoiceIndex); };
        }
    } else {
        actionRewardsGetter = [](uint64_t) { return storm::utility::zero<ValueType>(); };
    }

    return computeLongRunAverageValues(env, stateRewardsGetter, actionRewardsGetter);
}

template<typename ValueType, bool Nondeterministic>
std::vector<ValueType> SparseInfiniteHorizonHelper<ValueType, Nondeterministic>::computeLongRunAverageValues(Environment const& env,
                                                                                                             std::vector<ValueType> const* stateValues,
                                                                                                             std::vector<ValueType> const* actionValues) {
    ValueGetter stateValuesGetter;
    if (stateValues) {
        stateValuesGetter = [&stateValues](uint64_t stateIndex) { return (*stateValues)[stateIndex]; };
    } else {
        stateValuesGetter = [](uint64_t) { return storm::utility::zero<ValueType>(); };
    }
    ValueGetter actionValuesGetter;
    if (actionValues) {
        actionValuesGetter = [&actionValues](uint64_t globalChoiceIndex) { return (*actionValues)[globalChoiceIndex]; };
    } else {
        actionValuesGetter = [](uint64_t) { return storm::utility::zero<ValueType>(); };
    }

    return computeLongRunAverageValues(env, stateValuesGetter, actionValuesGetter);
}

template<typename ValueType, bool Nondeterministic>
std::vector<ValueType> SparseInfiniteHorizonHelper<ValueType, Nondeterministic>::computeLongRunAverageValues(Environment const& env,
                                                                                                             ValueGetter const& stateRewardsGetter,
                                                                                                             ValueGetter const& actionRewardsGetter) {
    // We will compute the long run average value for each MEC individually and then set-up an Equation system to compute the value also at non-mec states.
    // For a description of this approach see, e.g., Guck et al.: Modelling and Analysis of Markov Reward Automata (ATVA'14),
    // https://doi.org/10.1007/978-3-319-11936-6_13

    // Prepare an environment for the underlying solvers.
    auto underlyingSolverEnvironment = env;
    if (env.solver().isForceSoundness()) {
        // For sound computations, the error in the MECS plus the error in the remaining system should not exceed the user defined precsion.
        storm::RationalNumber newPrecision = env.solver().lra().getPrecision() / storm::utility::convertNumber<storm::RationalNumber>(2);
        underlyingSolverEnvironment.solver().minMax().setPrecision(newPrecision);
        underlyingSolverEnvironment.solver().minMax().setRelativeTerminationCriterion(env.solver().lra().getRelativeTerminationCriterion());
        underlyingSolverEnvironment.solver().setLinearEquationSolverPrecision(newPrecision, env.solver().lra().getRelativeTerminationCriterion());
        underlyingSolverEnvironment.solver().lra().setPrecision(newPrecision);
    }

    // If requested, allocate memory for the choices made
    if (Nondeterministic && this->isProduceSchedulerSet()) {
        if (!_producedOptimalChoices.is_initialized()) {
            _producedOptimalChoices.emplace();
        }
        _producedOptimalChoices->resize(_transitionMatrix.getRowGroupCount());
    }
    STORM_LOG_ASSERT(Nondeterministic || !this->isProduceSchedulerSet(), "Scheduler production enabled for deterministic model.");

    // Decompose the model to their bottom components (MECS or BSCCS)
    createDecomposition();

    // Compute the long-run average for all components in isolation.
    // Set up some logging
    std::string const componentString = (Nondeterministic ? std::string("Maximal end") : std::string("Bottom strongly connected")) +
                                        (_longRunComponentDecomposition->size() == 1 ? std::string(" component") : std::string(" components"));
    storm::utility::ProgressMeasurement progress(componentString);
    progress.setMaxCount(_longRunComponentDecomposition->size());
    progress.startNewMeasurement(0);
    STORM_LOG_INFO("Computing long run average values for " << _longRunComponentDecomposition->size() << " " << componentString << " individually...");
    std::vector<ValueType> componentLraValues;
    componentLraValues.reserve(_longRunComponentDecomposition->size());
    for (auto const& c : *_longRunComponentDecomposition) {
        componentLraValues.push_back(computeLraForComponent(underlyingSolverEnvironment, stateRewardsGetter, actionRewardsGetter, c));
        progress.updateProgress(componentLraValues.size());
    }

    // Solve the resulting SSP where end components are collapsed into single auxiliary states
    STORM_LOG_INFO("Solving stochastic shortest path problem.");
    return buildAndSolveSsp(underlyingSolverEnvironment, componentLraValues);
}

template<typename ValueType, bool Nondeterministic>
bool SparseInfiniteHorizonHelper<ValueType, Nondeterministic>::isContinuousTime() const {
    STORM_LOG_ASSERT((_markovianStates == nullptr) || (_exitRates != nullptr), "Inconsistent information given: Have Markovian states but no exit rates.");
    return _exitRates != nullptr;
}

template<typename ValueType, bool Nondeterministic>
void SparseInfiniteHorizonHelper<ValueType, Nondeterministic>::createBackwardTransitions() {
    if (this->_backwardTransitions == nullptr) {
        this->_computedBackwardTransitions =
            std::make_unique<storm::storage::SparseMatrix<ValueType>>(this->_transitionMatrix.transpose(true, false));  // will drop zeroes
        this->_backwardTransitions = this->_computedBackwardTransitions.get();
    }
}

template class SparseInfiniteHorizonHelper<double, true>;
template class SparseInfiniteHorizonHelper<storm::RationalNumber, true>;

template class SparseInfiniteHorizonHelper<double, false>;
template class SparseInfiniteHorizonHelper<storm::RationalNumber, false>;
template class SparseInfiniteHorizonHelper<storm::RationalFunction, false>;

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm