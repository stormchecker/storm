#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/SolutionBounds.h"
#include "storm/solver/SolverStatus.h"
#include "storm/solver/TerminationCondition.h"
#include "storm/solver/helper/ValueIterationOperatorForward.h"

namespace storm::solver::helper {

/*!
 * Implements sound value iteration
 * @see https://doi.org/10.1007/978-3-319-96145-3_37
 */
template<typename ValueType, bool TrivialRowGrouping>
class SoundValueIterationHelper {
   public:
    SoundValueIterationHelper(std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator);

    struct SVIData {
        SolverStatus status;
        std::pair<std::vector<ValueType>, std::vector<ValueType>> const& xy;
        std::optional<ValueType> const a, b;

        void trySetAverage(std::vector<ValueType>& out) const;

        /*!
         * Writes the enclosure of the solution that this data represents.
         * @return whether the two scaling factors it is built from are both known, i.e. whether anything was written.
         */
        bool trySetLowerUpper(std::vector<ValueType>& lowerOut, std::vector<ValueType>& upperOut) const;
        bool checkCustomTerminationCondition(storm::solver::TerminationCondition<ValueType> const& condition) const;

        bool checkConvergence(uint64_t& convergenceCheckState, std::function<void()> const& getNextConvergenceCheckState, bool relative,
                              ValueType const& precision) const;
    };

    template<typename BackendType>
    SVIData SVI(std::pair<std::vector<ValueType>, std::vector<ValueType>>& xy, std::pair<std::vector<ValueType> const*, ValueType> const& offsets,
                uint64_t& numIterations, bool relative, ValueType const& precision, BackendType&& backend,
                std::function<SolverStatus(SVIData const&)> const& iterationCallback, std::optional<storm::storage::BitVector> const& relevantValues,
                uint64_t convergenceCheckState = 0) const;

    template<storm::OptimizationDirection Dir>
    SVIData SVI(std::pair<std::vector<ValueType>, std::vector<ValueType>>& xy, std::pair<std::vector<ValueType> const*, ValueType> const& offsets,
                uint64_t& numIterations, bool relative, ValueType const& precision, std::optional<ValueType> const& a, std::optional<ValueType> const& b,
                std::function<SolverStatus(SVIData const&)> const& iterationCallback = {},
                std::optional<storm::storage::BitVector> const& relevantValues = {}) const;

    SVIData SVI(std::pair<std::vector<ValueType>, std::vector<ValueType>>& xy, std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative,
                ValueType const& precision, std::optional<storm::OptimizationDirection> const& dir, std::optional<ValueType> const& lowerBound,
                std::optional<ValueType> const& upperBound, std::function<SolverStatus(SVIData const&)> const& iterationCallback,
                std::optional<storm::storage::BitVector> const& relevantValues = {}) const;

    SolverStatus SVI(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative, ValueType const& precision,
                     std::optional<storm::OptimizationDirection> const& dir = {}, std::optional<ValueType> const& lowerBound = {},
                     std::optional<ValueType> const& upperBound = {}, std::function<SolverStatus(SVIData const&)> const& iterationCallback = {},
                     std::optional<storm::storage::BitVector> const& relevantValues = {}, SolutionBounds<ValueType>* solutionBounds = nullptr) const;

    SolverStatus SVI(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, bool relative, ValueType const& precision,
                     std::optional<storm::OptimizationDirection> const& dir = {}, std::optional<ValueType> const& lowerBound = {},
                     std::optional<ValueType> const& upperBound = {}, std::function<SolverStatus(SVIData const&)> const& iterationCallback = {},
                     std::optional<storm::storage::BitVector> const& relevantValues = {}, SolutionBounds<ValueType>* solutionBounds = nullptr) const;

   private:
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator;
    uint64_t sizeOfLargestRowGroup;
};

}  // namespace storm::solver::helper
