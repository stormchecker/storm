#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/SolutionBounds.h"
#include "storm/solver/SolverStatus.h"
#include "storm/solver/helper/ValueIterationOperatorForward.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/OptionalRef.h"
#include "storm/utility/constants.h"

namespace storm::solver::helper {

template<typename ValueType>
struct IIData {
    std::vector<ValueType> const& x;
    std::vector<ValueType> const& y;
    SolverStatus const status;
};

/*!
 * Implements interval iteration
 * @see https://doi.org/10.1007/978-3-319-63387-9_8
 */
template<typename ValueType, bool TrivialRowGrouping>
class IntervalIterationHelper {
   public:
    IntervalIterationHelper(std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator);

    template<OptimizationDirection Dir, typename OffsetType, bool Smooth = false, bool InvertedXDir = false>
    SolverStatus II(std::pair<std::vector<ValueType>, std::vector<ValueType>>& xy, OffsetType const& offsets, uint64_t& numIterations, bool relative,
                    ValueType const& precision, std::function<SolverStatus(IIData<ValueType> const&)> const& iterationCallback = {},
                    std::optional<storm::storage::BitVector> const& relevantValues = {}, ValueType const& gamma = storm::utility::zero<ValueType>()) const;

    /*!
     * Smooth variant of interval iteration: Instead of taking over the bounds computed by the Bellman operator right away, the bounds only move
     * towards the computed values. The parameter gamma controls how far they move (the parameter delta is currently not used by the update).
     * @see https://doi.org/10.1007/978-3-031-90653-4_7
     */
    SolverStatus smoothII(std::pair<std::vector<ValueType>, std::vector<ValueType>>& xy,
                          std::pair<std::vector<ValueType>, std::vector<ValueType>> const& offsets, uint64_t& numIterations, bool relative,
                          ValueType const& precision, ValueType const& gamma, ValueType const& delta,
                          std::optional<storm::OptimizationDirection> const& dir = {},
                          std::function<SolverStatus(IIData<ValueType> const&)> const& iterationCallback = {},
                          std::optional<storm::storage::BitVector> const& relevantValues = {}) const;

    /*!
     * Variant of interval iteration for floating point numbers with directed rounding.
     * The first vector of xy (and of offsets) holds the *negated* lower bounds, so that rounding upwards yields sound lower and upper bounds.
     * It is up to the caller to set the rounding mode of the floating point environment to upwards and to negate the result again.
     * If gamma is not zero, smooth updates are used (as in smoothII).
     * @see https://doi.org/10.1007/978-3-031-90653-4_7
     */
    SolverStatus roundII(std::pair<std::vector<ValueType>, std::vector<ValueType>>& xy,
                         std::pair<std::vector<ValueType>, std::vector<ValueType>> const& offsets, uint64_t& numIterations, bool relative,
                         ValueType const& precision, ValueType const& gamma, ValueType const& delta,
                         std::optional<storm::OptimizationDirection> const& dir = {},
                         std::function<SolverStatus(IIData<ValueType> const&)> const& iterationCallback = {},
                         std::optional<storm::storage::BitVector> const& relevantValues = {}) const;

    SolverStatus II(std::pair<std::vector<ValueType>, std::vector<ValueType>>& xy, std::pair<std::vector<ValueType>, std::vector<ValueType>> const& offsets,
                    uint64_t& numIterations, bool relative, ValueType const& precision, std::optional<storm::OptimizationDirection> const& dir = {},
                    std::function<SolverStatus(IIData<ValueType> const&)> const& iterationCallback = {},
                    std::optional<storm::storage::BitVector> const& relevantValues = {}) const;

    SolverStatus II(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative, ValueType const& precision,
                    std::function<void(std::vector<ValueType>&)> const& prepareLowerBounds,
                    std::function<void(std::vector<ValueType>&)> const& prepareUpperBounds, std::optional<storm::OptimizationDirection> const& dir = {},
                    std::function<SolverStatus(IIData<ValueType> const&)> const& iterationCallback = {},
                    std::optional<storm::storage::BitVector> const& relevantValues = {},
                    storm::OptionalRef<SolutionBounds<ValueType>> solutionBounds = storm::NullRef) const;

    SolverStatus II(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, bool relative, ValueType const& precision,
                    std::function<void(std::vector<ValueType>&)> const& prepareLowerBounds,
                    std::function<void(std::vector<ValueType>&)> const& prepareUpperBounds, std::optional<storm::OptimizationDirection> const& dir = {},
                    std::function<SolverStatus(IIData<ValueType> const&)> const& iterationCallback = {},
                    std::optional<storm::storage::BitVector> const& relevantValues = {}) const;

   private:
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator;
};

}  // namespace storm::solver::helper
