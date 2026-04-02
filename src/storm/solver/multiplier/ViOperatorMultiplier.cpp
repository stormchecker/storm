#include "ViOperatorMultiplier.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/helper/ValueIterationOperator.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::solver {

namespace detail {
enum class BackendOptimizationDirection { None, Minimize, Maximize };

/*!
 * This backend stores the best (maximal or minimal) value of the current row group.
 * It also allows to track the choices made by the scheduler.
 */
template<typename ValueType, BackendOptimizationDirection Dir = BackendOptimizationDirection::None, bool TrackChoices = false>
class MultiplierBackend {
   public:
    MultiplierBackend()
        requires(!TrackChoices)
        : choiceTracking(std::nullopt) {};

    MultiplierBackend(std::vector<uint64_t>& choices, std::vector<uint64_t> const& rowGroupIndices)
        requires TrackChoices
        : choiceTracking({choices, rowGroupIndices}) {
        // intentionally left empty.
    }

    void startNewIteration() {
        // intentionally left empty.
    }

    void firstRow(ValueType&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        best = std::move(value);
        if constexpr (TrackChoices) {
            choiceTracking.currentBestRow = row;
        }
    }

    void nextRow(ValueType&& value, [[maybe_unused]] uint64_t rowGroup, [[maybe_unused]] uint64_t row) {
        if constexpr (TrackChoices) {
            if (best &= value) {
                choiceTracking.currentBestRow = row;
            } else if (*best == value) {
                // Reaching this point means that there are multiple 'best' values
                // For rows that are equally good, we prefer to not change the currently selected choice.
                // This is necessary, e.g., for policy iteration correctness and helps keeping schedulers simple.
                if (row == choiceTracking.choices[rowGroup] + choiceTracking.rowGroupIndices[rowGroup]) {
                    choiceTracking.currentBestRow = row;
                }
            }
        } else if constexpr (HasDir) {
            best &= value;
        } else {
            STORM_LOG_ASSERT(false, "This backend does not support optimization direction.");
        }
    }

    void applyUpdate(ValueType& currValue, [[maybe_unused]] uint64_t rowGroup) {
        if constexpr (HasDir) {
            currValue = std::move(*best);
            if constexpr (TrackChoices) {
                choiceTracking.choices[rowGroup] = choiceTracking.currentBestRow - choiceTracking.rowGroupIndices[rowGroup];
            }
        } else {
            currValue = std::move(best);
        }
    }

    void endOfIteration() const {
        // intentionally left empty.
    }

    bool converged() const {
        return true;
    }

    bool constexpr abort() const {
        return false;
    }

   private:
    static constexpr bool HasDir = Dir != BackendOptimizationDirection::None;
    static constexpr storm::OptimizationDirection OptDir =
        Dir == BackendOptimizationDirection::Maximize ? storm::OptimizationDirection::Maximize : storm::OptimizationDirection::Minimize;
    static_assert(!TrackChoices || HasDir, "If TrackChoices is true, Dir must be set to either Minimize or Maximize.");

    std::conditional_t<HasDir, storm::utility::Extremum<OptDir, ValueType>, ValueType> best;

    struct SchedulerTrackingData {
        std::vector<uint64_t>& choices;                // Storage for the scheduler choices.
        std::vector<uint64_t> const& rowGroupIndices;  // Indices of the row groups.
        uint64_t currentBestRow{0};
    };
    std::conditional_t<TrackChoices, SchedulerTrackingData, std::nullopt_t> choiceTracking;  // Only used if TrackChoices is true.
};

/*!
 * This backend simply stores the row results in a vector.
 * @tparam ValueType
 */
template<typename ValueType>
class PlainMultiplicationBackend {
   public:
    PlainMultiplicationBackend(std::vector<ValueType>& rowResults) : rowResults(rowResults) {};

    void startNewIteration() {
        // intentionally left empty.
    }

    void firstRow(ValueType&& value, [[maybe_unused]] uint64_t rowGroup, uint64_t row) {
        rowResults[row] = std::move(value);
    }

    void nextRow(ValueType&& value, [[maybe_unused]] uint64_t rowGroup, uint64_t row) {
        rowResults[row] = std::move(value);
    }

    void applyUpdate([[maybe_unused]] ValueType& currValue, [[maybe_unused]] uint64_t rowGroup) {
        // intentionally left empty.
    }

    void endOfIteration() const {
        // intentionally left empty.
    }

    bool converged() const {
        return true;
    }

    bool constexpr abort() const {
        return false;
    }

   private:
    std::vector<ValueType>& rowResults;
};

}  // namespace detail

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
ViOperatorMultiplier<ValueType, TrivialRowGrouping, SolutionType>::ViOperatorMultiplier(storm::storage::SparseMatrix<ValueType> const& matrix)
    : Multiplier<ValueType, SolutionType>(matrix) {
    // Intentionally left empty.
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
typename ViOperatorMultiplier<ValueType, TrivialRowGrouping, SolutionType>::ViOpT&
ViOperatorMultiplier<ValueType, TrivialRowGrouping, SolutionType>::initialize() const {
    if (!viOperatorFwd) {
        return initialize(false);  // default to backward operator
    } else {
        return *viOperatorFwd;
    }
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
typename ViOperatorMultiplier<ValueType, TrivialRowGrouping, SolutionType>::ViOpT&
ViOperatorMultiplier<ValueType, TrivialRowGrouping, SolutionType>::initialize(bool backwards) const {
    auto& viOp = backwards ? viOperatorBwd : viOperatorFwd;
    if (!viOp) {
        viOp = std::make_unique<ViOpT>();
        if (backwards) {
            viOp->setMatrixBackwards(this->matrix);
        } else {
            viOp->setMatrixForwards(this->matrix);
        }
    }
    return *viOp;
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
void ViOperatorMultiplier<ValueType, TrivialRowGrouping, SolutionType>::multiply(Environment const& env, std::vector<SolutionType> const& x,
                                                                                 std::vector<ValueType> const* b, std::vector<SolutionType>& result) const {
    if (&result == &x) {
        auto& tmpResult = this->provideCachedVector(x.size());
        multiply(env, x, b, tmpResult);
        std::swap(result, tmpResult);
        return;
    }
    auto const& viOp = initialize();
    detail::PlainMultiplicationBackend<SolutionType> backend(result);
    // Below, we just add 'result' as a dummy argument to the apply method.
    // The backend already takes care of filling the result vector while processing the rows.
    if (b) {
        viOp.apply(x, result, *b, backend);
    } else {
        viOp.apply(x, result, storm::utility::zero<ValueType>(), backend);
    }
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
void ViOperatorMultiplier<ValueType, TrivialRowGrouping, SolutionType>::multiplyGaussSeidel(Environment const& /*env*/, std::vector<SolutionType>& x,
                                                                                            std::vector<ValueType> const* b, bool backwards) const {
    STORM_LOG_THROW(TrivialRowGrouping, storm::exceptions::NotSupportedException,
                    "This multiplier does not support multiplications without reduction when invoked with non-trivial row groups");
    detail::MultiplierBackend<SolutionType> backend;
    auto const& viOp = initialize(backwards);
    if (b) {
        viOp.applyInPlace(x, *b, backend);
    } else {
        viOp.applyInPlace(x, storm::utility::zero<ValueType>(), backend);
    }
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
void ViOperatorMultiplier<ValueType, TrivialRowGrouping, SolutionType>::multiplyAndReduce(Environment const& env, OptimizationDirection const& dir,
                                                                                          std::vector<uint64_t> const& rowGroupIndices,
                                                                                          std::vector<SolutionType> const& x, std::vector<ValueType> const* b,
                                                                                          std::vector<SolutionType>& result,
                                                                                          UncertaintyResolutionMode const& uncertaintyResolutionMode,
                                                                                          std::vector<uint64_t>* choices) const {
    if (&result == &x) {
        auto& tmpResult = this->provideCachedVector(x.size());
        multiplyAndReduce(env, dir, rowGroupIndices, x, b, tmpResult, uncertaintyResolutionMode, choices);
        std::swap(result, tmpResult);
        return;
    }
    STORM_LOG_THROW(&rowGroupIndices == &this->matrix.getRowGroupIndices(), storm::exceptions::NotSupportedException,
                    "The row group indices must be the same as the ones stored in the matrix of this multiplier");
    auto const& viOp = initialize();

    auto applyRobustDirection = [&]<storm::OptimizationDirection Dir, typename BT, typename OffsetType>(BT& backend, OffsetType const& offset) {
        bool robustUncertainty = false;
        if constexpr (storm::IsIntervalType<ValueType>) {
            robustUncertainty = isUncertaintyResolvedRobust(uncertaintyResolutionMode, dir);
        }

        if (robustUncertainty) {
            viOp.template applyRobust<invert(Dir)>(x, result, offset, backend);
        } else {
            viOp.template applyRobust<Dir>(x, result, offset, backend);
        }
    };

    auto applyBackend = [&]<typename OffsetType>(OffsetType const& offset) {
        if (storm::solver::minimize(dir)) {
            if (choices) {
                detail::MultiplierBackend<SolutionType, detail::BackendOptimizationDirection::Minimize, true> backend(*choices,
                                                                                                                      this->matrix.getRowGroupIndices());
                applyRobustDirection.template operator()<OptimizationDirection::Minimize>(backend, offset);
            } else {
                detail::MultiplierBackend<SolutionType, detail::BackendOptimizationDirection::Minimize, false> backend;
                applyRobustDirection.template operator()<OptimizationDirection::Minimize>(backend, offset);
            }
        } else {
            if (choices) {
                detail::MultiplierBackend<SolutionType, detail::BackendOptimizationDirection::Maximize, true> backend(*choices,
                                                                                                                      this->matrix.getRowGroupIndices());
                applyRobustDirection.template operator()<OptimizationDirection::Maximize>(backend, offset);
            } else {
                detail::MultiplierBackend<SolutionType, detail::BackendOptimizationDirection::Maximize, false> backend;
                applyRobustDirection.template operator()<OptimizationDirection::Maximize>(backend, offset);
            }
        }
    };

    if (b) {
        applyBackend(*b);
    } else {
        applyBackend(storm::utility::zero<ValueType>());
    }
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
void ViOperatorMultiplier<ValueType, TrivialRowGrouping, SolutionType>::multiplyAndReduceGaussSeidel(
    Environment const& env, OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices, std::vector<SolutionType>& x,
    std::vector<ValueType> const* b, std::vector<uint_fast64_t>* choices, bool backwards) const {
    STORM_LOG_THROW(&rowGroupIndices == &this->matrix.getRowGroupIndices(), storm::exceptions::NotSupportedException,
                    "The row group indices must be the same as the ones stored in the matrix of this multiplier");
    auto const& viOp = initialize(backwards);
    auto apply = [&]<typename BT>(BT& backend) {
        if (b) {
            viOp.applyInPlace(x, *b, backend);
        } else {
            viOp.applyInPlace(x, storm::utility::zero<ValueType>(), backend);
        }
    };
    if (storm::solver::minimize(dir)) {
        if (choices) {
            detail::MultiplierBackend<SolutionType, detail::BackendOptimizationDirection::Minimize, true> backend(*choices, this->matrix.getRowGroupIndices());
            apply(backend);
        } else {
            detail::MultiplierBackend<SolutionType, detail::BackendOptimizationDirection::Minimize, false> backend;
            apply(backend);
        }
    } else {
        if (choices) {
            detail::MultiplierBackend<SolutionType, detail::BackendOptimizationDirection::Maximize, true> backend(*choices, this->matrix.getRowGroupIndices());
            apply(backend);
        } else {
            detail::MultiplierBackend<SolutionType, detail::BackendOptimizationDirection::Maximize, false> backend;
            apply(backend);
        }
    }
}

template<typename ValueType, bool TrivialRowGrouping, typename SolutionType>
void ViOperatorMultiplier<ValueType, TrivialRowGrouping, SolutionType>::clearCache() const {
    viOperatorBwd.reset();
    viOperatorFwd.reset();
    Multiplier<ValueType, SolutionType>::clearCache();
};

template class ViOperatorMultiplier<double, true>;
template class ViOperatorMultiplier<double, false>;

template class ViOperatorMultiplier<storm::RationalNumber, true>;
template class ViOperatorMultiplier<storm::RationalNumber, false>;

template class ViOperatorMultiplier<storm::Interval, true, double>;
template class ViOperatorMultiplier<storm::Interval, false, double>;

template class ViOperatorMultiplier<storm::RationalInterval, true, storm::RationalNumber>;
template class ViOperatorMultiplier<storm::RationalInterval, false, storm::RationalNumber>;

}  // namespace storm::solver
