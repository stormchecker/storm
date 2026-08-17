#include "GuessingValueIterationHelper.h"

#include "storm/storage/SparseMatrix.h"

#include "storm/utility/Extremum.h"
#include "storm/utility/ProgressMeasurement.h"
#include "storm/utility/vector.h"

#include "storm/exceptions/NotSupportedException.h"

#include "storm/utility/macros.h"

namespace storm::solver::helper {

namespace gviinternal {

template<typename ValueType>
IterationHelper<ValueType>::IterationHelper(const storage::SparseMatrix<ValueType>& matrix_) : matrix(matrix_) {}

template<typename ValueType>
void IterationHelper<ValueType>::swipeWeights() {
    const auto& rowGroupIndices = matrix.getRowGroupIndices();
    IndexType i = weights.size();
    while (i > 0) {
        --i;

        auto den = storm::utility::convertNumber<ValueType>(rowGroupIndices[i + 1] - rowGroupIndices[i]);
        auto weightOnGroup = weights[i] / den;
        weights[i] = 0;

        for (auto row = rowGroupIndices[i]; row < rowGroupIndices[i + 1]; ++row) {
            for (auto const& entry : matrix.getRow(row)) {
                weights[entry.getColumn()] += weightOnGroup * entry.getValue();
            }
        }
    }
}

template<typename ValueType>
IndexType IterationHelper<ValueType>::selectRowGroupToGuess(const std::vector<ValueType>& lowerX, const std::vector<ValueType>& upperX) {
    weights = upperX;
    // upper - lower
    storm::utility::vector::addScaledVector<ValueType, ValueType>(weights, lowerX, -1);

    std::vector<ValueType> additiveWeights(weights.size(), 0);
    for (int i = 0; i < 50; i++) {
        swipeWeights();
        storm::utility::vector::addScaledVector(additiveWeights, weights, 1);
    }

    return static_cast<IndexType>(std::max_element(additiveWeights.begin(), additiveWeights.end()) - additiveWeights.begin());
}

template<typename ValueType>
ValueType IterationHelper<ValueType>::getMaxLength(std::vector<ValueType>& lower, std::vector<ValueType>& upper) const {
    auto itL = lower.begin();
    const auto itLEnd = lower.end();
    auto itU = upper.begin();

    ValueType result = *itU - *itL;
    for (++itL, ++itU; itL != itLEnd; ++itL, ++itU) {
        ValueType length = *itU - *itL;
        result = std::max(result, length);
    }
    return result;
}

template<typename ValueType>
ValueType IterationHelper<ValueType>::getSumLength(std::vector<ValueType>& lower, std::vector<ValueType>& upper) const {
    ValueType sum = 0;
    auto itL = lower.begin();
    const auto itLEnd = lower.end();
    auto itU = upper.begin();

    for (; itL != itLEnd; ++itL, ++itU) {
        sum += *itU - *itL;
    }
    return sum;
}

}  // namespace gviinternal

template<typename ValueType, storm::OptimizationDirection Dir>
class GVIBackend {
   public:
    ValueType xGuessed, yGuessed;

    GVIBackend() {}
    GVIBackend(uint64_t guessedRowGroup) : guessedRowGroup(guessedRowGroup) {}

    void startNewIteration() {}

    void firstRow(std::pair<ValueType, ValueType>&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        xBest = std::move(value.first);
        yBest = std::move(value.second);
    }

    void nextRow(std::pair<ValueType, ValueType>&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        xBest &= std::move(value.first);
        yBest &= std::move(value.second);
    }

    void applyUpdate(ValueType& xCurr, ValueType& yCurr, uint64_t rowGroup) {
        if (guessedRowGroup && rowGroup == *guessedRowGroup) {
            xGuessed = *xBest;
            yGuessed = *yBest;
            return;
        }
        xCurr = std::max(xCurr, *xBest);
        yCurr = std::min(yCurr, *yBest);
    }

    void endOfIteration() const {}

    bool constexpr converged() const {
        return false;
    }

    bool constexpr abort() const {
        return false;
    }

   private:
    storm::utility::Extremum<Dir, ValueType> xBest, yBest;
    std::optional<uint64_t> guessedRowGroup;
};

template<typename ValueType, bool TrivialRowGrouping>
gviinternal::IndexType GuessingValueIterationHelper<ValueType, TrivialRowGrouping>::selectRowGroupToGuess(std::vector<ValueType>& lowerX,
                                                                                                          std::vector<ValueType>& upperX) {
    return iterationHelper.selectRowGroupToGuess(lowerX, upperX);
}

template<typename ValueType, bool TrivialRowGrouping>
template<OptimizationDirection Dir>
void GuessingValueIterationHelper<ValueType, TrivialRowGrouping>::applyInPlace(std::vector<ValueType>& lowerX, std::vector<ValueType>& upperX,
                                                                               const std::vector<ValueType>& b, GVIBackend<ValueType, Dir>& backend) {
    std::pair<std::vector<ValueType>, std::vector<ValueType>> xy;
    xy.first.swap(lowerX);
    xy.second.swap(upperX);
    viOperator->applyInPlace(xy, b, backend);
    lowerX.swap(xy.first);
    upperX.swap(xy.second);
}

template<typename ValueType, bool TrivialRowGrouping>
template<OptimizationDirection Dir>
std::pair<gviinternal::VerifyResult, SolverStatus> GuessingValueIterationHelper<ValueType, TrivialRowGrouping>::tryVerify(
    std::vector<ValueType>& lowerX, std::vector<ValueType>& upperX, const std::vector<ValueType>& b, uint64_t& numIterations,
    gviinternal::IndexType rowGroupToGuess, ValueType guessValue, ValueType precision,
    std::function<SolverStatus(GVIData<ValueType> const&)> const& iterationCallback) {
    guessLower = lowerX;
    guessUpper = upperX;
    guessLower[rowGroupToGuess] = guessUpper[rowGroupToGuess] = guessValue;
    ValueType sumLengthBefore = 0;
    GVIBackend<ValueType, Dir> guessingBackend(rowGroupToGuess);
    GVIBackend<ValueType, Dir> iiBackend;

    auto status = SolverStatus::InProgress;
    while (status == SolverStatus::InProgress) {
        ++numIterations;
        applyInPlace<Dir>(lowerX, upperX, b, iiBackend);
        applyInPlace<Dir>(guessLower, guessUpper, b, guessingBackend);
        auto guessedNewLower = guessingBackend.xGuessed;
        auto guessedNewUpper = guessingBackend.yGuessed;
        if (guessValue <= guessedNewLower) {
            lowerX = guessLower;
            lowerX[rowGroupToGuess] = guessedNewLower;
            return {gviinternal::VerifyResult::Verified, status};
        }
        if (guessedNewUpper <= guessValue) {
            upperX = guessUpper;
            upperX[rowGroupToGuess] = guessedNewUpper;
            return {gviinternal::VerifyResult::Verified, status};
        }

        auto sumLength = iterationHelper.getSumLength(guessLower, guessUpper);
        if (sumLength == sumLengthBefore) {
            // nothing changed. abort the verification
            return {gviinternal::VerifyResult::Unverified, status};
        }
        sumLengthBefore = sumLength;

        if (iterationHelper.getMaxLength(lowerX, upperX) < 2 * precision) {
            return {gviinternal::VerifyResult::Converged, status};
        }

        if (iterationCallback) {
            status = iterationCallback({lowerX, upperX, status});
        }
    }
    return {gviinternal::VerifyResult::Unverified, status};
}

template<typename ValueType, bool TrivialRowGrouping>
SolverStatus GuessingValueIterationHelper<ValueType, TrivialRowGrouping>::solveEquations(
    std::vector<ValueType>& lowerX, std::vector<ValueType>& upperX, const std::vector<ValueType>& b, uint64_t& numIterations, ValueType precision,
    std::optional<storm::solver::OptimizationDirection> dir, std::function<SolverStatus(GVIData<ValueType> const&)> const& iterationCallback) {
    if (!dir.has_value() || minimize(dir.value()))
        return solveEquations<OptimizationDirection::Minimize>(lowerX, upperX, b, numIterations, precision, iterationCallback);
    else
        return solveEquations<OptimizationDirection::Maximize>(lowerX, upperX, b, numIterations, precision, iterationCallback);
}

template<typename ValueType, bool TrivialRowGrouping>
template<OptimizationDirection Dir>
SolverStatus GuessingValueIterationHelper<ValueType, TrivialRowGrouping>::solveEquations(
    std::vector<ValueType>& lowerX, std::vector<ValueType>& upperX, const std::vector<ValueType>& b, uint64_t& numIterations, ValueType precision,
    std::function<SolverStatus(GVIData<ValueType> const&)> const& iterationCallback) {
    // do n iterations first
    auto status = doIterations<Dir>(lowerX, upperX, b, numIterations, lowerX.size(), precision, iterationCallback);
    if (status != SolverStatus::InProgress)
        return status;
    while (status == SolverStatus::InProgress) {
        auto rowGroupToGuess = selectRowGroupToGuess(lowerX, upperX);
        bool didVerify = false;
        // try verification using different fractions of the interval
        for (int den = 2; den < 30 && !didVerify; ++den) {
            for (int num = 1; num < den; num++) {
                if (std::gcd(num, den) != 1)
                    continue;
                auto guessValue = (lowerX[rowGroupToGuess] * num + upperX[rowGroupToGuess] * (den - num)) / den;
                auto [verifyResult, verifyStatus] = tryVerify<Dir>(lowerX, upperX, b, numIterations, rowGroupToGuess, guessValue, precision, iterationCallback);
                status = verifyStatus;
                if (verifyResult == gviinternal::VerifyResult::Verified || verifyResult == gviinternal::VerifyResult::Converged) {
                    didVerify = true;
                    break;
                }
            }
        }
        if (didVerify) {
            if (iterationHelper.getMaxLength(lowerX, upperX) < 2 * precision)
                return SolverStatus::Converged;
        } else {
            break;
        }
    }

    // verification has failed. iterate until convergence
    return doIterations<Dir>(lowerX, upperX, b, numIterations, {}, precision, iterationCallback);
}

template<typename ValueType, bool TrivialRowGrouping>
GuessingValueIterationHelper<ValueType, TrivialRowGrouping>::GuessingValueIterationHelper(
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator, const storage::SparseMatrix<ValueType>& matrix)
    : viOperator(viOperator), iterationHelper(matrix) {
    // Intentionally left empty.
}

template<typename ValueType, bool TrivialRowGrouping>
template<OptimizationDirection Dir>
SolverStatus GuessingValueIterationHelper<ValueType, TrivialRowGrouping>::doIterations(
    std::vector<ValueType>& lowerX, std::vector<ValueType>& upperX, const std::vector<ValueType>& b, uint64_t& numIterations,
    std::optional<uint64_t> maxIterations, const ValueType& precision, std::function<SolverStatus(GVIData<ValueType> const&)> const& iterationCallback) {
    ValueType sumLengthBefore = 0;
    uint64_t localIterations = 0;
    GVIBackend<ValueType, Dir> iiBackend;
    auto status = SolverStatus::InProgress;
    while (status == SolverStatus::InProgress && (!maxIterations.has_value() || localIterations < maxIterations)) {
        applyInPlace<Dir>(lowerX, upperX, b, iiBackend);
        ++localIterations;
        ++numIterations;

        auto sumLength = iterationHelper.getSumLength(lowerX, upperX);
        if (sumLengthBefore == sumLength) {
            // nothing changed. abort the iterations.
            return SolverStatus::Aborted;
        }
        sumLengthBefore = sumLength;

        if (iterationHelper.getMaxLength(lowerX, upperX) < 2 * precision) {
            return SolverStatus::Converged;
        }
        if (iterationCallback) {
            status = iterationCallback(GVIData<ValueType>{lowerX, upperX, status});
        }
    }

    if (iterationHelper.getMaxLength(lowerX, upperX) < 2 * precision)
        return SolverStatus::Converged;
    return status;
}

template class GuessingValueIterationHelper<double, true>;
template class GuessingValueIterationHelper<double, false>;
template class GuessingValueIterationHelper<storm::RationalNumber, true>;
template class GuessingValueIterationHelper<storm::RationalNumber, false>;
}  // namespace storm::solver::helper