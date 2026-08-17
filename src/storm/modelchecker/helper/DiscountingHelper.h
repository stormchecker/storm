#pragma once

#include "SingleValueModelCheckerHelper.h"
#include "storm/solver/helper/ValueIterationOperator.h"
#include "storm/storage/Scheduler.h"
#include "storm/utility/ProgressMeasurement.h"

namespace storm {
class Environment;
namespace modelchecker {
namespace helper {
template<typename ValueType, bool TrivialRowGrouping = false>
class DiscountingHelper : public SingleValueModelCheckerHelper<ValueType, storm::models::ModelRepresentation::Sparse> {
   public:
    DiscountingHelper(storm::storage::SparseMatrix<ValueType> const& A, ValueType discountFactor);
    DiscountingHelper(storm::storage::SparseMatrix<ValueType> const& A, ValueType discountFactor, bool trackScheduler);

    bool solveWithDiscountedValueIteration(Environment const& env, std::optional<OptimizationDirection> dir, std::vector<ValueType>& x,
                                           std::vector<ValueType> const& b) const;

    /*!
     * Retrieves the generated scheduler. Note: it is only legal to call this function if a scheduler was generated.
     */
    storm::storage::Scheduler<ValueType> computeScheduler() const;

    /*!
     * Retrieves whether the solver generated a scheduler.
     */
    bool hasScheduler() const;

    void setTrackScheduler(bool trackScheduler);

    bool isTrackSchedulerSet() const;

   private:
    void setUpViOperator() const;

    void showProgressIterative(uint64_t iteration) const;

    void extractScheduler(std::vector<ValueType>& x, std::vector<ValueType> const& b, OptimizationDirection const& dir,
                          UncertaintyResolutionMode uncertaintyResolutionMode) const;

    mutable std::shared_ptr<storm::solver::helper::ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator;
    mutable std::unique_ptr<std::vector<ValueType>> auxiliaryRowGroupVector;

    mutable boost::optional<storm::utility::ProgressMeasurement> progressMeasurement;

    // If the solver takes posession of the matrix, we store the moved matrix in this member, so it gets deleted
    // when the solver is destructed.
    std::unique_ptr<storm::storage::SparseMatrix<ValueType>> localA;

    // A reference to the original sparse matrix given to this solver. If the solver takes posession of the matrix
    // the reference refers to localA.
    storm::storage::SparseMatrix<ValueType> const* A;

    storm::storage::SparseMatrix<ValueType> discountedA;

    ValueType discountFactor;

    /// Whether we generate a scheduler during solving.
    bool trackScheduler = false;

    /// The scheduler choices that induce the optimal values (if they could be successfully generated).
    mutable boost::optional<std::vector<uint_fast64_t>> schedulerChoices;
};
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm