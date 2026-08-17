#include "storm/solver/GameSolver.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/StandardGameSolver.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

template<typename ValueType>
GameSolver<ValueType>::GameSolver() : trackSchedulers(false), uniqueSolution(false), cachingEnabled(false) {
    // Intentionally left empty
}

template<typename ValueType>
void GameSolver<ValueType>::setTrackSchedulers(bool value) {
    trackSchedulers = value;
    if (!trackSchedulers) {
        player1SchedulerChoices = boost::none;
        player2SchedulerChoices = boost::none;
    }
}

template<typename ValueType>
bool GameSolver<ValueType>::isTrackSchedulersSet() const {
    return trackSchedulers;
}

template<typename ValueType>
bool GameSolver<ValueType>::hasSchedulers() const {
    return player1SchedulerChoices.is_initialized() && player2SchedulerChoices.is_initialized();
}

template<typename ValueType>
storm::storage::Scheduler<ValueType> GameSolver<ValueType>::computePlayer1Scheduler() const {
    STORM_LOG_THROW(hasSchedulers(), storm::exceptions::IllegalFunctionCallException, "Cannot retrieve player 1 scheduler, because none was generated.");
    storm::storage::Scheduler<ValueType> result(player1SchedulerChoices->size());
    uint_fast64_t state = 0;
    for (auto const& schedulerChoice : player1SchedulerChoices.get()) {
        result.setChoice(schedulerChoice, state);
        ++state;
    }
    return result;
}

template<typename ValueType>
storm::storage::Scheduler<ValueType> GameSolver<ValueType>::computePlayer2Scheduler() const {
    STORM_LOG_THROW(hasSchedulers(), storm::exceptions::IllegalFunctionCallException, "Cannot retrieve player 2 scheduler, because none was generated.");
    storm::storage::Scheduler<ValueType> result(player2SchedulerChoices->size());
    uint_fast64_t state = 0;
    for (auto const& schedulerChoice : player2SchedulerChoices.get()) {
        result.setChoice(schedulerChoice, state);
        ++state;
    }
    return result;
}

template<typename ValueType>
std::vector<uint_fast64_t> const& GameSolver<ValueType>::getPlayer1SchedulerChoices() const {
    STORM_LOG_THROW(hasSchedulers(), storm::exceptions::IllegalFunctionCallException,
                    "Cannot retrieve player 1 scheduler choices, because they were not generated.");
    return player1SchedulerChoices.get();
}

template<typename ValueType>
std::vector<uint_fast64_t> const& GameSolver<ValueType>::getPlayer2SchedulerChoices() const {
    STORM_LOG_THROW(hasSchedulers(), storm::exceptions::IllegalFunctionCallException,
                    "Cannot retrieve player 2 scheduler choices, because they were not generated.");
    return player2SchedulerChoices.get();
}

template<typename ValueType>
void GameSolver<ValueType>::setSchedulerHints(std::vector<uint_fast64_t>&& player1Choices, std::vector<uint_fast64_t>&& player2Choices) {
    this->player1ChoicesHint = std::move(player1Choices);
    this->player2ChoicesHint = std::move(player2Choices);
}

template<typename ValueType>
bool GameSolver<ValueType>::hasSchedulerHints() const {
    return player1ChoicesHint.is_initialized() && player2ChoicesHint.is_initialized();
}

template<typename ValueType>
void GameSolver<ValueType>::setCachingEnabled(bool value) {
    if (cachingEnabled && !value) {
        // caching will be turned off. Hence we clear the cache at this point
        clearCache();
    }
    cachingEnabled = value;
}

template<typename ValueType>
bool GameSolver<ValueType>::isCachingEnabled() const {
    return cachingEnabled;
}

template<typename ValueType>
void GameSolver<ValueType>::clearCache() const {
    // Intentionally left empty.
}

template<typename ValueType>
void GameSolver<ValueType>::setHasUniqueSolution(bool value) {
    this->uniqueSolution = value;
}

template<typename ValueType>
bool GameSolver<ValueType>::hasUniqueSolution() const {
    return this->uniqueSolution;
}

template<typename ValueType>
GameSolverFactory<ValueType>::GameSolverFactory() {
    // Intentionally left empty.
}

template<typename ValueType>
std::unique_ptr<GameSolver<ValueType>> GameSolverFactory<ValueType>::create(
    Environment const& env, storm::storage::SparseMatrix<storm::storage::sparse::state_type> const& player1Matrix,
    storm::storage::SparseMatrix<ValueType> const& player2Matrix) const {
    return std::make_unique<StandardGameSolver<ValueType>>(player1Matrix, player2Matrix, std::make_unique<GeneralLinearEquationSolverFactory<ValueType>>());
}

template<typename ValueType>
std::unique_ptr<GameSolver<ValueType>> GameSolverFactory<ValueType>::create(Environment const& env,
                                                                            storm::storage::SparseMatrix<storm::storage::sparse::state_type>&& player1Matrix,
                                                                            storm::storage::SparseMatrix<ValueType>&& player2Matrix) const {
    return std::make_unique<StandardGameSolver<ValueType>>(std::move(player1Matrix), std::move(player2Matrix),
                                                           std::make_unique<GeneralLinearEquationSolverFactory<ValueType>>());
}

template<typename ValueType>
std::unique_ptr<GameSolver<ValueType>> GameSolverFactory<ValueType>::create(Environment const& env, std::vector<uint64_t> const& player1Grouping,
                                                                            storm::storage::SparseMatrix<ValueType> const& player2Matrix) const {
    return std::make_unique<StandardGameSolver<ValueType>>(player1Grouping, player2Matrix, std::make_unique<GeneralLinearEquationSolverFactory<ValueType>>());
}

template<typename ValueType>
std::unique_ptr<GameSolver<ValueType>> GameSolverFactory<ValueType>::create(Environment const& env, std::vector<uint64_t>&& player1Grouping,
                                                                            storm::storage::SparseMatrix<ValueType>&& player2Matrix) const {
    return std::make_unique<StandardGameSolver<ValueType>>(std::move(player1Grouping), std::move(player2Matrix),
                                                           std::make_unique<GeneralLinearEquationSolverFactory<ValueType>>());
}

template class GameSolver<double>;
template class GameSolver<storm::RationalNumber>;

template class GameSolverFactory<double>;
template class GameSolverFactory<storm::RationalNumber>;
}  // namespace solver
}  // namespace storm
