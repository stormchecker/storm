#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "storm/solver/MultiplicationStyle.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/SolverStatus.h"
#include "storm/solver/UncertaintyResolutionMode.h"
#include "storm/solver/helper/ValueIterationOperatorForward.h"

namespace storm::solver::helper {

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType = ValueType>
class ValueIterationHelper {
   public:
    explicit ValueIterationHelper(std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping, SolutionType>> viOperator);

    template<storm::OptimizationDirection Dir, bool Relative, storm::OptimizationDirection RobustDir>
    SolverStatus VI(std::vector<SolutionType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, SolutionType const& precision,
                    std::function<SolverStatus(SolverStatus const&)> const& iterationCallback = {},
                    MultiplicationStyle mult = MultiplicationStyle::GaussSeidel) const;

    template<storm::OptimizationDirection Dir, bool Relative>
    SolverStatus VI(std::vector<SolutionType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, SolutionType const& precision,
                    const std::function<SolverStatus(const SolverStatus&)>& iterationCallback = {}, MultiplicationStyle mult = MultiplicationStyle::GaussSeidel,
                    UncertaintyResolutionMode const& uncertaintyResolutionMode = UncertaintyResolutionMode::Unset) const;

    SolverStatus VI(std::vector<SolutionType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative,
                    SolutionType const& precision, std::optional<storm::OptimizationDirection> const& dir = {},
                    std::function<SolverStatus(SolverStatus const&)> const& iterationCallback = {}, MultiplicationStyle mult = MultiplicationStyle::GaussSeidel,
                    UncertaintyResolutionMode const& uncertaintyResolutionMode = UncertaintyResolutionMode::Unset) const;

    SolverStatus VI(std::vector<SolutionType>& operand, std::vector<ValueType> const& offsets, bool relative, SolutionType const& precision,
                    std::optional<storm::OptimizationDirection> const& dir = {}, std::function<SolverStatus(SolverStatus const&)> const& iterationCallback = {},
                    MultiplicationStyle mult = MultiplicationStyle::GaussSeidel,
                    UncertaintyResolutionMode const& uncertaintyResolutionMode = UncertaintyResolutionMode::Unset) const;

   private:
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping, SolutionType>> viOperator;
};

}  // namespace storm::solver::helper
