#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/SolverStatus.h"

#include "storm/solver/helper/ValueIterationOperatorForward.h"

namespace storm::solver::helper {

/*!
 * Implements Optimistic value iteration
 * @see https://doi.org/10.1007/978-3-030-53291-8_26
 */
template<typename ValueType, bool TrivialRowGrouping>
class OptimisticValueIterationHelper {
   public:
    OptimisticValueIterationHelper(std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator);

    template<OptimizationDirection Dir, bool Relative>
    SolverStatus OVI(std::pair<std::vector<ValueType>, std::vector<ValueType>>& vu, std::vector<ValueType> const& offsets, uint64_t& numIterations,
                     ValueType const& precision, ValueType const& guessValue, std::optional<ValueType> const& lowerBound = {},
                     std::optional<ValueType> const& upperBound = {},
                     std::function<SolverStatus(SolverStatus const&, std::vector<ValueType> const&)> const& iterationCallback = {}) const;

    SolverStatus OVI(std::pair<std::vector<ValueType>, std::vector<ValueType>>& vu, std::vector<ValueType> const& offsets, uint64_t& numIterations,
                     bool relative, ValueType const& precision, std::optional<storm::OptimizationDirection> const& dir, ValueType const& guessValue,
                     std::optional<ValueType> const& lowerBound = {}, std::optional<ValueType> const& upperBound = {},
                     std::function<SolverStatus(SolverStatus const&, std::vector<ValueType> const&)> const& iterationCallback = {}) const;

    SolverStatus OVI(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative, ValueType const& precision,
                     std::optional<storm::OptimizationDirection> const& dir = {}, std::optional<ValueType> const& guessValue = {},
                     std::optional<ValueType> const& lowerBound = {}, std::optional<ValueType> const& upperBound = {},
                     std::function<SolverStatus(SolverStatus const&, std::vector<ValueType> const&)> const& iterationCallback = {}) const;

    SolverStatus OVI(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, bool relative, ValueType const& precision,
                     std::optional<storm::OptimizationDirection> const& dir = {}, std::optional<ValueType> const& guessValue = {},
                     std::optional<ValueType> const& lowerBound = {}, std::optional<ValueType> const& upperBound = {},
                     std::function<SolverStatus(SolverStatus const&, std::vector<ValueType> const&)> const& iterationCallback = {}) const;

   private:
    template<storm::OptimizationDirection Dir, bool Relative>
    SolverStatus GSVI(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, ValueType const& precision,
                      std::function<SolverStatus(SolverStatus const&, std::vector<ValueType> const&)> const& iterationCallback = {}) const;

    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator;
};

}  // namespace storm::solver::helper
