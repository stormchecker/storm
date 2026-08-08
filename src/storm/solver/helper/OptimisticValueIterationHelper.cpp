#include "storm/solver/helper/OptimisticValueIterationHelper.h"

#include <type_traits>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/solver/helper/ValueIterationOperator.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm::solver::helper {

template<bool Relative, typename ValueType>
static ValueType diff(ValueType const& oldValue, ValueType const& newValue) {
    if constexpr (Relative) {
        return storm::utility::abs<ValueType>((newValue - oldValue) / newValue);
    } else {
        return storm::utility::abs<ValueType>(newValue - oldValue);
    }
}

template<typename ValueType, storm::OptimizationDirection Dir, bool Relative>
class GSVIBackend {
   public:
    GSVIBackend(ValueType const& precision) : precision{precision} {
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
            isConverged = storm::utility::isZero(*best) || diff<Relative>(currValue, *best) <= precision;
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

template<typename ValueType, bool TrivialRowGrouping>
template<storm::OptimizationDirection Dir, bool Relative>
SolverStatus OptimisticValueIterationHelper<ValueType, TrivialRowGrouping>::GSVI(
    std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, ValueType const& precision,
    std::function<SolverStatus(SolverStatus const&, std::vector<ValueType> const&)> const& iterationCallback) const {
    GSVIBackend<ValueType, Dir, Relative> backend{precision};
    SolverStatus status{SolverStatus::InProgress};
    while (status == SolverStatus::InProgress) {
        ++numIterations;
        if (viOperator->applyInPlace(operand, offsets, backend)) {
            status = SolverStatus::Converged;
        } else if (iterationCallback) {
            status = iterationCallback(status, operand);
        }
    }
    return status;
}

template<bool Relative, typename ValueType>
void guessCandidate(std::pair<std::vector<ValueType>, std::vector<ValueType>>& vu, ValueType const& guessValue, std::optional<ValueType> const& lowerBound,
                    std::optional<ValueType> const& upperBound) {
    std::function<ValueType(ValueType const&)> guess;
    [[maybe_unused]] ValueType factor = storm::utility::one<ValueType>() + guessValue;
    if constexpr (Relative) {
        // the guess is given by value + |value * guessValue|. If all values are positive, this can be simplified a bit
        if (lowerBound && *lowerBound < storm::utility::zero<ValueType>()) {
            guess = [&guessValue](ValueType const& val) { return val + storm::utility::abs<ValueType>(val * guessValue); };
        } else {
            guess = [&factor](ValueType const& val) { return val * factor; };
        }
    } else {
        guess = [&guessValue](ValueType const& val) { return storm::utility::isZero(val) ? storm::utility::zero<ValueType>() : val + guessValue; };
    }
    if (lowerBound || upperBound) {
        std::function<ValueType(ValueType const&)> guessAndClamp;
        if (!lowerBound) {
            guessAndClamp = [&guess, &upperBound](ValueType const& val) { return std::min(guess(val), *upperBound); };
        } else if (!upperBound) {
            guessAndClamp = [&guess, &lowerBound](ValueType const& val) { return std::max(guess(val), *lowerBound); };
        } else {
            guessAndClamp = [&guess, &lowerBound, &upperBound](ValueType const& val) { return std::clamp(guess(val), *lowerBound, *upperBound); };
        }
        storm::utility::vector::applyPointwise(vu.first, vu.second, guessAndClamp);
    } else {
        storm::utility::vector::applyPointwise(vu.first, vu.second, guess);
    }
}

template<typename ValueType, OptimizationDirection Dir, bool Relative>
class OVIBackend {
   public:
    void startNewIteration() {
        isAllUp = true;
        isAllDown = true;
        crossed = false;
        errorValue = storm::utility::zero<ValueType>();
    }

    void firstRow(std::pair<ValueType, ValueType>&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        vBest = std::move(value.first);
        uBest = std::move(value.second);
    }

    void nextRow(std::pair<ValueType, ValueType>&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        vBest &= std::move(value.first);
        uBest &= std::move(value.second);
    }

    void applyUpdate(ValueType& vCurr, ValueType& uCurr, [[maybe_unused]] uint64_t rowGroup) {
        if (*vBest != storm::utility::zero<ValueType>()) {
            errorValue &= diff<Relative>(vCurr, *vBest);
        }
        if (*uBest < uCurr) {
            uCurr = *uBest;
            isAllUp = false;
        } else if (*uBest > uCurr) {
            isAllDown = false;
        }
        vCurr = *vBest;
        if (vCurr > uCurr) {
            crossed = true;
        }
    }

    void endOfIteration() const {
        // intentionally left empty.
    }

    bool converged() const {
        return isAllDown || isAllUp;
    }

    bool allUp() const {
        return isAllUp;
    }

    bool allDown() const {
        return isAllDown;
    }

    bool abort() const {
        return crossed;
    }

    ValueType error() {
        return *errorValue;
    }

   private:
    bool isAllUp{true};
    bool isAllDown{true};
    bool crossed{false};
    storm::utility::Extremum<Dir, ValueType> vBest, uBest;
    storm::utility::Extremum<OptimizationDirection::Maximize, ValueType> errorValue;
};

template<typename ValueType, bool TrivialRowGrouping>
OptimisticValueIterationHelper<ValueType, TrivialRowGrouping>::OptimisticValueIterationHelper(
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator)
    : viOperator(viOperator) {
    // Intentionally left empty.
}

template<typename ValueType, bool TrivialRowGrouping>
template<OptimizationDirection Dir, bool Relative>
SolverStatus OptimisticValueIterationHelper<ValueType, TrivialRowGrouping>::OVI(
    std::pair<std::vector<ValueType>, std::vector<ValueType>>& vu, std::vector<ValueType> const& offsets, uint64_t& numIterations, ValueType const& precision,
    ValueType const& guessValue, std::optional<ValueType> const& lowerBound, std::optional<ValueType> const& upperBound,
    std::function<SolverStatus(SolverStatus const&, std::vector<ValueType> const&)> const& iterationCallback) const {
    ValueType currentGuessValue = guessValue;
    for (uint64_t numTries = 1; true; ++numTries) {
        if (SolverStatus status = GSVI<Dir, Relative>(vu.first, offsets, numIterations, currentGuessValue, iterationCallback);
            status != SolverStatus::Converged) {
            return status;
        }
        guessCandidate<Relative>(vu, precision, lowerBound, upperBound);
        OVIBackend<ValueType, Dir, Relative> backend;
        uint64_t maxIters;
        if (storm::utility::isZero(currentGuessValue)) {
            maxIters = std::numeric_limits<uint64_t>::max();
        } else {
            maxIters = numIterations + storm::utility::convertNumber<uint64_t, ValueType>(
                                           storm::utility::ceil<ValueType>(storm::utility::one<ValueType>() / currentGuessValue));
        }
        while (numIterations < maxIters) {
            ++numIterations;
            if (viOperator->applyInPlace(vu, offsets, backend)) {
                if (backend.allDown()) {
                    return SolverStatus::Converged;
                } else {
                    STORM_LOG_ASSERT(backend.allUp(), "Expected all values to be up.");
                    break;
                }
            }
            if (backend.abort()) {
                break;
            }
            if (iterationCallback) {
                if (auto status = iterationCallback(SolverStatus::InProgress, vu.first); status != SolverStatus::InProgress) {
                    return status;
                }
            }
        }
        STORM_LOG_WARN_COND(numTries != 20, "Optimistic Value Iteration did not terminate after 20 refinements. It might be stuck.");
        currentGuessValue = backend.error() / storm::utility::convertNumber<ValueType, uint64_t>(2u);
    }
}

template<typename ValueType, bool TrivialRowGrouping>
SolverStatus OptimisticValueIterationHelper<ValueType, TrivialRowGrouping>::OVI(
    std::pair<std::vector<ValueType>, std::vector<ValueType>>& vu, std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative,
    ValueType const& precision, std::optional<storm::OptimizationDirection> const& dir, ValueType const& guessValue, std::optional<ValueType> const& lowerBound,
    std::optional<ValueType> const& upperBound,
    std::function<SolverStatus(SolverStatus const&, std::vector<ValueType> const&)> const& iterationCallback) const {
    // Catch the case where lower- and upper bound are already close enough. (when guessing candidates, OVI handles this case not very well, in particular
    // when lowerBound==upperBound)
    if (lowerBound && upperBound) {
        ValueType diff = *upperBound - *lowerBound;
        if ((relative && diff <= precision * std::min(storm::utility::abs(*lowerBound), storm::utility::abs(*upperBound))) ||
            (!relative && diff <= precision)) {
            vu.first.assign(vu.first.size(), *lowerBound);
            vu.second.assign(vu.second.size(), *upperBound);
            return SolverStatus::Converged;
        }
    }

    if (!dir.has_value() || maximize(*dir)) {
        if (relative) {
            return OVI<OptimizationDirection::Maximize, true>(vu, offsets, numIterations, precision, guessValue, lowerBound, upperBound, iterationCallback);
        } else {
            return OVI<OptimizationDirection::Maximize, false>(vu, offsets, numIterations, precision, guessValue, lowerBound, upperBound, iterationCallback);
        }
    } else {
        if (relative) {
            return OVI<OptimizationDirection::Minimize, true>(vu, offsets, numIterations, precision, guessValue, lowerBound, upperBound, iterationCallback);
        } else {
            return OVI<OptimizationDirection::Minimize, false>(vu, offsets, numIterations, precision, guessValue, lowerBound, upperBound, iterationCallback);
        }
    }
}

template<typename ValueType, bool TrivialRowGrouping>
SolverStatus OptimisticValueIterationHelper<ValueType, TrivialRowGrouping>::OVI(
    std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative, ValueType const& precision,
    std::optional<storm::OptimizationDirection> const& dir, std::optional<ValueType> const& guessValue, std::optional<ValueType> const& lowerBound,
    std::optional<ValueType> const& upperBound,
    std::function<SolverStatus(SolverStatus const&, std::vector<ValueType> const&)> const& iterationCallback) const {
    // Create two vectors v and u using the given operand plus an auxiliary vector.
    std::pair<std::vector<ValueType>, std::vector<ValueType>> vu;
    auto& auxVector = viOperator->allocateAuxiliaryVector(operand.size());
    vu.first.swap(operand);
    vu.second.swap(auxVector);
    auto doublePrec = precision + precision;
    if constexpr (std::is_same_v<ValueType, double>) {
        doublePrec -= precision * 1e-6;  // be slightly more precise to avoid a good chunk of floating point issues
    }
    auto status = OVI(vu, offsets, numIterations, relative, doublePrec, dir, guessValue ? *guessValue : doublePrec, lowerBound, upperBound, iterationCallback);
    auto two = storm::utility::convertNumber<ValueType>(2.0);
    // get the average of lower- and upper result
    storm::utility::vector::applyPointwise<ValueType, ValueType, ValueType>(
        vu.first, vu.second, vu.first, [&two](ValueType const& a, ValueType const& b) -> ValueType { return (a + b) / two; });
    // Swap operand and aux vector back to original positions.
    vu.first.swap(operand);
    vu.second.swap(auxVector);
    viOperator->freeAuxiliaryVector();
    return status;
}

template<typename ValueType, bool TrivialRowGrouping>
SolverStatus OptimisticValueIterationHelper<ValueType, TrivialRowGrouping>::OVI(
    std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, bool relative, ValueType const& precision,
    std::optional<storm::OptimizationDirection> const& dir, std::optional<ValueType> const& guessValue, std::optional<ValueType> const& lowerBound,
    std::optional<ValueType> const& upperBound,
    std::function<SolverStatus(SolverStatus const&, std::vector<ValueType> const&)> const& iterationCallback) const {
    uint64_t numIterations = 0;
    return OVI(operand, offsets, numIterations, relative, precision, dir, guessValue, lowerBound, upperBound, iterationCallback);
}

template class OptimisticValueIterationHelper<double, true>;
template class OptimisticValueIterationHelper<double, false>;
template class OptimisticValueIterationHelper<storm::RationalNumber, true>;
template class OptimisticValueIterationHelper<storm::RationalNumber, false>;

}  // namespace storm::solver::helper
