#include "storm/solver/helper/SoundValueIterationHelper.h"

#include <type_traits>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/solver/helper/ValueIterationOperator.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm::solver::helper {

template<typename ValueType, bool TrivialRowGrouping>
SoundValueIterationHelper<ValueType, TrivialRowGrouping>::SoundValueIterationHelper(
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator)
    : viOperator(viOperator) {
    sizeOfLargestRowGroup = 1;
    if constexpr (!TrivialRowGrouping) {
        auto it = viOperator->getRowGroupIndices().cbegin();
        auto itEnd = viOperator->getRowGroupIndices().cend() - 1;
        while (it != itEnd) {
            auto const curr = *it;
            sizeOfLargestRowGroup = std::max(sizeOfLargestRowGroup, *(++it) - curr);
        }
    }
}

enum class SVIStage { Initial, y_less_1, b_eq_d };

template<typename ValueType, OptimizationDirection Dir, SVIStage Stage, bool TrivialRowGrouping>
class SVIBackend {
   public:
    static const SVIStage CurrentStage = Stage;
    using RowValueStorageType = std::vector<std::pair<ValueType, ValueType>>;

    SVIBackend(RowValueStorageType& rowValueStorage, std::optional<ValueType> const& a, std::optional<ValueType> const& b,
               std::optional<ValueType> const& d = {})
        : currRowValues(rowValueStorage) {
        if (a.has_value()) {
            aValue &= *a;
        }
        if (b.has_value()) {
            bValue &= *b;
        }
        if (d.has_value()) {
            dValue &= *d;
        }
    }

    void startNewIteration() {
        allYLessOne = true;
        curr_a.reset();
        curr_b.reset();
    }

    void firstRow(std::pair<ValueType, ValueType>&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        STORM_LOG_ASSERT(currRowValuesIndex == 0, "Expected currRowValuesIndex to be 0.");
        if constexpr (!TrivialRowGrouping) {
            bestValue.reset();
        }
        best = std::move(value);
    }

    void nextRow(std::pair<ValueType, ValueType>&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        STORM_LOG_ASSERT(!TrivialRowGrouping, "Expected non-trivial row grouping.");
        STORM_LOG_ASSERT(currRowValuesIndex < currRowValues.size(), "CurrRowValuesIndex out of range.");
        if (Stage == SVIStage::Initial && bValue.empty()) {
            if (value.second > best.second || (value.second == best.second && better(value.first, best.first))) {
                std::swap(value, best);
            }
            currRowValues[currRowValuesIndex++] = std::move(value);
        } else {
            STORM_LOG_ASSERT(!bValue.empty(), "BValue should not be empty.");
            auto const& b = Stage == SVIStage::b_eq_d ? *dValue : *bValue;
            if (bestValue.empty()) {
                bestValue = best.first + b * best.second;
            }
            if (ValueType currentValue = value.first + b * value.second; bestValue &= currentValue) {
                std::swap(value, best);
                if (Stage != SVIStage::b_eq_d && value.second < best.second) {
                    // We need to store the 'old' best values as they might be relevant for the decision value.
                    currRowValues[currRowValuesIndex++] = std::move(value);
                }
            } else if (best.second > value.second) {
                if (*bestValue == currentValue) {
                    // In this case we have the same value, but the current row is to be preferred as it has a smaller y value
                    std::swap(value, best);
                } else if (Stage != SVIStage::b_eq_d) {
                    // In this case we have a worse weighted value
                    // However, this could be relevant for the decision value
                    currRowValues[currRowValuesIndex++] = std::move(value);
                }
            }
        }
    }

    void applyUpdate(ValueType& xCurr, ValueType& yCurr, [[maybe_unused]] uint64_t rowGroup) {
        std::swap(xCurr, best.first);
        std::swap(yCurr, best.second);
        if constexpr (Stage != SVIStage::b_eq_d && !TrivialRowGrouping) {
            // Update decision value
            while (currRowValuesIndex) {
                if (auto const& rowVal = currRowValues[--currRowValuesIndex]; yCurr > rowVal.second) {
                    dValue &= (rowVal.first - xCurr) / (yCurr - rowVal.second);
                }
            }
        } else {
            STORM_LOG_ASSERT(currRowValuesIndex == 0, "Expected currRowValuesIndex to be 0.");
        }

        // keep track of bounds a,b
        if constexpr (Stage == SVIStage::Initial) {
            if (allYLessOne) {
                if (yCurr < storm::utility::one<ValueType>()) {
                    ValueType val = xCurr / (storm::utility::one<ValueType>() - yCurr);
                    curr_a &= val;
                    curr_b &= val;
                } else {
                    allYLessOne = false;
                }
            }
        } else {
            STORM_LOG_ASSERT(yCurr < storm::utility::one<ValueType>(), "Unexpected y value for this stage.");
            ValueType val = xCurr / (storm::utility::one<ValueType>() - yCurr);
            curr_a &= val;
            curr_b &= val;
        }
    }

    void endOfIteration() {
        nextStage = Stage;
        if (nextStage == SVIStage::Initial && allYLessOne) {
            nextStage = SVIStage::y_less_1;
        }
        if (nextStage == SVIStage::y_less_1 || nextStage == SVIStage::b_eq_d) {
            aValue &= std::move(*curr_a);
            if (nextStage == SVIStage::y_less_1) {
                curr_b &= dValue;
                bValue &= std::move(*curr_b);
                if (!dValue.empty() && *bValue == *dValue) {
                    nextStage = SVIStage::b_eq_d;
                }
            } else {
                // in the b_eq_d stage, we slightly repurpose _b and _d:
                // _b is now used to track an upper bound (which can pass _d)
                // _d is now used for the weighting when selecting the best row
                bValue &= std::move(*curr_b);
            }
        }
    }

    bool constexpr converged() const {
        return false;
    }

    bool constexpr abort() const {
        return false;
    }

    std::optional<ValueType> a() const {
        return aValue.getOptionalValue();
    }

    std::optional<ValueType> b() const {
        return bValue.getOptionalValue();
    }

    std::optional<ValueType> d() const {
        return dValue.getOptionalValue();
    }

    bool moveToNextStage() const {
        return nextStage != Stage;
    }

    template<SVIStage NewStage>
    auto createBackendForNextStage() const {
        std::optional<ValueType> d;
        if (NewStage == SVIStage::b_eq_d && !bValue.empty())
            d = *bValue;
        else if (NewStage != SVIStage::Initial && !dValue.empty())
            d = *dValue;
        return SVIBackend<ValueType, Dir, NewStage, TrivialRowGrouping>(currRowValues, a(), b(), d);
    }

    SVIStage const& getNextStage() const {
        return nextStage;
    }

   private:
    static bool better(ValueType const& lhs, ValueType const& rhs) {
        if constexpr (minimize(Dir)) {
            return lhs < rhs;
        } else {
            return lhs > rhs;
        }
    }

    using ExtremumDir = storm::utility::Extremum<Dir, ValueType>;
    using ExtremumInvDir = storm::utility::Extremum<invert(Dir), ValueType>;

    ExtremumDir aValue, dValue;
    ExtremumInvDir bValue;

    SVIStage nextStage{Stage};

    ExtremumDir curr_b;
    ExtremumInvDir curr_a;
    bool allYLessOne;

    std::pair<ValueType, ValueType> best;
    ExtremumDir bestValue;
    RowValueStorageType& currRowValues;
    uint64_t currRowValuesIndex{0};
};

template<typename ValueType, bool TrivialRowGrouping>
void SoundValueIterationHelper<ValueType, TrivialRowGrouping>::SVIData::trySetAverage(std::vector<ValueType>& out) const {
    if (a.has_value() && b.has_value()) {
        ValueType abAvg = (*a + *b) / storm::utility::convertNumber<ValueType, uint64_t>(2);
        storm::utility::vector::applyPointwise(xy.first, xy.second, out,
                                               [&abAvg](ValueType const& xVal, ValueType const& yVal) -> ValueType { return xVal + abAvg * yVal; });
    }
}

template<typename ValueType, bool TrivialRowGrouping>
void SoundValueIterationHelper<ValueType, TrivialRowGrouping>::SVIData::trySetLowerUpper(std::vector<ValueType>& lowerOut,
                                                                                         std::vector<ValueType>& upperOut) const {
    auto [min, max] = std::minmax(*a, *b);
    uint64_t const size = xy.first.size();
    for (uint64_t i = 0; i < size; ++i) {
        // We allow setting both vectors "in-place", e.g. we might have &lowerOut == &xy.first.
        // This requires to use temporary values.
        ValueType xi = xy.first[i];
        ValueType yi = xy.second[i];
        lowerOut[i] = xi + min * yi;
        upperOut[i] = xi + max * yi;
    }
}

template<typename ValueType, bool TrivialRowGrouping>
bool SoundValueIterationHelper<ValueType, TrivialRowGrouping>::SVIData::checkCustomTerminationCondition(
    storm::solver::TerminationCondition<ValueType> const& condition) const {
    if (a.has_value() && b.has_value()) {
        if (condition.requiresGuarantee(storm::solver::SolverGuarantee::GreaterOrEqual)) {
            auto max = std::max(*a, *b);
            return condition.terminateNow([&](uint64_t const& i) { return xy.first[i] + xy.second[i] * max; }, storm::solver::SolverGuarantee::GreaterOrEqual);
        } else if (condition.requiresGuarantee(storm::solver::SolverGuarantee::LessOrEqual)) {
            auto min = std::min(*a, *b);
            return condition.terminateNow([&](uint64_t const& i) { return xy.first[i] + xy.second[i] * min; }, storm::solver::SolverGuarantee::GreaterOrEqual);
        }
    }
    return false;
}

template<typename ValueType, bool TrivialRowGrouping>
bool SoundValueIterationHelper<ValueType, TrivialRowGrouping>::SVIData::checkConvergence(uint64_t& convergenceCheckState,
                                                                                         std::function<void()> const& getNextConvergenceCheckState,
                                                                                         bool relative, ValueType const& precision) const {
    if (!a.has_value() || !b.has_value())
        return false;
    if (*a == *b)
        return true;
    if (relative) {
        auto [min, max] = std::minmax(*a, *b);
        if (min >= storm::utility::zero<ValueType>()) {
            ValueType const val = (max - min) / precision - min;
            for (; convergenceCheckState < xy.first.size(); getNextConvergenceCheckState()) {
                if (!storm::utility::isZero(xy.second[convergenceCheckState]) && val > xy.first[convergenceCheckState] / xy.second[convergenceCheckState]) {
                    return false;
                }
            }
        } else if (max <= storm::utility::zero<ValueType>()) {
            ValueType const val = (min - max) / precision - max;
            for (; convergenceCheckState < xy.first.size(); getNextConvergenceCheckState()) {
                if (!storm::utility::isZero(xy.second[convergenceCheckState]) && val < xy.first[convergenceCheckState] / xy.second[convergenceCheckState]) {
                    return false;
                }
            }
        } else {
            for (; convergenceCheckState < xy.first.size(); getNextConvergenceCheckState()) {
                ValueType l = xy.first[convergenceCheckState] + min * xy.second[convergenceCheckState];
                ValueType u = xy.first[convergenceCheckState] + max * xy.second[convergenceCheckState];
                STORM_LOG_ASSERT(u >= l, "Upper bound less than lower bound.");
                if (l > storm::utility::zero<ValueType>()) {
                    if ((u - l) > l * precision) {
                        return false;
                    }
                } else if (u < storm::utility::zero<ValueType>()) {
                    if ((l - u) < u * precision) {
                        return false;
                    }
                } else {  //  l <= 0 <= u
                    if (l != u) {
                        return false;
                    }
                }
            }
        }
    } else {
        ValueType val = precision / storm::utility::abs<ValueType>(*b - *a);
        for (; convergenceCheckState < xy.first.size(); getNextConvergenceCheckState()) {
            if (xy.second[convergenceCheckState] > val) {
                return false;
            }
        }
    }
    return true;
}

template<typename ValueType, bool TrivialRowGrouping>
template<typename BackendType>
typename SoundValueIterationHelper<ValueType, TrivialRowGrouping>::SVIData SoundValueIterationHelper<ValueType, TrivialRowGrouping>::SVI(
    std::pair<std::vector<ValueType>, std::vector<ValueType>>& xy, std::pair<std::vector<ValueType> const*, ValueType> const& offsets, uint64_t& numIterations,
    bool relative, ValueType const& precision, BackendType&& backend, std::function<SolverStatus(SVIData const&)> const& iterationCallback,
    std::optional<storm::storage::BitVector> const& relevantValues, uint64_t convergenceCheckState) const {
    if constexpr (BackendType::CurrentStage == SVIStage::Initial) {
        xy.first.assign(xy.first.size(), storm::utility::zero<ValueType>());
        xy.second.assign(xy.first.size(), storm::utility::one<ValueType>());
        convergenceCheckState = relevantValues.has_value() ? relevantValues->getNextSetIndex(0ull) : 0ull;
    }
    std::function<void()> getNextConvergenceCheckState;
    if (relevantValues) {
        getNextConvergenceCheckState = [&convergenceCheckState, &relevantValues]() {
            convergenceCheckState = relevantValues->getNextSetIndex(++convergenceCheckState);
        };
    } else {
        getNextConvergenceCheckState = [&convergenceCheckState]() { ++convergenceCheckState; };
    }

    while (true) {
        ++numIterations;
        viOperator->applyInPlace(xy, offsets, backend);
        SVIData data{SolverStatus::InProgress, xy, backend.a(), backend.b()};
        if (data.checkConvergence(convergenceCheckState, getNextConvergenceCheckState, relative, precision)) {
            return SVIData{SolverStatus::Converged, xy, backend.a(), backend.b()};
        } else {
            if (iterationCallback) {
                SVIData data{SolverStatus::InProgress, xy, backend.a(), backend.b()};
                data.status = iterationCallback(data);
                if (data.status != SolverStatus::InProgress) {
                    return data;
                }
            }
            if (backend.moveToNextStage()) {
                switch (backend.getNextStage()) {
                    case SVIStage::y_less_1:
                        return SVI(xy, offsets, numIterations, relative, precision, backend.template createBackendForNextStage<SVIStage::y_less_1>(),
                                   iterationCallback, relevantValues, convergenceCheckState);
                    case SVIStage::b_eq_d:
                        return SVI(xy, offsets, numIterations, relative, precision, backend.template createBackendForNextStage<SVIStage::b_eq_d>(),
                                   iterationCallback, relevantValues, convergenceCheckState);
                    default:
                        STORM_LOG_ASSERT(false, "Unexpected next stage.");
                }
            }
        }
    }
}

template<typename ValueType, bool TrivialRowGrouping>
template<storm::OptimizationDirection Dir>
typename SoundValueIterationHelper<ValueType, TrivialRowGrouping>::SVIData SoundValueIterationHelper<ValueType, TrivialRowGrouping>::SVI(
    std::pair<std::vector<ValueType>, std::vector<ValueType>>& xy, std::pair<std::vector<ValueType> const*, ValueType> const& offsets, uint64_t& numIterations,
    bool relative, ValueType const& precision, std::optional<ValueType> const& a, std::optional<ValueType> const& b,
    std::function<SolverStatus(SVIData const&)> const& iterationCallback, std::optional<storm::storage::BitVector> const& relevantValues) const {
    typename SVIBackend<ValueType, Dir, SVIStage::Initial, TrivialRowGrouping>::RowValueStorageType rowValueStorage;
    rowValueStorage.resize(sizeOfLargestRowGroup - 1);
    return SVI(xy, offsets, numIterations, relative, precision, SVIBackend<ValueType, Dir, SVIStage::Initial, TrivialRowGrouping>(rowValueStorage, a, b),
               iterationCallback, relevantValues);
}

template<typename ValueType, bool TrivialRowGrouping>
typename SoundValueIterationHelper<ValueType, TrivialRowGrouping>::SVIData SoundValueIterationHelper<ValueType, TrivialRowGrouping>::SVI(
    std::pair<std::vector<ValueType>, std::vector<ValueType>>& xy, std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative,
    ValueType const& precision, std::optional<storm::OptimizationDirection> const& dir, std::optional<ValueType> const& lowerBound,
    std::optional<ValueType> const& upperBound, std::function<SolverStatus(SVIData const&)> const& iterationCallback,
    std::optional<storm::storage::BitVector> const& relevantValues) const {
    std::pair<std::vector<ValueType> const*, ValueType> offsetsPair{&offsets, storm::utility::zero<ValueType>()};
    if (!dir.has_value() || maximize(*dir)) {
        // When we maximize, a is the lower bound and b is the upper bound
        return SVI<storm::OptimizationDirection::Maximize>(xy, offsetsPair, numIterations, relative, precision, lowerBound, upperBound, iterationCallback,
                                                           relevantValues);
    } else {
        // When we minimize, b is the lower bound and a is the upper bound
        return SVI<storm::OptimizationDirection::Minimize>(xy, offsetsPair, numIterations, relative, precision, upperBound, lowerBound, iterationCallback,
                                                           relevantValues);
    }
}

template<typename ValueType, bool TrivialRowGrouping>
SolverStatus SoundValueIterationHelper<ValueType, TrivialRowGrouping>::SVI(
    std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative, ValueType const& precision,
    std::optional<storm::OptimizationDirection> const& dir, std::optional<ValueType> const& lowerBound, std::optional<ValueType> const& upperBound,
    std::function<SolverStatus(SVIData const&)> const& iterationCallback, std::optional<storm::storage::BitVector> const& relevantValues) const {
    // Create two vectors x and y using the given operand plus an auxiliary vector.
    std::pair<std::vector<ValueType>, std::vector<ValueType>> xy;
    auto& auxVector = viOperator->allocateAuxiliaryVector(operand.size());
    xy.first.swap(operand);
    xy.second.swap(auxVector);
    auto doublePrec = precision + precision;
    if constexpr (std::is_same_v<ValueType, double>) {
        doublePrec -= precision * 1e-6;  // be slightly more precise to avoid a good chunk of floating point issues
    }
    auto res = SVI(xy, offsets, numIterations, relative, doublePrec, dir, lowerBound, upperBound, iterationCallback, relevantValues);
    res.trySetAverage(xy.first);
    // Swap operand and aux vector back to original positions.
    xy.first.swap(operand);
    xy.second.swap(auxVector);
    viOperator->freeAuxiliaryVector();
    return res.status;
}

template<typename ValueType, bool TrivialRowGrouping>
SolverStatus SoundValueIterationHelper<ValueType, TrivialRowGrouping>::SVI(
    std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, bool relative, ValueType const& precision,
    std::optional<storm::OptimizationDirection> const& dir, std::optional<ValueType> const& lowerBound, std::optional<ValueType> const& upperBound,
    std::function<SolverStatus(SVIData const&)> const& iterationCallback, std::optional<storm::storage::BitVector> const& relevantValues) const {
    uint64_t numIterations = 0;
    return SVI(operand, offsets, numIterations, relative, precision, dir, lowerBound, upperBound, iterationCallback, relevantValues);
}

template class SoundValueIterationHelper<double, true>;
template class SoundValueIterationHelper<double, false>;
template class SoundValueIterationHelper<storm::RationalNumber, true>;
template class SoundValueIterationHelper<storm::RationalNumber, false>;

}  // namespace storm::solver::helper
