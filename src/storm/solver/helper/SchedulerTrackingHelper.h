#pragma once
#include <memory>
#include <vector>

#include <boost/optional.hpp>

#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/UncertaintyResolutionMode.h"
#include "storm/solver/helper/ValueIterationOperatorForward.h"

namespace storm::solver::helper {

template<typename ValueType, storm::OptimizationDirection Dir, bool TrivialRowGrouping>
class RobustSchedulerTrackingBackend {
   public:
    RobustSchedulerTrackingBackend(std::vector<uint64_t>& schedulerStorage, std::vector<uint64_t> const& robustIndices, bool applyUpdates)
        : schedulerStorage(schedulerStorage), robustIndices(robustIndices), applyUpdates(applyUpdates) {
        // intentionally empty
    }
    void startNewIteration() {
        isConverged = true;
    }

    void processRobustRow(ValueType&& value, uint64_t row, std::vector<std::pair<ValueType, std::pair<ValueType, uint64_t>>> const& info) {
        currStart = robustIndices[row];
        for (uint64_t i = 0; i < info.size(); ++i) {
            isConverged &= schedulerStorage[currStart + i] == info[i].second.second;
            schedulerStorage[currStart + i] = info[i].second.second;
        }
        best = value;
    }

    void applyUpdate(ValueType& currValue, uint64_t /* rowGroup */) {
        if (applyUpdates) {
            currValue = best;
        }
    }

    void endOfIteration() const {}

    bool converged() const {
        return isConverged;
    }

    bool constexpr abort() const {
        return false;
    }

   private:
    std::vector<uint64_t>& schedulerStorage;
    std::vector<uint64_t> const& robustIndices;

    ValueType best;

    uint64_t currStart;

    bool const applyUpdates;

    bool isConverged;

    std::vector<std::pair<uint64_t, ValueType>> currValues;
};

/*!
 * Helper class to extract optimal scheduler choices from a MinMax equation system solution
 */
template<typename ValueType, typename SolutionType = ValueType, bool TrivialRowGrouping = false>
class SchedulerTrackingHelper {
   public:
    /*!
     * Initializes this helper with the given value iteration operator
     */
    SchedulerTrackingHelper(std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping, SolutionType>> viOperator);

    /*!
     * Computes the optimal choices from the given solution.
     * Essentially, this applies one iteration of value iteration and stores the optimal choice for each state.
     * @param operandIn Operand for the value iteration step. Should hold the solution values of the MinMax equation system
     * @param offsets Offsets that are added to each choice result.
     * @param dir Optimization direction to consider.
     * @param schedulerStorage where the scheduler choices will be stored. Should have the same size as the operand(s).
     * @param uncertaintyResolutionMode Represents the mode indicating how the uncertainty should be resolved.
     * @param operandOut if given, the result values of the performed value iteration step will be stored in this vector. Can be the same as operandIn.
     * @return True if the scheduler coincides with the provided scheduler encoded in schedulerStorage
     *
     * @note: schedulers are encoded using row indices that are local to their row group, i.e. schedulerStorage[i]==j means that we choose the j'th row of row
     * group i
     */
    bool computeScheduler(std::vector<SolutionType>& operandIn, std::vector<ValueType> const& offsets, storm::OptimizationDirection const& dir,
                          std::vector<uint64_t>& schedulerStorage, UncertaintyResolutionMode uncertaintyResolutionMode,
                          std::vector<SolutionType>* operandOut = nullptr, boost::optional<std::vector<uint64_t>> const& robustIndices = boost::none) const;

   private:
    /*!
     * Internal variant of computeScheduler
     */
    template<storm::OptimizationDirection Dir, storm::OptimizationDirection RobustDir>
    bool computeScheduler(std::vector<SolutionType>& operandIn, std::vector<ValueType> const& offsets, std::vector<uint64_t>& schedulerStorage,
                          std::vector<SolutionType>* operandOut, boost::optional<std::vector<uint64_t>> const& robustIndices = boost::none) const;

   private:
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping, SolutionType>> viOperator;
};

}  // namespace storm::solver::helper
