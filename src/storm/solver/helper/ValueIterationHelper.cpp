#include "storm/solver/helper/ValueIterationHelper.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/solver/helper/ValueIterationOperator.h"
#include "storm/utility/Extremum.h"

namespace storm::solver::helper {

template<typename ValueType, storm::OptimizationDirection Dir, bool Relative>
class VIOperatorBackend {
   public:
    VIOperatorBackend(ValueType const& precision) : precision{precision} {
        // intentionally empty
    }

    void startNewIteration() {
        isConverged = true;
    }

    void firstRow(ValueType&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        best = std::move(value);
    }

    void nextRow(ValueType&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        best &= value;
    }

    void applyUpdate(ValueType& currValue, [[maybe_unused]] uint64_t rowGroup) {
        if (isConverged) {
            if constexpr (Relative) {
                isConverged = storm::utility::abs<ValueType>(currValue - *best) <= storm::utility::abs<ValueType>(precision * currValue);
            } else {
                isConverged = storm::utility::abs<ValueType>(currValue - *best) <= precision;
            }
        }
        currValue = std::move(*best);
    }

    void endOfIteration() const {
        // intentionally left empty.
    }

    bool converged() const {
        return isConverged;
    }

    bool constexpr abort() const {
        return false;
    }

   private:
    storm::utility::Extremum<Dir, ValueType> best;
    ValueType const precision;
    bool isConverged{true};
};

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
ValueIterationHelper<ValueType, TrivialRowGrouping, SolutionType>::ValueIterationHelper(
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping, SolutionType>> viOperator)
    : viOperator(viOperator) {
    // Intentionally left empty
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
template<storm::OptimizationDirection Dir, bool Relative, storm::OptimizationDirection RobustDir>
SolverStatus ValueIterationHelper<ValueType, TrivialRowGrouping, SolutionType>::VI(std::vector<SolutionType>& operand, std::vector<ValueType> const& offsets,
                                                                                   uint64_t& numIterations, SolutionType const& precision,
                                                                                   std::function<SolverStatus(SolverStatus const&)> const& iterationCallback,
                                                                                   MultiplicationStyle mult) const {
    VIOperatorBackend<SolutionType, Dir, Relative> backend{precision};
    std::vector<SolutionType>* operand1{&operand};
    std::vector<SolutionType>* operand2{&operand};
    if (mult == MultiplicationStyle::Regular) {
        operand2 = &viOperator->allocateAuxiliaryVector(operand.size());
    }
    bool resultInAuxVector{false};
    SolverStatus status{SolverStatus::InProgress};
    while (status == SolverStatus::InProgress) {
        ++numIterations;
        bool applyResult = viOperator->template applyRobust<RobustDir>(*operand1, *operand2, offsets, backend);
        if (applyResult) {
            status = SolverStatus::Converged;
        } else if (iterationCallback) {
            status = iterationCallback(status);
        }
        if (mult == MultiplicationStyle::Regular) {
            std::swap(operand1, operand2);
            resultInAuxVector = !resultInAuxVector;
        }
    }
    if (mult == MultiplicationStyle::Regular) {
        if (resultInAuxVector) {
            STORM_LOG_ASSERT(&operand == operand2, "Unexpected operand address");
            std::swap(*operand1, *operand2);
        }
        viOperator->freeAuxiliaryVector();
    }
    return status;
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
template<storm::OptimizationDirection Dir, bool Relative>
SolverStatus ValueIterationHelper<ValueType, TrivialRowGrouping, SolutionType>::VI(std::vector<SolutionType>& operand, std::vector<ValueType> const& offsets,
                                                                                   uint64_t& numIterations, SolutionType const& precision,
                                                                                   const std::function<SolverStatus(const SolverStatus&)>& iterationCallback,
                                                                                   MultiplicationStyle mult,
                                                                                   UncertaintyResolutionMode const& uncertaintyResolutionMode) const {
    bool robustUncertainty = false;
    if constexpr (storm::IsIntervalType<ValueType>) {
        robustUncertainty = isUncertaintyResolvedRobust(uncertaintyResolutionMode, Dir);
    }

    if (robustUncertainty) {
        return VI<Dir, Relative, invert(Dir)>(operand, offsets, numIterations, precision, iterationCallback, mult);
    } else {
        return VI<Dir, Relative, Dir>(operand, offsets, numIterations, precision, iterationCallback, mult);
    }
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
SolverStatus ValueIterationHelper<ValueType, TrivialRowGrouping, SolutionType>::VI(std::vector<SolutionType>& operand, std::vector<ValueType> const& offsets,
                                                                                   uint64_t& numIterations, bool relative, SolutionType const& precision,
                                                                                   std::optional<storm::OptimizationDirection> const& dir,
                                                                                   std::function<SolverStatus(SolverStatus const&)> const& iterationCallback,
                                                                                   MultiplicationStyle mult,
                                                                                   UncertaintyResolutionMode const& uncertaintyResolutionMode) const {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(uncertaintyResolutionMode != UncertaintyResolutionMode::Unset, storm::exceptions::IllegalFunctionCallException,
                        "Uncertainty resolution mode must be set for uncertain (interval) models.");
        STORM_LOG_THROW(dir.has_value() || (uncertaintyResolutionMode != UncertaintyResolutionMode::Robust &&
                                            uncertaintyResolutionMode != UncertaintyResolutionMode::Cooperative),
                        storm::exceptions::IllegalFunctionCallException,
                        "Robust or cooperative nature resolution modes cannot be used if optimization direction is not set.");
    }

    STORM_LOG_ASSERT(TrivialRowGrouping || dir.has_value(), "no optimization direction given!");
    if (!dir.has_value() || maximize(*dir)) {
        if (relative) {
            return VI<storm::OptimizationDirection::Maximize, true>(operand, offsets, numIterations, precision, iterationCallback, mult,
                                                                    uncertaintyResolutionMode);
        } else {
            return VI<storm::OptimizationDirection::Maximize, false>(operand, offsets, numIterations, precision, iterationCallback, mult,
                                                                     uncertaintyResolutionMode);
        }
    } else {
        if (relative) {
            return VI<storm::OptimizationDirection::Minimize, true>(operand, offsets, numIterations, precision, iterationCallback, mult,
                                                                    uncertaintyResolutionMode);
        } else {
            return VI<storm::OptimizationDirection::Minimize, false>(operand, offsets, numIterations, precision, iterationCallback, mult,
                                                                     uncertaintyResolutionMode);
        }
    }
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
SolverStatus ValueIterationHelper<ValueType, TrivialRowGrouping, SolutionType>::VI(std::vector<SolutionType>& operand, std::vector<ValueType> const& offsets,
                                                                                   bool relative, SolutionType const& precision,
                                                                                   std::optional<storm::OptimizationDirection> const& dir,
                                                                                   std::function<SolverStatus(SolverStatus const&)> const& iterationCallback,
                                                                                   MultiplicationStyle mult,
                                                                                   UncertaintyResolutionMode const& uncertaintyResolutionMode) const {
    uint64_t numIterations = 0;
    return VI(operand, offsets, numIterations, relative, precision, dir, iterationCallback, mult, uncertaintyResolutionMode);
}

template class ValueIterationHelper<double, true>;
template class ValueIterationHelper<double, false>;
template class ValueIterationHelper<storm::RationalNumber, true>;
template class ValueIterationHelper<storm::RationalNumber, false>;
template class ValueIterationHelper<storm::Interval, true, double>;
template class ValueIterationHelper<storm::Interval, false, double>;
template class ValueIterationHelper<storm::RationalInterval, true, storm::RationalNumber>;
template class ValueIterationHelper<storm::RationalInterval, false, storm::RationalNumber>;

}  // namespace storm::solver::helper
