#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/SolverStatus.h"

#include "storm/solver/helper/ValueIterationOperatorForward.h"

namespace storm::solver::helper {

enum class RSResult { InProgress, Converged, PrecisionExceeded };

/*!
 * Implements rational search
 * @see https://doi.org/10.1007/s10703-020-00348-y
 * @tparam TargetValueType The value type in which we want the result to be in
 * @tparam ExactValueType Value type used for exact computations
 * @tparam ImpreciseValueType value type used for fast, but inexact computatioins
 */
template<typename TargetValueType, typename ExactValueType, typename ImpreciseValueType, bool TrivialRowGrouping>
class RationalSearchHelper {
   public:
    static const bool IsTargetExact = std::is_same_v<TargetValueType, ExactValueType>;

    RationalSearchHelper(std::shared_ptr<ValueIterationOperator<ExactValueType, TrivialRowGrouping>> exactViOperator,
                         std::shared_ptr<ValueIterationOperator<ImpreciseValueType, TrivialRowGrouping>> impreciseViOperator);

    template<storm::OptimizationDirection Dir>
    SolverStatus RS(std::vector<TargetValueType>& operand, std::vector<TargetValueType> const& offsets, uint64_t& numIterations,
                    TargetValueType const& precision, std::function<SolverStatus(SolverStatus const&)> const& iterationCallback = {}) const;

    SolverStatus RS(std::vector<TargetValueType>& operand, std::vector<TargetValueType> const& offsets, uint64_t& numIterations,
                    TargetValueType const& precision, std::optional<storm::OptimizationDirection> const& dir = {},
                    std::function<SolverStatus(SolverStatus const&)> const& iterationCallback = {}) const;

    SolverStatus RS(std::vector<TargetValueType>& operand, std::vector<TargetValueType> const& offsets, TargetValueType const& precision,
                    std::optional<storm::OptimizationDirection> const& dir = {},
                    std::function<SolverStatus(SolverStatus const&)> const& iterationCallback = {}) const;

   private:
    template<typename ValueType, storm::OptimizationDirection Dir>
    std::pair<RSResult, SolverStatus> RS(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations,
                                         ExactValueType precision, std::vector<ExactValueType> const& exactOffsets, std::vector<TargetValueType>& target,
                                         std::function<SolverStatus(SolverStatus const&)> const& iterationCallback) const;

    template<typename ValueType>
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> const& getOperator() const;

    template<typename ValueType, storm::OptimizationDirection Dir>
    RSResult sharpen(uint64_t precision, std::vector<ValueType> const& operand, std::vector<ExactValueType> const& exactOffsets,
                     std::vector<TargetValueType>& target) const;

    std::shared_ptr<ValueIterationOperator<ExactValueType, TrivialRowGrouping>> exactOperator;
    std::shared_ptr<ValueIterationOperator<ImpreciseValueType, TrivialRowGrouping>> impreciseOperator;
};

}  // namespace storm::solver::helper
