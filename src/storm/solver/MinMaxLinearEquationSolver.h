#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <boost/optional.hpp>

#include "storm/solver/AbstractEquationSolver.h"
#include "storm/solver/MinMaxLinearEquationSolverRequirements.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/SolverSelectionOptions.h"
#include "storm/solver/UncertaintyResolutionMode.h"
#include "storm/storage/sparse/StateType.h"

#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/utility/macros.h"

namespace storm {

class Environment;

namespace storage {
template<typename T>
class SparseMatrix;
template<typename ValueType>
class Scheduler;
}  // namespace storage

namespace solver {

/*!
 * A class representing the interface that all min-max linear equation solvers shall implement.
 */
template<typename ValueType, typename SolutionType = ValueType>
class MinMaxLinearEquationSolver : public AbstractEquationSolver<SolutionType> {
   protected:
    MinMaxLinearEquationSolver(OptimizationDirectionSetting direction = OptimizationDirectionSetting::Unset);

   public:
    virtual ~MinMaxLinearEquationSolver();

    virtual void setMatrix(storm::storage::SparseMatrix<ValueType> const& matrix) = 0;
    virtual void setMatrix(storm::storage::SparseMatrix<ValueType>&& matrix) = 0;

    /*!
     * Solves the equation system x = min/max(A*x + b) given by the parameters. Note that the matrix A has
     * to be given upon construction time of the solver object.
     *
     * @param d For minimum, all the value of a group of rows is the taken as the minimum over all rows and as
     * the maximum otherwise.
     * @param x The solution vector x. The initial values of x represent a guess of the real values to the
     * solver, but may be ignored.
     * @param b The vector to add after matrix-vector multiplication.
     */
    bool solveEquations(Environment const& env, OptimizationDirection d, std::vector<SolutionType>& x, std::vector<ValueType> const& b) const;

    /*!
     * Behaves the same as the other variant of <code>solveEquations</code>, with the distinction that
     * instead of providing the optimization direction as an argument, the internally set optimization direction
     * is used. Note: this method can only be called after setting the optimization direction.
     */
    void solveEquations(Environment const& env, std::vector<SolutionType>& x, std::vector<ValueType> const& b) const;

    /*!
     * Sets an optimization direction to use for calls to methods that do not explicitly provide one.
     */
    void setOptimizationDirection(OptimizationDirection direction);

    /*!
     * Unsets the optimization direction to use for calls to methods that do not explicitly provide one.
     */
    void unsetOptimizationDirection();

    /*!
     * Sets how the uncertainty should be resolved.
     */
    void setUncertaintyResolutionMode(UncertaintyResolutionMode uncertaintyResolutionMode);

    /*!
     * Retrieves the mode indicating how the uncertainty should be resolved.
     */
    UncertaintyResolutionMode getUncertaintyResolutionMode() const;

    /*!
     * Sets the states for which the choices are fixed.
     * @param schedulerFixedForRowGroup bitvector with the states where the choices are fixed.
     */
    virtual void setSchedulerFixedForRowGroup(storm::storage::BitVector&& schedulerFixedForRowGroup);

    /*!
     * Sets whether the solution to the min max equation system is known to be unique.
     */
    void setHasUniqueSolution(bool value = true);

    /*!
     * Retrieves whether the solution to the min max equation system is assumed to be unique.
     * Note that having no end components implies that the solution is unique. Thus, this also returns true if
     * `hasNoEndComponents()` returns true.
     * Also note that a unique solution does not imply the absence of ECs, because, e.g. in Rmin properties there
     * can still be ECs in which infinite reward is collected.
     */
    bool hasUniqueSolution() const;

    /*!
     * Sets whether the min max equation system is known to not have any end components
     */
    void setHasNoEndComponents(bool value = true);

    /*!
     * Retrieves whether the min max equation system is known to not have any end components
     */
    bool hasNoEndComponents() const;

    /*!
     * Sets whether schedulers are generated when solving equation systems. If the argument is false, the currently
     * stored scheduler (if any) is deleted.
     */
    void setTrackScheduler(bool trackScheduler = true);

    /*!
     * Retrieves whether this solver is set to generate schedulers.
     */
    bool isTrackSchedulerSet() const;

    /*!
     * Retrieves whether the solver generated a scheduler.
     */
    bool hasScheduler() const;

    /*!
     * Retrieves the generated scheduler. Note: it is only legal to call this function if a scheduler was generated.
     */
    storm::storage::Scheduler<ValueType> computeScheduler() const;

    /*!
     * Retrieves the generated (deterministic) choices of the optimal scheduler. Note: it is only legal to call this function if a scheduler was generated.
     */
    std::vector<uint_fast64_t> const& getSchedulerChoices() const;

    /*!
     * Retrieves the generated robust index into the scheduler. Note: it is only legal to call this function if a scheduler was generated.
     */
    std::vector<uint_fast64_t> const& getRobustSchedulerIndex() const;

    /*!
     * Sets whether some of the generated data during solver calls should be cached.
     * This possibly decreases the runtime of subsequent calls but also increases memory consumption.
     */
    void setCachingEnabled(bool value);

    /*!
     * Retrieves whether some of the generated data during solver calls should be cached.
     */
    bool isCachingEnabled() const;

    /*!
     * Clears the currently cached data that has been stored during previous calls of the solver.
     */
    virtual void clearCache() const;

    /*!
     * Sets a valid initial scheduler that is required by some solvers (see requirements of solvers).
     */
    void setInitialScheduler(std::vector<uint_fast64_t>&& choices);

    /*!
     * Returns true iff an initial scheduler is set.
     */
    bool hasInitialScheduler() const;

    /*!
     * Retrieves the initial scheduler if one was set.
     */
    std::vector<uint_fast64_t> const& getInitialScheduler() const;

    /*!
     * Retrieves the requirements of this solver for solving equations with the current settings. The requirements
     * are guaranteed to be ordered according to their appearance in the SolverRequirement type.
     */
    virtual MinMaxLinearEquationSolverRequirements getRequirements(Environment const& env,
                                                                   boost::optional<storm::solver::OptimizationDirection> const& direction = boost::none,
                                                                   bool const& hasInitialScheduler = false) const;

    /*!
     * Notifies the solver that the requirements for solving equations have been checked. If this has not been
     * done before solving equations, the solver might issue a warning, perform the checks itself or even fail.
     */
    void setRequirementsChecked(bool value = true);

    /*!
     * Retrieves whether the solver is aware that the requirements were checked.
     */
    bool isRequirementsCheckedSet() const;

   protected:
    virtual bool internalSolveEquations(Environment const& env, OptimizationDirection d, std::vector<SolutionType>& x,
                                        std::vector<ValueType> const& b) const = 0;

    /// The optimization direction to use for calls to functions that do not provide it explicitly. Can also be unset.
    OptimizationDirectionSetting direction;

    /// Whether we generate a scheduler during solving.
    bool trackScheduler;

    /// The scheduler choices that induce the optimal values (if they could be successfully generated).
    mutable boost::optional<std::vector<uint_fast64_t>> schedulerChoices;

    /// For interval models, this index points into the schedulerChoices and is a state -> index map.
    /// In an MDP, the scheduler choices are just one number for each row group
    /// - the row that was chosen. In an iMC (this is currently the only
    /// interval model that supports extracting schedulers here), a scheduler is
    /// an order of entries within a single row. In the current representation,
    /// the schedulerChoices from robustSchedulerIndex[i] to
    /// robustSchedulerIndex[i+1] (exclusive) are this permutation. So, the
    /// robustSchedulerIndex[i] currently just counts how many matrix entries
    /// come before row i.
    mutable boost::optional<std::vector<uint_fast64_t>> robustSchedulerIndex;

    /// A scheduler that can be used by solvers that require a valid initial scheduler.
    boost::optional<std::vector<uint_fast64_t>> initialScheduler;

    ///
    boost::optional<storm::storage::BitVector> choiceFixedForRowGroup;

   private:
    /// Whether the solver can assume that the min-max equation system has a unique solution
    bool uniqueSolution;

    /// Whether the solver can assume that the min-max equation system has no end components
    bool noEndComponents;

    /// Whether some of the generated data during solver calls should be cached.
    bool cachingEnabled;

    /// A flag storing whether the requirements of the solver were checked.
    bool requirementsChecked;

    /// For uncertain models, this mode decides how the uncertainty will be resolved by nature.
    UncertaintyResolutionMode uncertaintyResolutionMode;
};

template<typename ValueType, typename SolutionType = ValueType>
class MinMaxLinearEquationSolverFactory {
   public:
    MinMaxLinearEquationSolverFactory();
    virtual ~MinMaxLinearEquationSolverFactory() = default;

    std::unique_ptr<MinMaxLinearEquationSolver<ValueType, SolutionType>> create(Environment const& env,
                                                                                storm::storage::SparseMatrix<ValueType> const& matrix) const;
    std::unique_ptr<MinMaxLinearEquationSolver<ValueType, SolutionType>> create(Environment const& env, storm::storage::SparseMatrix<ValueType>&& matrix) const;
    virtual std::unique_ptr<MinMaxLinearEquationSolver<ValueType, SolutionType>> create(Environment const& env) const = 0;

    /*!
     * Retrieves the requirements of the solver that would be created when calling create() right now. The
     * requirements are guaranteed to be ordered according to their appearance in the SolverRequirement type.
     */
    MinMaxLinearEquationSolverRequirements getRequirements(Environment const& env, bool hasUniqueSolution = false, bool hasNoEndComponents = false,
                                                           boost::optional<storm::solver::OptimizationDirection> const& direction = boost::none,
                                                           bool hasInitialScheduler = false, bool trackScheduler = false) const;
    void setRequirementsChecked(bool value = true);
    bool isRequirementsCheckedSet() const;

   private:
    bool requirementsChecked;
};

template<typename ValueType, typename SolutionType = ValueType>
class GeneralMinMaxLinearEquationSolverFactory : public MinMaxLinearEquationSolverFactory<ValueType, SolutionType> {
   public:
    GeneralMinMaxLinearEquationSolverFactory();

    // Make the other create methods visible.
    using MinMaxLinearEquationSolverFactory<ValueType, SolutionType>::create;

    virtual std::unique_ptr<MinMaxLinearEquationSolver<ValueType, SolutionType>> create(Environment const& env) const override;

   protected:
    MinMaxMethod getMethod(Environment env) const;
};

}  // namespace solver
}  // namespace storm
