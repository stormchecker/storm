#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "storm/solver/MultiplicationStyle.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/SolverStatus.h"
#include "storm/solver/helper/ValueIterationOperatorForward.h"

namespace storm::solver::helper {

template<typename ValueType, bool TrivialRowGrouping>
class DiscountedValueIterationHelper {
   public:
    explicit DiscountedValueIterationHelper(std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator);

    template<storm::OptimizationDirection Dir, bool Relative>
    SolverStatus DiscountedVI(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, ValueType const& precision,
                              ValueType const& discountFactor, ValueType const& maximalAbsoluteReward,
                              std::function<SolverStatus(SolverStatus const&)> const& iterationCallback = {},
                              MultiplicationStyle mult = MultiplicationStyle::GaussSeidel) const;

    SolverStatus DiscountedVI(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative,
                              ValueType const& precision, ValueType const& discountFactor, ValueType const& maximalAbsoluteReward,
                              std::optional<storm::OptimizationDirection> const& dir = {},
                              std::function<SolverStatus(SolverStatus const&)> const& iterationCallback = {},
                              MultiplicationStyle mult = MultiplicationStyle::GaussSeidel) const;

    SolverStatus DiscountedVI(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, bool relative, ValueType const& precision,
                              ValueType const& discountFactor, ValueType const& maximalAbsoluteReward,
                              std::optional<storm::OptimizationDirection> const& dir = {},
                              std::function<SolverStatus(SolverStatus const&)> const& iterationCallback = {},
                              MultiplicationStyle mult = MultiplicationStyle::GaussSeidel) const;

   private:
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator;
};

}  // namespace storm::solver::helper
