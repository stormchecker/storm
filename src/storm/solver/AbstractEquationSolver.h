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

template<typename ValueType>
class AbstractEquationSolver {
   public:
    AbstractEquationSolver();

    /*!
     * Sets a custom termination condition that is used together with the regular termination condition of the
     * solver.
     *
     * @param terminationCondition An object that can be queried whether to terminate early or not.
     */
    void setTerminationCondition(std::unique_ptr<TerminationCondition<ValueType>> terminationCondition);

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
    bool terminateNow(std::vector<ValueType> const& values, SolverGuarantee const& guarantee) const;

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
    void setLowerBound(ValueType const& value);

    /*!
     * Sets an upper bound for the solution that can potentially be used by the solver.
     */
    void setUpperBound(ValueType const& value);

    /*!
     * Sets bounds for the solution that can potentially be used by the solver.
     */
    void setBounds(ValueType const& lower, ValueType const& upper);

    /*!
     * Retrieves the lower bound (if there is any).
     */
    ValueType const& getLowerBound() const;

    /*!
     * Retrieves the lower bound for the variable with the given index (if there is any lower bound).
     * @pre some lower bound (local or global) has been specified
     * @return the largest lower bound known for the given row
     */
    ValueType const& getLowerBound(uint64_t const& index) const;

    /*!
     * Retrieves the lower bound (if there is any).
     * If the given flag is true and if there are only local bounds,
     * the minimum of the local bounds is returned.
     */
    ValueType getLowerBound(bool convertLocalBounds) const;

    /*!
     * Retrieves the upper bound (if there is any).
     */
    ValueType const& getUpperBound() const;

    /*!
     * Retrieves the upper bound for the variable with the given index (if there is any upper bound).
     * @pre some upper bound (local or global) has been specified
     * @return the smallest upper bound known for the given row
     */
    ValueType const& getUpperBound(uint64_t const& index) const;

    /*!
     * Retrieves the upper bound (if there is any).
     * If the given flag is true and if there are only local bounds,
     * the maximum of the local bounds is returned.
     */
    ValueType getUpperBound(bool convertLocalBounds) const;

    /*!
     * Retrieves a vector containing the lower bounds (if there are any).
     */
    std::vector<ValueType> const& getLowerBounds() const;

    /*!
     * Retrieves a vector containing the upper bounds (if there are any).
     */
    std::vector<ValueType> const& getUpperBounds() const;

    /*!
     * Sets lower bounds for the solution that can potentially be used by the solver.
     */
    void setLowerBounds(std::vector<ValueType> const& values);

    /*!
     * Sets lower bounds for the solution that can potentially be used by the solver.
     */
    void setLowerBounds(std::vector<ValueType>&& values);

    /*!
     * Sets upper bounds for the solution that can potentially be used by the solver.
     */
    void setUpperBounds(std::vector<ValueType> const& values);

    /*!
     * Sets upper bounds for the solution that can potentially be used by the solver.
     */
    void setUpperBounds(std::vector<ValueType>&& values);

    /*!
     * Sets bounds for the solution that can potentially be used by the solver.
     */
    void setBounds(std::vector<ValueType> const& lower, std::vector<ValueType> const& upper);

    void setBoundsFromOtherSolver(AbstractEquationSolver<ValueType> const& other);

    /*!
     * Removes all specified solution bounds
     */
    void clearBounds();

    /*!
     * Retrieves whether the last call to this solver computed sound bounds on the solution.
     * Only some algorithms, most notably interval iteration and optimistic value iteration, provide these, and
     * even those only when they converged.
     */
    bool hasSolutionBounds() const;

    /*!
     * Retrieves sound bounds on the solution that the last call to this solver computed.
     * @pre Such bounds were computed, see hasSolutionBounds().
     */
    std::vector<ValueType> const& getSolutionLowerBounds() const;
    std::vector<ValueType> const& getSolutionUpperBounds() const;

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
    TerminationCondition<ValueType> const& getTerminationCondition() const;
    std::unique_ptr<TerminationCondition<ValueType>> const& getTerminationConditionPointer() const;

    /*!
     * Stores sound bounds on the solution that were obtained while solving. Note that solving is const, so
     * that this is as well.
     */
    void setSolutionBounds(std::vector<ValueType> lower, std::vector<ValueType> upper) const;

    /*!
     * Discards any bounds on the solution obtained by a previous call. This must happen whenever solving
     * starts, so that a solver that is reused does not report stale bounds.
     */
    void clearSolutionBounds() const;

    void createUpperBoundsVector(std::vector<ValueType>& upperBoundsVector) const;
    void createUpperBoundsVector(std::unique_ptr<std::vector<ValueType>>& upperBoundsVector, uint64_t length) const;
    void createLowerBoundsVector(std::vector<ValueType>& lowerBoundsVector) const;

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
    SolverStatus updateStatus(SolverStatus status, std::vector<ValueType> const& x, SolverGuarantee const& guarantee, uint64_t iterations,
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
    std::unique_ptr<TerminationCondition<ValueType>> terminationCondition;

    // A bit vector containing the indices of the relevant values if they were set.
    boost::optional<storm::storage::BitVector> relevantValues;

    // A lower bound if one was set.
    boost::optional<ValueType> lowerBound;

    // An upper bound if one was set.
    boost::optional<ValueType> upperBound;

    // Lower bounds if they were set.
    boost::optional<std::vector<ValueType>> lowerBounds;

    // Lower bounds if they were set.
    boost::optional<std::vector<ValueType>> upperBounds;

   private:
    // Sound bounds on the solution, if the last call to this solver produced any.
    mutable SolutionBounds<ValueType> solutionBounds;

    // Indicates the progress of this solver.
    mutable boost::optional<storm::utility::ProgressMeasurement> progressMeasurement;
};

}  // namespace solver
}  // namespace storm
