#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/SolverStatus.h"
#include "storm/solver/helper/ValueIterationOperatorForward.h"
#include "storm/storage/BitVector.h"

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

    template<OptimizationDirection Dir>
    SolverStatus II(std::pair<std::vector<ValueType>, std::vector<ValueType>>& xy, std::vector<ValueType> const& offsets, uint64_t& numIterations,
                    bool relative, ValueType const& precision, std::function<SolverStatus(IIData<ValueType> const&)> const& iterationCallback = {},
                    std::optional<storm::storage::BitVector> const& relevantValues = {}) const;

    SolverStatus II(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative, ValueType const& precision,
                    std::function<void(std::vector<ValueType>&)> const& prepareLowerBounds,
                    std::function<void(std::vector<ValueType>&)> const& prepareUpperBounds, std::optional<storm::OptimizationDirection> const& dir = {},
                    std::function<SolverStatus(IIData<ValueType> const&)> const& iterationCallback = {},
                    std::optional<storm::storage::BitVector> const& relevantValues = {}) const;

    SolverStatus II(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, bool relative, ValueType const& precision,
                    std::function<void(std::vector<ValueType>&)> const& prepareLowerBounds,
                    std::function<void(std::vector<ValueType>&)> const& prepareUpperBounds, std::optional<storm::OptimizationDirection> const& dir = {},
                    std::function<SolverStatus(IIData<ValueType> const&)> const& iterationCallback = {},
                    std::optional<storm::storage::BitVector> const& relevantValues = {}) const;

   private:
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator;
};

}  // namespace storm::solver::helper
