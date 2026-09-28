#pragma once

#include <boost/optional.hpp>
#include <chrono>
#include <iostream>
#include <memory>

#include "storm/solver/SolutionBounds.h"
#include "storm/solver/SolverStatus.h"
#include "storm/solver/TerminationCondition.h"
#include "storm/utility/ProgressMeasurement.h"

namespace storm {
namespace solver {

template<typename SolutionType>
class AbstractEquationSolver {
   public:
    AbstractEquationSolver();

    /*!
     * Configures whether this solver should show progress during iterative solving.
     *
     * @param verbose If true, progress will be shown.
     * @param delay The delay (in seconds) between progress emissions.
     */
    void setShowProgress(bool verbose, uint64_t delay = 5);

    /*!
     * Sets a custom termination condition that is used together with the regular termination condition of the
     * solver.
     *
     * @param terminationCondition An object that can be queried whether to terminate early or not.
     */
    void setTerminationCondition(std::unique_ptr<TerminationCondition<SolutionType>> terminationCondition);

    /*!
     * Removes a previously set custom termination condition.
     */
    void resetTerminationCondition();

    /*!
     * Retrieves whether a custom termination condition has been set.
     */
    bool hasCustomTerminationCondition() const;

    /*!
     * Checks whether the solver can terminate wrt. to its termination condition. If no termination condition,
     * this will yield false.
     */
    bool terminateNow(std::vector<SolutionType> const& values, SolverGuarantee const& guarantee) const;

    /*!
     * Retrieves whether this solver has particularly relevant values.
     */
    bool hasRelevantValues() const;

    /*!
     * Retrieves the relevant values (if there are any).
     */
    storm::storage::BitVector const& getRelevantValues() const;
    boost::optional<storm::storage::BitVector> const& getOptionalRelevantValues() const;

    /*!
     * Sets the relevant values.
     */
    void setRelevantValues(storm::storage::BitVector&& valuesOfInterest);

    /*!
     * Sets the relevant values.
     */
    void setRelevantValues(storm::storage::BitVector const& valuesOfInterest);

    /*!
     * Removes the values of interest (if there were any).
     */
    void clearRelevantValues();

    enum class BoundType { Global, Local, Any };

    /*!
     * Retrieves whether this solver has a lower bound.
     */
    bool hasLowerBound(BoundType const& type = BoundType::Any) const;

    /*!
     * Retrieves whether this solver has an upper bound.
     */
    bool hasUpperBound(BoundType const& type = BoundType::Any) const;

    /*!
     * Sets a lower bound for the solution that can potentially be used by the solver.
     */
    void setLowerBound(SolutionType const& value);

    /*!
     * Sets an upper bound for the solution that can potentially be used by the solver.
     */
    void setUpperBound(SolutionType const& value);

    /*!
     * Sets bounds for the solution that can potentially be used by the solver.
     */
    void setBounds(SolutionType const& lower, SolutionType const& upper);

    /*!
     * Retrieves the lower bound (if there is any).
     */
    SolutionType const& getLowerBound() const;

    /*!
     * Retrieves the lower bound for the variable with the given index (if there is any lower bound).
     * @pre some lower bound (local or global) has been specified
     * @return the largest lower bound known for the given row
     */
    SolutionType const& getLowerBound(uint64_t const& index) const;

    /*!
     * Retrieves the lower bound (if there is any).
     * If the given flag is true and if there are only local bounds,
     * the minimum of the local bounds is returned.
     */
    SolutionType getLowerBound(bool convertLocalBounds) const;

    /*!
     * Retrieves the upper bound (if there is any).
     */
    SolutionType const& getUpperBound() const;

    /*!
     * Retrieves the upper bound for the variable with the given index (if there is any upper bound).
     * @pre some upper bound (local or global) has been specified
     * @return the smallest upper bound known for the given row
     */
    SolutionType const& getUpperBound(uint64_t const& index) const;

    /*!
     * Retrieves the upper bound (if there is any).
     * If the given flag is true and if there are only local bounds,
     * the maximum of the local bounds is returned.
     */
    SolutionType getUpperBound(bool convertLocalBounds) const;

    /*!
     * Retrieves a vector containing the lower bounds (if there are any).
     */
    std::vector<SolutionType> const& getLowerBounds() const;

    /*!
     * Retrieves a vector containing the upper bounds (if there are any).
     */
    std::vector<SolutionType> const& getUpperBounds() const;

    /*!
     * Sets lower bounds for the solution that can potentially be used by the solver.
     */
    void setLowerBounds(std::vector<SolutionType> const& values);

    /*!
     * Sets lower bounds for the solution that can potentially be used by the solver.
     */
    void setLowerBounds(std::vector<SolutionType>&& values);

    /*!
     * Sets upper bounds for the solution that can potentially be used by the solver.
     */
    void setUpperBounds(std::vector<SolutionType> const& values);

    /*!
     * Sets upper bounds for the solution that can potentially be used by the solver.
     */
    void setUpperBounds(std::vector<SolutionType>&& values);

    /*!
     * Sets bounds for the solution that can potentially be used by the solver.
     */
    void setBounds(std::vector<SolutionType> const& lower, std::vector<SolutionType> const& upper);

    void setBoundsFromOtherSolver(AbstractEquationSolver<SolutionType> const& other);

    /*!
     * Removes all specified solution bounds
     */
    void clearBounds();

    /*!
     * Retrieves whether the last call to this solver computed a sound lower resp. upper bound on the solution.
     * Only some algorithms provide these, and they need not provide both.
     */
    bool hasSolutionLowerBounds() const;
    bool hasSolutionUpperBounds() const;

    /*!
     * Retrieves sound bounds on the solution that the last call to this solver computed.
     * @pre The respective bound was computed, see hasSolutionLowerBounds() resp. hasSolutionUpperBounds().
     */
    std::vector<SolutionType> const& getSolutionLowerBounds() const;
    std::vector<SolutionType> const& getSolutionUpperBounds() const;

    /*!
     * Retrieves whether progress is to be shown.
     */
    bool isShowProgressSet() const;

    /*!
     * Retrieves the delay between progress emissions.
     */
    uint64_t getShowProgressDelay() const;

    /*!
     * Starts to measure progress.
     */
    void startMeasureProgress(uint64_t startingIteration = 0) const;

    /*!
     * Shows progress if this solver is asked to do so.
     */
    void showProgressIterative(uint64_t iterations, boost::optional<uint64_t> const& bound = boost::none) const;

   protected:
    /*!
     * Retrieves the custom termination condition (if any was set).
     *
     * @return The custom termination condition.
     */
    TerminationCondition<SolutionType> const& getTerminationCondition() const;
    std::unique_ptr<TerminationCondition<SolutionType>> const& getTerminationConditionPointer() const;

    /*!
     * Stores sound bounds on the solution that were obtained while solving. Note that solving is const, so
     * that this is as well.
     */
    void setSolutionBounds(SolutionBounds<SolutionType> bounds) const;

    /*!
     * Reports the given solution as an exact one, i.e. stores it as both the lower and the upper bound.
     *
     * @param x The computed solution.
     */
    void setSolutionBoundsExact(std::vector<SolutionType> const& x) const;

    /*!
     * Discards any bounds on the solution obtained by a previous call. This must happen whenever solving
     * starts, so that a solver that is reused does not report stale bounds.
     */
    void clearSolutionBounds() const;

    void createUpperBoundsVector(std::vector<SolutionType>& upperBoundsVector) const;
    void createUpperBoundsVector(std::unique_ptr<std::vector<SolutionType>>& upperBoundsVector, uint64_t length) const;
    void createLowerBoundsVector(std::vector<SolutionType>& lowerBoundsVector) const;

    /*!
     * Report the current status of the solver.
     * @param status Solver status.
     * @param iterations Number of iterations (if solver is iterative).
     */
    void reportStatus(SolverStatus status, boost::optional<uint64_t> const& iterations = boost::none) const;

    /*!
     * Update the status of the solver with respect to convergence, early termination, abortion, etc.
     * @param status Current status.
     * @param x Vector for terminatation condition.
     * @param guarantee Guarentee for termination condition.
     * @param iterations Current number of iterations.
     * @param maximalNumberOfIterations Maximal number of iterations.
     * @return New status.
     */
    SolverStatus updateStatus(SolverStatus status, std::vector<SolutionType> const& x, SolverGuarantee const& guarantee, uint64_t iterations,
                              uint64_t maximalNumberOfIterations) const;

    /*!
     * Update the status of the solver with respect to convergence, early termination, abortion, etc.
     * @param status Current status.
     * @param earlyTermination Flag indicating if the solver can be terminated early.
     * @param iterations Current number of iterations.
     * @param maximalNumberOfIterations Maximal number of iterations.
     * @return New status.
     */
    SolverStatus updateStatus(SolverStatus status, bool earlyTermination, uint64_t iterations, uint64_t maximalNumberOfIterations) const;

    // A termination condition to be used (can be unset).
    std::unique_ptr<TerminationCondition<SolutionType>> terminationCondition;

    // A bit vector containing the indices of the relevant values if they were set.
    boost::optional<storm::storage::BitVector> relevantValues;

    // A lower bound if one was set.
    boost::optional<SolutionType> lowerBound;

    // An upper bound if one was set.
    boost::optional<SolutionType> upperBound;

    // Lower bounds if they were set.
    boost::optional<std::vector<SolutionType>> lowerBounds;

    // Lower bounds if they were set.
    boost::optional<std::vector<SolutionType>> upperBounds;

   private:
    // Sound bounds on the solution, if the last call to this solver produced any.
    mutable SolutionBounds<SolutionType> solutionBounds;

    // Indicates the progress of this solver.
    mutable boost::optional<storm::utility::ProgressMeasurement> progressMeasurement;
};

}  // namespace solver
}  // namespace storm
