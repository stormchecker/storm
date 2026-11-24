#include "storm/solver/helper/DiscountedValueIterationHelper.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/solver/helper/ValueIterationOperator.h"
#include "storm/utility/Extremum.h"

namespace storm::solver::helper {

template<typename ValueType, storm::OptimizationDirection Dir, bool Relative>
class DiscountedVIOperatorBackend {
   public:
    DiscountedVIOperatorBackend(ValueType const& precision, ValueType const& discountFactor, ValueType const& maximalAbsoluteReward)
        : precision{precision},
          discountFactor{discountFactor},
          // We initialize the bound with a value guarantees preciseness when the difference between two iterations is less than it
          // See Russell, Norvig: Artificial Intelligence: A Modern Approach, 4th ed., p.583
          bound{(((storm::utility::one<ValueType>() - discountFactor) * precision) / (discountFactor))} {
        auto upper = storm::utility::log<ValueType>((2 * maximalAbsoluteReward) / (precision * (1 - discountFactor)));
        maxIterations = storm::utility::convertNumber<uint64_t>(storm::utility::ceil<ValueType>(upper / -storm::utility::log(discountFactor)));
        STORM_LOG_DEBUG("Maximum number of iterations: " << maxIterations);
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
                isConverged = storm::utility::abs<ValueType>(currValue - *best) <= storm::utility::abs<ValueType>(bound * currValue);
            } else {
                isConverged = storm::utility::abs<ValueType>(currValue - *best) <= bound;
            }
            // If we want to use the maximum number of iterations as the convergence criterion, we can use the following line
            // isConverged = currentIteration >= maxIterations;
            // In some preliminary experiments using only this criterion, performance was worse than with the check above.
        }
        currValue = std::move(*best);
    }

    void endOfIteration() {
        ++currentIteration;
    }

    bool converged() const {
        return isConverged;
    }

    bool constexpr abort() const {
        return false;
    }

   private:
    storm::utility::Extremum<Dir, ValueType> best;
    uint64_t currentIteration = 0;
    uint64_t maxIterations = 0;
    ValueType const precision;
    ValueType const discountFactor;
    ValueType const bound;
    bool isConverged{true};
};

template<typename ValueType, bool TrivialRowGrouping>
DiscountedValueIterationHelper<ValueType, TrivialRowGrouping>::DiscountedValueIterationHelper(
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator)
    : viOperator(viOperator) {
    // Intentionally left empty
}

template<typename ValueType, bool TrivialRowGrouping>
template<storm::OptimizationDirection Dir, bool Relative>
SolverStatus DiscountedValueIterationHelper<ValueType, TrivialRowGrouping>::DiscountedVI(
    std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, ValueType const& precision,
    ValueType const& discountFactor, ValueType const& maximalAbsoluteReward, std::function<SolverStatus(SolverStatus const&)> const& iterationCallback,
    MultiplicationStyle mult) const {
    DiscountedVIOperatorBackend<ValueType, Dir, Relative> backend{precision, discountFactor, maximalAbsoluteReward};
    std::vector<ValueType>* operand1{&operand};
    std::vector<ValueType>* operand2{&operand};
    if (mult == MultiplicationStyle::Regular) {
        operand2 = &viOperator->allocateAuxiliaryVector(operand.size());
    }
    bool resultInAuxVector{false};
    SolverStatus status{SolverStatus::InProgress};
    while (status == SolverStatus::InProgress) {
        ++numIterations;
        if (viOperator->apply(*operand1, *operand2, offsets, backend)) {
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
    return status;
}

template<typename ValueType, bool TrivialRowGrouping>
SolverStatus DiscountedValueIterationHelper<ValueType, TrivialRowGrouping>::DiscountedVI(
    std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative, ValueType const& precision,
    ValueType const& discountFactor, ValueType const& maximalAbsoluteReward, std::optional<storm::OptimizationDirection> const& dir,
    std::function<SolverStatus(SolverStatus const&)> const& iterationCallback, MultiplicationStyle mult) const {
    STORM_LOG_ASSERT(TrivialRowGrouping || dir.has_value(), "No optimization direction given!");
    if (!dir.has_value() || maximize(*dir)) {
        if (relative) {
            return DiscountedVI<storm::OptimizationDirection::Maximize, true>(operand, offsets, numIterations, precision, discountFactor, maximalAbsoluteReward,
                                                                              iterationCallback, mult);
        } else {
            return DiscountedVI<storm::OptimizationDirection::Maximize, false>(operand, offsets, numIterations, precision, discountFactor,
                                                                               maximalAbsoluteReward, iterationCallback, mult);
        }
    } else {
        if (relative) {
            return DiscountedVI<storm::OptimizationDirection::Minimize, true>(operand, offsets, numIterations, precision, discountFactor, maximalAbsoluteReward,
                                                                              iterationCallback, mult);
        } else {
            return DiscountedVI<storm::OptimizationDirection::Minimize, false>(operand, offsets, numIterations, precision, discountFactor,
                                                                               maximalAbsoluteReward, iterationCallback, mult);
        }
    }
}

template<typename ValueType, bool TrivialRowGrouping>
SolverStatus DiscountedValueIterationHelper<ValueType, TrivialRowGrouping>::DiscountedVI(
    std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, bool relative, ValueType const& precision, ValueType const& discountFactor,
    ValueType const& maximalAbsoluteReward, std::optional<storm::OptimizationDirection> const& dir,
    std::function<SolverStatus(SolverStatus const&)> const& iterationCallback, MultiplicationStyle mult) const {
    uint64_t numIterations = 0;
    return DiscountedVI(operand, offsets, numIterations, relative, precision, discountFactor, maximalAbsoluteReward, dir, iterationCallback, mult);
}

template class DiscountedValueIterationHelper<double, true>;
template class DiscountedValueIterationHelper<double, false>;
template class DiscountedValueIterationHelper<storm::RationalNumber, true>;
template class DiscountedValueIterationHelper<storm::RationalNumber, false>;

}  // namespace storm::solver::helper
