#include "storm/solver/helper/RationalSearchHelper.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/solver/helper/ValueIterationHelper.h"
#include "storm/solver/helper/ValueIterationOperator.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/KwekMehlhorn.h"
#include "storm/utility/vector.h"

namespace storm::solver::helper {

template<typename TargetValueType, typename ExactValueType, typename ImpreciseValueType, bool TrivialRowGrouping>
RationalSearchHelper<TargetValueType, ExactValueType, ImpreciseValueType, TrivialRowGrouping>::RationalSearchHelper(
    std::shared_ptr<ValueIterationOperator<ExactValueType, TrivialRowGrouping>> exactViOperator,
    std::shared_ptr<ValueIterationOperator<ImpreciseValueType, TrivialRowGrouping>> impreciseViOperator)
    : exactOperator(exactViOperator), impreciseOperator(impreciseViOperator) {
    // Intentionally left empty
}

template<typename ValueType, typename ExactValueType, storm::OptimizationDirection Dir>
class RSBackend {
   public:
    void startNewIteration() {
        allEqual = true;
    }

    void firstRow(ExactValueType&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        best = std::move(value);
    }

    void nextRow(ExactValueType&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        best &= value;
    }

    void applyUpdate(ExactValueType& currValue, [[maybe_unused]] uint64_t rowGroup) {
        if (currValue != *best) {
            allEqual = false;
        }
    }

    void endOfIteration() const {
        // intentionally left empty.
    }

    bool converged() const {
        return allEqual;
    }

    bool constexpr abort() const {
        return !allEqual;
    }

   private:
    storm::utility::Extremum<Dir, ExactValueType> best;
    bool allEqual{true};
};

template<typename TargetValueType, typename ExactValueType, typename ImpreciseValueType, bool TrivialRowGrouping>
template<typename ValueType, storm::OptimizationDirection Dir>
RSResult RationalSearchHelper<TargetValueType, ExactValueType, ImpreciseValueType, TrivialRowGrouping>::sharpen(uint64_t precision,
                                                                                                                std::vector<ValueType> const& operand,
                                                                                                                std::vector<ExactValueType> const& exactOffsets,
                                                                                                                std::vector<TargetValueType>& target) const {
    RSBackend<ValueType, ExactValueType, Dir> backend;
    auto& sharpOperand = exactOperator->allocateAuxiliaryVector(operand.size());

    for (uint64_t p = 0; p <= precision; ++p) {
        // If we need an exact computation but are currently using imprecise values, we might need to consider switching to precise values
        if (std::is_same_v<ExactValueType, TargetValueType> && std::is_same_v<ValueType, ImpreciseValueType> &&
            p > std::numeric_limits<ImpreciseValueType>::max_digits10) {
            exactOperator->freeAuxiliaryVector();
            return RSResult::PrecisionExceeded;
        }
        storm::utility::kwek_mehlhorn::sharpen(p, operand, sharpOperand);

        if (exactOperator->applyInPlace(sharpOperand, exactOffsets, backend)) {
            // Put the solution into the target vector
            if constexpr (std::is_same_v<ExactValueType, TargetValueType>) {
                target.swap(sharpOperand);
            } else {
                target.resize(sharpOperand.size());
                storm::utility::vector::convertNumericVector(sharpOperand, target);
            }
            exactOperator->freeAuxiliaryVector();
            return RSResult::Converged;
        }
    }
    exactOperator->freeAuxiliaryVector();
    return RSResult::InProgress;
}

template<typename TargetValueType, typename ExactValueType, typename ImpreciseValueType, bool TrivialRowGrouping>
template<typename ValueType>
std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> const&
RationalSearchHelper<TargetValueType, ExactValueType, ImpreciseValueType, TrivialRowGrouping>::getOperator() const {
    if constexpr (std::is_same_v<ValueType, ExactValueType>) {
        return exactOperator;
    } else {
        return impreciseOperator;
    }
}

template<typename TargetValueType, typename ExactValueType, typename ImpreciseValueType, bool TrivialRowGrouping>
template<typename ValueType, storm::OptimizationDirection Dir>
std::pair<RSResult, SolverStatus> RationalSearchHelper<TargetValueType, ExactValueType, ImpreciseValueType, TrivialRowGrouping>::RS(
    std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, ExactValueType precision,
    std::vector<ExactValueType> const& exactOffsets, std::vector<TargetValueType>& target,
    std::function<SolverStatus(SolverStatus const&)> const& iterationCallback) const {
    ValueIterationHelper<ValueType, TrivialRowGrouping> viHelper(getOperator<ValueType>());
    SolverStatus status{SolverStatus::InProgress};
    RSResult result{RSResult::InProgress};
    while (status == SolverStatus::InProgress) {
        auto viStatus = viHelper.VI(operand, offsets, numIterations, false, storm::utility::convertNumber<ValueType>(precision), Dir, iterationCallback);

        // Compute maximal precision until which to sharpen.
        auto p = storm::utility::convertNumber<uint64_t>(
            storm::utility::ceil(storm::utility::log10<ExactValueType>(storm::utility::one<ExactValueType>() / precision)));

        // check if the sharpened vector is the desired solution.
        result = sharpen<ValueType, Dir>(p, operand, exactOffsets, target);
        switch (result) {
            case RSResult::Converged:
                status = SolverStatus::Converged;
                break;
            case RSResult::InProgress:
                if (viStatus != SolverStatus::Converged) {
                    status = viStatus;
                } else {
                    // Increase the precision.
                    precision /= storm::utility::convertNumber<ExactValueType>(static_cast<uint64_t>(10));
                }
                break;
            case RSResult::PrecisionExceeded:
                status = SolverStatus::Aborted;
                break;
        }
    }
    return std::pair(result, status);
}

template<typename TargetValueType, typename ExactValueType, typename ImpreciseValueType, bool TrivialRowGrouping>
template<storm::OptimizationDirection Dir>
SolverStatus RationalSearchHelper<TargetValueType, ExactValueType, ImpreciseValueType, TrivialRowGrouping>::RS(
    std::vector<TargetValueType>& operand, std::vector<TargetValueType> const& offsets, uint64_t& numIterations, TargetValueType const& precision,
    std::function<SolverStatus(SolverStatus const&)> const& iterationCallback) const {
    if constexpr (std::is_same_v<TargetValueType, ExactValueType>) {
        // We need an exact solution
        // We first try to solve the problem using imprecise values and fall back to exact values if needed.
        auto& impreciseOperand = impreciseOperator->allocateAuxiliaryVector(operand.size());
        storm::utility::vector::convertNumericVector(operand, impreciseOperand);
        auto impreciseOffsets = storm::utility::vector::convertNumericVector<ImpreciseValueType>(offsets);
        auto [result, status] = RS<ImpreciseValueType, Dir>(impreciseOperand, impreciseOffsets, numIterations, precision, offsets, operand, iterationCallback);
        if (result != RSResult::PrecisionExceeded) {
            return status;
        }
        STORM_LOG_WARN("Precision of value type was exceeded, trying to recover by switching to rational arithmetic.");
        storm::utility::vector::convertNumericVector(impreciseOperand, operand);
        return RS<TargetValueType, Dir>(operand, offsets, numIterations, precision, offsets, operand, iterationCallback).second;
    } else {
        // We only try with the inexact type
        auto exactOffsets = storm::utility::vector::convertNumericVector<ExactValueType>(offsets);
        return RS<TargetValueType, Dir>(operand, offsets, numIterations, storm::utility::convertNumber<ExactValueType>(precision), exactOffsets, operand,
                                        iterationCallback)
            .second;
    }
}

template<typename TargetValueType, typename ExactValueType, typename ImpreciseValueType, bool TrivialRowGrouping>
SolverStatus RationalSearchHelper<TargetValueType, ExactValueType, ImpreciseValueType, TrivialRowGrouping>::RS(
    std::vector<TargetValueType>& operand, std::vector<TargetValueType> const& offsets, uint64_t& numIterations, TargetValueType const& precision,
    std::optional<storm::OptimizationDirection> const& dir, std::function<SolverStatus(SolverStatus const&)> const& iterationCallback) const {
    STORM_LOG_ASSERT(TrivialRowGrouping || dir.has_value(), "No optimization direction given!");
    if (!dir.has_value() || maximize(*dir)) {
        return RS<storm::OptimizationDirection::Maximize>(operand, offsets, numIterations, precision, iterationCallback);
    } else {
        return RS<storm::OptimizationDirection::Minimize>(operand, offsets, numIterations, precision, iterationCallback);
    }
}

template<typename TargetValueType, typename ExactValueType, typename ImpreciseValueType, bool TrivialRowGrouping>
SolverStatus RationalSearchHelper<TargetValueType, ExactValueType, ImpreciseValueType, TrivialRowGrouping>::RS(
    std::vector<TargetValueType>& operand, std::vector<TargetValueType> const& offsets, TargetValueType const& precision,
    std::optional<storm::OptimizationDirection> const& dir, std::function<SolverStatus(SolverStatus const&)> const& iterationCallback) const {
    uint64_t numIterations = 0;
    return RS(operand, offsets, numIterations, precision, dir, iterationCallback);
}

template class RationalSearchHelper<double, storm::RationalNumber, double, true>;
template class RationalSearchHelper<double, storm::RationalNumber, double, false>;
template class RationalSearchHelper<storm::RationalNumber, storm::RationalNumber, double, true>;
template class RationalSearchHelper<storm::RationalNumber, storm::RationalNumber, double, false>;

}  // namespace storm::solver::helper
