#include "storm/solver/helper/ValueIterationHelper.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/solver/helper/ValueIterationOperator.h"
#include "storm/utility/Extremum.h"

namespace storm::solver::helper {

template<typename ValueType, storm::OptimizationDirection Dir, bool Relative, bool TrackDirection>
class VIOperatorBackend {
   public:
    VIOperatorBackend(ValueType const& precision) : precision{precision} {
        // intentionally empty
    }

    void startNewIteration() {
        isConverged = true;
        if constexpr (TrackDirection) {
            isNonDecreasing = true;
            isNonIncreasing = true;
        }
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
        if constexpr (TrackDirection) {
            // Record the direction this iteration moves the operand in; see the comment in ValueIterationHelper::VI.
            if (isNonDecreasing && *best < currValue) {
                isNonDecreasing = false;
            }
            if (isNonIncreasing && currValue < *best) {
                isNonIncreasing = false;
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

    bool nonDecreasing() const {
        return isNonDecreasing;
    }

    bool nonIncreasing() const {
        return isNonIncreasing;
    }

    bool constexpr abort() const {
        return false;
    }

   private:
    storm::utility::Extremum<Dir, ValueType> best;
    ValueType const precision;
    bool isConverged{true};
    bool isNonDecreasing{true};
    bool isNonIncreasing{true};
};

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
ValueIterationHelper<ValueType, TrivialRowGrouping, SolutionType>::ValueIterationHelper(
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping, SolutionType>> viOperator)
    : viOperator(viOperator) {
    // Intentionally left empty
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
template<storm::OptimizationDirection Dir, bool Relative, storm::OptimizationDirection RobustDir, bool TrackDirection>
SolverStatus ValueIterationHelper<ValueType, TrivialRowGrouping, SolutionType>::VI(std::vector<SolutionType>& operand, std::vector<ValueType> const& offsets,
                                                                                   uint64_t& numIterations, SolutionType const& precision,
                                                                                   std::function<SolverStatus(SolverStatus const&)> const& iterationCallback,
                                                                                   MultiplicationStyle mult,
                                                                                   storm::OptionalRef<SolutionBounds<SolutionType>> solutionBounds,
                                                                                   SolverGuarantee const& guarantee) const {
    VIOperatorBackend<SolutionType, Dir, Relative, TrackDirection> backend{precision};
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
            STORM_LOG_ASSERT(&operand == operand2, "Unexpected operand address.");
            std::swap(*operand1, *operand2);
        }
        viOperator->freeAuxiliaryVector();
    }
    if (!solutionBounds.has_value()) {
        return status;
    }
    if constexpr (TrackDirection) {
        if (mult == MultiplicationStyle::GaussSeidel) {
            // An iteration that decreased nothing gives x <= T(x), so x lies below the unique fixpoint; dually for
            // one that increased nothing.
            if (backend.nonDecreasing()) {
                solutionBounds->lower = operand;
            }
            if (backend.nonIncreasing()) {
                solutionBounds->upper = operand;
            }
        }
    } else if (guarantee == SolverGuarantee::LessOrEqual) {
        // The iteration maintains a guarantee that held for the initial operand, so the result carries it too.
        solutionBounds->lower = operand;
    } else if (guarantee == SolverGuarantee::GreaterOrEqual) {
        solutionBounds->upper = operand;
    }
    return status;
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
template<storm::OptimizationDirection Dir, bool Relative>
SolverStatus ValueIterationHelper<ValueType, TrivialRowGrouping, SolutionType>::VI(std::vector<SolutionType>& operand, std::vector<ValueType> const& offsets,
                                                                                   uint64_t& numIterations, SolutionType const& precision,
                                                                                   const std::function<SolverStatus(const SolverStatus&)>& iterationCallback,
                                                                                   MultiplicationStyle mult,
                                                                                   UncertaintyResolutionMode const& uncertaintyResolutionMode,
                                                                                   storm::OptionalRef<SolutionBounds<SolutionType>> solutionBounds,
                                                                                   SolverGuarantee const& guarantee) const {
    bool robustUncertainty = false;
    if constexpr (storm::IsIntervalType<ValueType>) {
        robustUncertainty = isUncertaintyResolvedRobust(uncertaintyResolutionMode, Dir);
    }

    // Reading the direction of every sweep costs in the innermost loop, so only do it where it is the sole route
    // to a bound: where one is wanted and the operand carries no guarantee that the iteration already maintains.
    bool const trackDirection = solutionBounds.has_value() && guarantee == SolverGuarantee::None;
    if (robustUncertainty) {
        if (trackDirection) {
            return VI<Dir, Relative, invert(Dir), true>(operand, offsets, numIterations, precision, iterationCallback, mult, solutionBounds, guarantee);
        }
        return VI<Dir, Relative, invert(Dir), false>(operand, offsets, numIterations, precision, iterationCallback, mult, solutionBounds, guarantee);
    } else {
        if (trackDirection) {
            return VI<Dir, Relative, Dir, true>(operand, offsets, numIterations, precision, iterationCallback, mult, solutionBounds, guarantee);
        }
        return VI<Dir, Relative, Dir, false>(operand, offsets, numIterations, precision, iterationCallback, mult, solutionBounds, guarantee);
    }
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
SolverStatus ValueIterationHelper<ValueType, TrivialRowGrouping, SolutionType>::VI(
    std::vector<SolutionType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative, SolutionType const& precision,
    std::optional<storm::OptimizationDirection> const& dir, std::function<SolverStatus(SolverStatus const&)> const& iterationCallback, MultiplicationStyle mult,
    UncertaintyResolutionMode const& uncertaintyResolutionMode, storm::OptionalRef<SolutionBounds<SolutionType>> solutionBounds,
    SolverGuarantee const& guarantee) const {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(uncertaintyResolutionMode != UncertaintyResolutionMode::Unset, storm::exceptions::IllegalFunctionCallException,
                        "Uncertainty resolution mode must be set for uncertain (interval) models.");
        STORM_LOG_THROW(dir.has_value() || (uncertaintyResolutionMode != UncertaintyResolutionMode::Robust &&
                                            uncertaintyResolutionMode != UncertaintyResolutionMode::Cooperative),
                        storm::exceptions::IllegalFunctionCallException,
                        "Robust or cooperative nature resolution modes cannot be used if optimization direction is not set.");
    }

    STORM_LOG_ASSERT(TrivialRowGrouping || dir.has_value(), "No optimization direction given!");
    if (!dir.has_value() || maximize(*dir)) {
        if (relative) {
            return VI<storm::OptimizationDirection::Maximize, true>(operand, offsets, numIterations, precision, iterationCallback, mult,
                                                                    uncertaintyResolutionMode, solutionBounds, guarantee);
        } else {
            return VI<storm::OptimizationDirection::Maximize, false>(operand, offsets, numIterations, precision, iterationCallback, mult,
                                                                     uncertaintyResolutionMode, solutionBounds, guarantee);
        }
    } else {
        if (relative) {
            return VI<storm::OptimizationDirection::Minimize, true>(operand, offsets, numIterations, precision, iterationCallback, mult,
                                                                    uncertaintyResolutionMode, solutionBounds, guarantee);
        } else {
            return VI<storm::OptimizationDirection::Minimize, false>(operand, offsets, numIterations, precision, iterationCallback, mult,
                                                                     uncertaintyResolutionMode, solutionBounds, guarantee);
        }
    }
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
SolverStatus ValueIterationHelper<ValueType, TrivialRowGrouping, SolutionType>::VI(
    std::vector<SolutionType>& operand, std::vector<ValueType> const& offsets, bool relative, SolutionType const& precision,
    std::optional<storm::OptimizationDirection> const& dir, std::function<SolverStatus(SolverStatus const&)> const& iterationCallback, MultiplicationStyle mult,
    UncertaintyResolutionMode const& uncertaintyResolutionMode, storm::OptionalRef<SolutionBounds<SolutionType>> solutionBounds,
    SolverGuarantee const& guarantee) const {
    uint64_t numIterations = 0;
    return VI(operand, offsets, numIterations, relative, precision, dir, iterationCallback, mult, uncertaintyResolutionMode, solutionBounds, guarantee);
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
