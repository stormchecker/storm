#include "storm/solver/MinMaxLinearEquationSolver.h"

#include <memory>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/solver/AcyclicMinMaxLinearEquationSolver.h"
#include "storm/solver/IterativeMinMaxLinearEquationSolver.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/LpMinMaxLinearEquationSolver.h"
#include "storm/solver/TopologicalMinMaxLinearEquationSolver.h"
#include "storm/storage/Scheduler.h"
#include "storm/utility/macros.h"

namespace storm::solver {

template<typename ValueType, typename SolutionType>
MinMaxLinearEquationSolver<ValueType, SolutionType>::MinMaxLinearEquationSolver(OptimizationDirectionSetting direction)
    : direction(direction),
      trackScheduler(false),
      uniqueSolution(false),
      noEndComponents(false),
      cachingEnabled(false),
      requirementsChecked(false),
      uncertaintyResolutionMode(UncertaintyResolutionMode::Unset) {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
MinMaxLinearEquationSolver<ValueType, SolutionType>::~MinMaxLinearEquationSolver() {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
bool MinMaxLinearEquationSolver<ValueType, SolutionType>::solveEquations(Environment const& env, OptimizationDirection d, std::vector<SolutionType>& x,
                                                                         std::vector<ValueType> const& b) const {
    STORM_LOG_WARN_COND_DEBUG(this->isRequirementsCheckedSet(),
                              "The requirements of the solver have not been marked as checked. Please provide the appropriate check or mark the requirements "
                              "as checked (if applicable).");
    return internalSolveEquations(env, d, x, b);
}

template<typename ValueType, typename SolutionType>
void MinMaxLinearEquationSolver<ValueType, SolutionType>::solveEquations(Environment const& env, std::vector<SolutionType>& x,
                                                                         std::vector<ValueType> const& b) const {
    STORM_LOG_THROW(isSet(this->direction), storm::exceptions::IllegalFunctionCallException, "Optimization direction not set.");
    STORM_LOG_THROW(isSet(this->uncertaintyResolutionMode) || !storm::IsIntervalType<ValueType>, storm::exceptions::IllegalFunctionCallException,
                    "Uncertainty resolution mode not set.");
    solveEquations(env, convert(this->direction), x, b);
}

template<typename ValueType, typename SolutionType>
void MinMaxLinearEquationSolver<ValueType, SolutionType>::setOptimizationDirection(OptimizationDirection d) {
    direction = convert(d);
}

template<typename ValueType, typename SolutionType>
void MinMaxLinearEquationSolver<ValueType, SolutionType>::unsetOptimizationDirection() {
    direction = OptimizationDirectionSetting::Unset;
}

template<typename ValueType, typename SolutionType>
void MinMaxLinearEquationSolver<ValueType, SolutionType>::setHasUniqueSolution(bool value) {
    uniqueSolution = value;
}

template<typename ValueType, typename SolutionType>
bool MinMaxLinearEquationSolver<ValueType, SolutionType>::hasUniqueSolution() const {
    return uniqueSolution || noEndComponents;
}

template<typename ValueType, typename SolutionType>
void MinMaxLinearEquationSolver<ValueType, SolutionType>::setHasNoEndComponents(bool value) {
    noEndComponents = value;
}

template<typename ValueType, typename SolutionType>
bool MinMaxLinearEquationSolver<ValueType, SolutionType>::hasNoEndComponents() const {
    return noEndComponents;
}

template<typename ValueType, typename SolutionType>
void MinMaxLinearEquationSolver<ValueType, SolutionType>::setTrackScheduler(bool trackScheduler) {
    this->trackScheduler = trackScheduler;
    if (!this->trackScheduler) {
        schedulerChoices = boost::none;
    }
}

template<typename ValueType, typename SolutionType>
bool MinMaxLinearEquationSolver<ValueType, SolutionType>::isTrackSchedulerSet() const {
    return this->trackScheduler;
}

template<typename ValueType, typename SolutionType>
bool MinMaxLinearEquationSolver<ValueType, SolutionType>::hasScheduler() const {
    return static_cast<bool>(schedulerChoices);
}

template<typename ValueType, typename SolutionType>
storm::storage::Scheduler<ValueType> MinMaxLinearEquationSolver<ValueType, SolutionType>::computeScheduler() const {
    STORM_LOG_THROW(hasScheduler(), storm::exceptions::IllegalFunctionCallException, "Cannot retrieve scheduler, because none was generated.");
    storm::storage::Scheduler<ValueType> result(schedulerChoices->size());
    uint_fast64_t state = 0;
    for (auto const& schedulerChoice : schedulerChoices.get()) {
        result.setChoice(schedulerChoice, state);
        ++state;
    }
    return result;
}

template<typename ValueType, typename SolutionType>
std::vector<uint_fast64_t> const& MinMaxLinearEquationSolver<ValueType, SolutionType>::getSchedulerChoices() const {
    STORM_LOG_THROW(hasScheduler(), storm::exceptions::IllegalFunctionCallException, "Cannot retrieve scheduler choices, because they were not generated.");
    return schedulerChoices.get();
}

template<typename ValueType, typename SolutionType>
std::vector<uint_fast64_t> const& MinMaxLinearEquationSolver<ValueType, SolutionType>::getRobustSchedulerIndex() const {
    STORM_LOG_THROW(hasScheduler(), storm::exceptions::IllegalFunctionCallException,
                    "Cannot retrieve robust index into scheduler choices, because they were not generated.");
    return robustSchedulerIndex.get();
}

template<typename ValueType, typename SolutionType>
void MinMaxLinearEquationSolver<ValueType, SolutionType>::setCachingEnabled(bool value) {
    if (cachingEnabled && !value) {
        // caching will be turned off. Hence we clear the cache at this point
        clearCache();
    }
    cachingEnabled = value;
}

template<typename ValueType, typename SolutionType>
bool MinMaxLinearEquationSolver<ValueType, SolutionType>::isCachingEnabled() const {
    return cachingEnabled;
}

template<typename ValueType, typename SolutionType>
void MinMaxLinearEquationSolver<ValueType, SolutionType>::clearCache() const {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
void MinMaxLinearEquationSolver<ValueType, SolutionType>::setInitialScheduler(std::vector<uint_fast64_t>&& choices) {
    initialScheduler = std::move(choices);
}

template<typename ValueType, typename SolutionType>
bool MinMaxLinearEquationSolver<ValueType, SolutionType>::hasInitialScheduler() const {
    return static_cast<bool>(initialScheduler);
}

template<typename ValueType, typename SolutionType>
std::vector<uint_fast64_t> const& MinMaxLinearEquationSolver<ValueType, SolutionType>::getInitialScheduler() const {
    return initialScheduler.get();
}

template<typename ValueType, typename SolutionType>
MinMaxLinearEquationSolverRequirements MinMaxLinearEquationSolver<ValueType, SolutionType>::getRequirements(
    Environment const&, boost::optional<storm::solver::OptimizationDirection> const& direction, bool const& hasInitialScheduler) const {
    return MinMaxLinearEquationSolverRequirements();
}

template<typename ValueType, typename SolutionType>
void MinMaxLinearEquationSolver<ValueType, SolutionType>::setRequirementsChecked(bool value) {
    this->requirementsChecked = value;
}

template<typename ValueType, typename SolutionType>
bool MinMaxLinearEquationSolver<ValueType, SolutionType>::isRequirementsCheckedSet() const {
    return requirementsChecked;
}

template<typename ValueType, typename SolutionType>
void MinMaxLinearEquationSolver<ValueType, SolutionType>::setSchedulerFixedForRowGroup(storm::storage::BitVector&& schedulerFixedForRowGroup) {
    STORM_LOG_ASSERT(this->hasInitialScheduler(), "Expecting an initial scheduler to be set before setting the states for which the choices are fixed");
    this->choiceFixedForRowGroup = std::move(schedulerFixedForRowGroup);
}

template<typename ValueType, typename SolutionType>
MinMaxLinearEquationSolverFactory<ValueType, SolutionType>::MinMaxLinearEquationSolverFactory() : requirementsChecked(false) {
    // Intentionally left empty
}

template<typename ValueType, typename SolutionType>
void MinMaxLinearEquationSolverFactory<ValueType, SolutionType>::setRequirementsChecked(bool value) {
    this->requirementsChecked = value;
}

template<typename ValueType, typename SolutionType>
bool MinMaxLinearEquationSolverFactory<ValueType, SolutionType>::isRequirementsCheckedSet() const {
    return this->requirementsChecked;
}

template<typename ValueType, typename SolutionType>
void MinMaxLinearEquationSolver<ValueType, SolutionType>::setUncertaintyResolutionMode(storm::solver::UncertaintyResolutionMode uncertaintyResolutionMode) {
    this->uncertaintyResolutionMode = uncertaintyResolutionMode;
}

template<typename ValueType, typename SolutionType>
UncertaintyResolutionMode MinMaxLinearEquationSolver<ValueType, SolutionType>::getUncertaintyResolutionMode() const {
    return this->uncertaintyResolutionMode;
}

template<typename ValueType, typename SolutionType>
MinMaxLinearEquationSolverRequirements MinMaxLinearEquationSolverFactory<ValueType, SolutionType>::getRequirements(
    Environment const& env, bool hasUniqueSolution, bool hasNoEndComponents, boost::optional<storm::solver::OptimizationDirection> const& direction,
    bool hasInitialScheduler, bool trackScheduler) const {
    // Create dummy solver and ask it for requirements.
    std::unique_ptr<MinMaxLinearEquationSolver<ValueType, SolutionType>> solver = this->create(env);
    solver->setTrackScheduler(trackScheduler);
    solver->setHasUniqueSolution(hasUniqueSolution);
    solver->setHasNoEndComponents(hasNoEndComponents);
    return solver->getRequirements(env, direction, hasInitialScheduler);
}

template<typename ValueType, typename SolutionType>
std::unique_ptr<MinMaxLinearEquationSolver<ValueType, SolutionType>> MinMaxLinearEquationSolverFactory<ValueType, SolutionType>::create(
    Environment const& env, storm::storage::SparseMatrix<ValueType> const& matrix) const {
    std::unique_ptr<MinMaxLinearEquationSolver<ValueType, SolutionType>> solver = this->create(env);
    solver->setMatrix(matrix);
    return solver;
}

template<typename ValueType, typename SolutionType>
std::unique_ptr<MinMaxLinearEquationSolver<ValueType, SolutionType>> MinMaxLinearEquationSolverFactory<ValueType, SolutionType>::create(
    Environment const& env, storm::storage::SparseMatrix<ValueType>&& matrix) const {
    std::unique_ptr<MinMaxLinearEquationSolver<ValueType, SolutionType>> solver = this->create(env);
    solver->setMatrix(std::move(matrix));
    return solver;
}

template<typename ValueType, typename SolutionType>
GeneralMinMaxLinearEquationSolverFactory<ValueType, SolutionType>::GeneralMinMaxLinearEquationSolverFactory()
    : MinMaxLinearEquationSolverFactory<ValueType, SolutionType>() {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
std::unique_ptr<MinMaxLinearEquationSolver<ValueType, SolutionType>> GeneralMinMaxLinearEquationSolverFactory<ValueType, SolutionType>::create(
    Environment const& env) const {
    std::unique_ptr<MinMaxLinearEquationSolver<ValueType, SolutionType>> result;
    // TODO some minmax linear equation solvers only support SolutionType == ValueType.
    auto method = GeneralMinMaxLinearEquationSolverFactory<ValueType, SolutionType>::getMethod(env);
    if (method == MinMaxMethod::ValueIteration || method == MinMaxMethod::PolicyIteration || method == MinMaxMethod::RationalSearch ||
        method == MinMaxMethod::IntervalIteration || method == MinMaxMethod::SoundValueIteration || method == MinMaxMethod::OptimisticValueIteration ||
        method == MinMaxMethod::GuessingValueIteration || method == MinMaxMethod::ViToPi) {
        result = std::make_unique<IterativeMinMaxLinearEquationSolver<ValueType, SolutionType>>(
            std::make_unique<GeneralLinearEquationSolverFactory<SolutionType>>());
    } else if (method == MinMaxMethod::Topological) {
        if constexpr (storm::IsIntervalType<ValueType>) {
            STORM_LOG_ERROR("Topological method not implemented for ValueType==Interval.");
        } else {
            result = std::make_unique<TopologicalMinMaxLinearEquationSolver<ValueType, SolutionType>>();
        }
    } else if (method == MinMaxMethod::LinearProgramming || method == MinMaxMethod::ViToLp) {
        if constexpr (storm::IsIntervalType<ValueType>) {
            STORM_LOG_ERROR("LP method not implemented for ValueType==Interval.");
        } else {
            result = std::make_unique<LpMinMaxLinearEquationSolver<ValueType>>(storm::utility::solver::getLpSolverFactory<ValueType>());
        }
    } else if (method == MinMaxMethod::Acyclic) {
        if constexpr (storm::IsIntervalType<ValueType>) {
            STORM_LOG_ERROR("Acyclic method not implemented for ValueType==Interval");
        } else {
            result = std::make_unique<AcyclicMinMaxLinearEquationSolver<ValueType>>();
        }
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidSettingsException, "Unsupported technique.");
    }
    result->setRequirementsChecked(this->isRequirementsCheckedSet());
    return result;
}

template<>
std::unique_ptr<MinMaxLinearEquationSolver<storm::RationalNumber>> GeneralMinMaxLinearEquationSolverFactory<storm::RationalNumber>::create(
    Environment const& env) const {
    std::unique_ptr<MinMaxLinearEquationSolver<storm::RationalNumber>> result;
    auto method = getMethod(env);
    if (method == MinMaxMethod::ValueIteration || method == MinMaxMethod::PolicyIteration || method == MinMaxMethod::RationalSearch ||
        method == MinMaxMethod::IntervalIteration || method == MinMaxMethod::SoundValueIteration || method == MinMaxMethod::OptimisticValueIteration ||
        method == MinMaxMethod::GuessingValueIteration || method == MinMaxMethod::ViToPi) {
        result = std::make_unique<IterativeMinMaxLinearEquationSolver<storm::RationalNumber>>(
            std::make_unique<GeneralLinearEquationSolverFactory<storm::RationalNumber>>());
    } else if (method == MinMaxMethod::LinearProgramming || method == MinMaxMethod::ViToLp) {
        result = std::make_unique<LpMinMaxLinearEquationSolver<storm::RationalNumber>>(storm::utility::solver::getLpSolverFactory<storm::RationalNumber>());
    } else if (method == MinMaxMethod::Acyclic) {
        result = std::make_unique<AcyclicMinMaxLinearEquationSolver<storm::RationalNumber>>();
    } else if (method == MinMaxMethod::Topological) {
        result = std::make_unique<TopologicalMinMaxLinearEquationSolver<storm::RationalNumber>>();
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidSettingsException, "Unsupported technique.");
    }
    result->setRequirementsChecked(this->isRequirementsCheckedSet());
    return result;
}

template<typename ValueType, typename SolutionType>
MinMaxMethod GeneralMinMaxLinearEquationSolverFactory<ValueType, SolutionType>::getMethod(storm::Environment env) const {
    // Default to robust value iteration in case of interval models.
    auto method = env.solver().minMax().getMethod();
    if (storm::IsIntervalType<ValueType> && method != MinMaxMethod::ValueIteration) {
        STORM_LOG_WARN("Selected method is not supported for this solver and interval models, switching to robust value iteration.");
        method = MinMaxMethod::ValueIteration;
    }
    return method;
}

template class MinMaxLinearEquationSolver<double>;

template class MinMaxLinearEquationSolverFactory<double>;
template class GeneralMinMaxLinearEquationSolverFactory<double>;

template class MinMaxLinearEquationSolver<storm::RationalNumber>;
template class MinMaxLinearEquationSolverFactory<storm::RationalNumber>;
template class GeneralMinMaxLinearEquationSolverFactory<storm::RationalNumber>;

template class MinMaxLinearEquationSolver<storm::Interval, double>;
template class MinMaxLinearEquationSolverFactory<storm::Interval, double>;
template class GeneralMinMaxLinearEquationSolverFactory<storm::Interval, double>;

template class MinMaxLinearEquationSolver<storm::RationalInterval, storm::RationalNumber>;
template class MinMaxLinearEquationSolverFactory<storm::RationalInterval, storm::RationalNumber>;
template class GeneralMinMaxLinearEquationSolverFactory<storm::RationalInterval, storm::RationalNumber>;
}  // namespace storm::solver
