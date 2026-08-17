#include "storm/solver/helper/IntervalterationHelper.h"

#include <type_traits>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/solver/helper/ValueIterationOperator.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/constants.h"
#include "storm/utility/vector.h"

namespace storm::solver::helper {

template<typename ValueType, bool TrivialRowGrouping>
IntervalIterationHelper<ValueType, TrivialRowGrouping>::IntervalIterationHelper(
    std::shared_ptr<ValueIterationOperator<ValueType, TrivialRowGrouping>> viOperator)
    : viOperator(viOperator) {
    // Intentionally left empty.
}

template<typename ValueType, OptimizationDirection Dir>
class IIBackend {
   public:
    void startNewIteration() {}

    void firstRow(std::pair<ValueType, ValueType>&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        xBest = std::move(value.first);
        yBest = std::move(value.second);
    }

    void nextRow(std::pair<ValueType, ValueType>&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        xBest &= std::move(value.first);
        yBest &= std::move(value.second);
    }

    void applyUpdate(ValueType& xCurr, ValueType& yCurr, [[maybe_unused]] uint64_t rowGroup) {
        xCurr = std::max(xCurr, *xBest);
        yCurr = std::min(yCurr, *yBest);
    }

    void endOfIteration() const {
        // intentionally left empty.
    }

    bool constexpr converged() const {
        return false;
    }

    bool constexpr abort() const {
        return false;
    }

   private:
    storm::utility::Extremum<Dir, ValueType> xBest, yBest;
};

template<typename ValueType>
bool checkConvergence(std::pair<std::vector<ValueType>, std::vector<ValueType>> const& xy, uint64_t& convergenceCheckState,
                      std::function<void()> const& getNextConvergenceCheckState, bool relative, ValueType const& precision) {
    if (relative) {
        for (; convergenceCheckState < xy.first.size(); getNextConvergenceCheckState()) {
            ValueType const& l = xy.first[convergenceCheckState];
            ValueType const& u = xy.second[convergenceCheckState];
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
    } else {
        for (; convergenceCheckState < xy.first.size(); getNextConvergenceCheckState()) {
            if (xy.second[convergenceCheckState] - xy.first[convergenceCheckState] > precision) {
                return false;
            }
        }
    }
    return true;
}

template<typename ValueType, bool TrivialRowGrouping>
template<OptimizationDirection Dir>
SolverStatus IntervalIterationHelper<ValueType, TrivialRowGrouping>::II(std::pair<std::vector<ValueType>, std::vector<ValueType>>& xy,
                                                                        std::vector<ValueType> const& offsets, uint64_t& numIterations, bool relative,
                                                                        ValueType const& precision,
                                                                        std::function<SolverStatus(IIData<ValueType> const&)> const& iterationCallback,
                                                                        std::optional<storm::storage::BitVector> const& relevantValues) const {
    SolverStatus status{SolverStatus::InProgress};
    IIBackend<ValueType, Dir> backend;
    uint64_t convergenceCheckState = 0;
    std::function<void()> getNextConvergenceCheckState;
    if (relevantValues) {
        convergenceCheckState = relevantValues->getNextSetIndex(0);
        getNextConvergenceCheckState = [&convergenceCheckState, &relevantValues]() {
            convergenceCheckState = relevantValues->getNextSetIndex(++convergenceCheckState);
        };
    } else {
        getNextConvergenceCheckState = [&convergenceCheckState]() { ++convergenceCheckState; };
    }
    while (status == SolverStatus::InProgress) {
        ++numIterations;
        viOperator->applyInPlace(xy, offsets, backend);
        if (checkConvergence(xy, convergenceCheckState, getNextConvergenceCheckState, relative, precision)) {
            status = SolverStatus::Converged;
        } else if (iterationCallback) {
            status = iterationCallback(IIData<ValueType>({xy.first, xy.second, status}));
        }
    }
    return status;
}

template<typename ValueType, bool TrivialRowGrouping>
SolverStatus IntervalIterationHelper<ValueType, TrivialRowGrouping>::II(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets,
                                                                        uint64_t& numIterations, bool relative, ValueType const& precision,
                                                                        std::function<void(std::vector<ValueType>&)> const& prepareLowerBounds,
                                                                        std::function<void(std::vector<ValueType>&)> const& prepareUpperBounds,
                                                                        std::optional<storm::OptimizationDirection> const& dir,
                                                                        std::function<SolverStatus(IIData<ValueType> const&)> const& iterationCallback,
                                                                        std::optional<storm::storage::BitVector> const& relevantValues) const {
    // Create two vectors x and y using the given operand plus an auxiliary vector.
    std::pair<std::vector<ValueType>, std::vector<ValueType>> xy;
    auto& auxVector = viOperator->allocateAuxiliaryVector(operand.size());
    xy.first.swap(operand);
    xy.second.swap(auxVector);
    prepareLowerBounds(xy.first);
    prepareUpperBounds(xy.second);
    auto doublePrec = precision + precision;
    if constexpr (std::is_same_v<ValueType, double>) {
        doublePrec -= precision * 1e-6;  // be slightly more precise to avoid a good chunk of floating point issues
    }
    SolverStatus status;
    if (!dir.has_value() || maximize(*dir)) {
        status = II<OptimizationDirection::Maximize>(xy, offsets, numIterations, relative, precision, iterationCallback, relevantValues);
    } else {
        status = II<OptimizationDirection::Minimize>(xy, offsets, numIterations, relative, precision, iterationCallback, relevantValues);
    }
    auto two = storm::utility::convertNumber<ValueType>(2.0);
    // get the average of lower- and upper result
    storm::utility::vector::applyPointwise<ValueType, ValueType, ValueType>(
        xy.first, xy.second, xy.first, [&two](ValueType const& a, ValueType const& b) -> ValueType { return (a + b) / two; });
    // Swap operand and aux vector back to original positions.
    xy.first.swap(operand);
    xy.second.swap(auxVector);
    viOperator->freeAuxiliaryVector();
    return status;
}

template<typename ValueType, bool TrivialRowGrouping>
SolverStatus IntervalIterationHelper<ValueType, TrivialRowGrouping>::II(std::vector<ValueType>& operand, std::vector<ValueType> const& offsets, bool relative,
                                                                        ValueType const& precision,
                                                                        std::function<void(std::vector<ValueType>&)> const& prepareLowerBounds,
                                                                        std::function<void(std::vector<ValueType>&)> const& prepareUpperBounds,
                                                                        std::optional<storm::OptimizationDirection> const& dir,
                                                                        std::function<SolverStatus(IIData<ValueType> const&)> const& iterationCallback,
                                                                        std::optional<storm::storage::BitVector> const& relevantValues) const {
    uint64_t numIterations = 0;
    return II(operand, offsets, numIterations, relative, precision, prepareLowerBounds, prepareUpperBounds, dir, iterationCallback, relevantValues);
}

template class IntervalIterationHelper<double, true>;
template class IntervalIterationHelper<double, false>;
template class IntervalIterationHelper<storm::RationalNumber, true>;
template class IntervalIterationHelper<storm::RationalNumber, false>;

}  // namespace storm::solver::helper