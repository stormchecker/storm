#include "storm/solver/multiplier/Multiplier.h"
#include <type_traits>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/solver/MultiplierEnvironment.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/solver/SolverSelectionOptions.h"
#include "storm/solver/multiplier/NativeMultiplier.h"
#include "storm/solver/multiplier/ViOperatorMultiplier.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/ProgressMeasurement.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

template<typename ValueType, typename SolutionType>
Multiplier<ValueType, SolutionType>::Multiplier(storm::storage::SparseMatrix<ValueType> const& matrix) : matrix(matrix) {
    // Intentionally left empty.
}

template<typename ValueType, typename SolutionType>
void Multiplier<ValueType, SolutionType>::clearCache() const {
    cachedVector.reset();
}

template<typename ValueType, typename SolutionType>
void Multiplier<ValueType, SolutionType>::multiplyAndReduce(Environment const& env, OptimizationDirection const& dir, std::vector<SolutionType> const& x,
                                                            std::vector<ValueType> const* b, std::vector<SolutionType>& result,
                                                            UncertaintyResolutionMode const& uncertaintyResolutionMode,
                                                            std::vector<uint_fast64_t>* choices) const {
    multiplyAndReduce(env, dir, this->matrix.getRowGroupIndices(), x, b, result, uncertaintyResolutionMode, choices);
}

template<typename ValueType, typename SolutionType>
void Multiplier<ValueType, SolutionType>::multiplyAndReduceGaussSeidel(Environment const& env, OptimizationDirection const& dir, std::vector<SolutionType>& x,
                                                                       std::vector<ValueType> const* b, std::vector<uint_fast64_t>* choices,
                                                                       bool backwards) const {
    multiplyAndReduceGaussSeidel(env, dir, this->matrix.getRowGroupIndices(), x, b, choices, backwards);
}

template<typename ValueType, typename SolutionType>
void Multiplier<ValueType, SolutionType>::repeatedMultiply(Environment const& env, std::vector<SolutionType>& x, std::vector<ValueType> const* b,
                                                           uint64_t n) const {
    storm::utility::ProgressMeasurement progress("multiplications");
    progress.setMaxCount(n);
    progress.startNewMeasurement(0);
    for (uint64_t i = 0; i < n; ++i) {
        progress.updateProgress(i);
        multiply(env, x, b, x);
        if (storm::utility::resources::isTerminate()) {
            STORM_LOG_WARN("Aborting after " << i << " of " << n << " multiplications.");
            break;
        }
    }
}

template<typename ValueType, typename SolutionType>
void Multiplier<ValueType, SolutionType>::repeatedMultiplyAndReduce(Environment const& env, OptimizationDirection const& dir, std::vector<SolutionType>& x,
                                                                    std::vector<ValueType> const* b, uint64_t n,
                                                                    UncertaintyResolutionMode const& uncertaintyResolutionMode) const {
    storm::utility::ProgressMeasurement progress("multiplications");
    progress.setMaxCount(n);
    progress.startNewMeasurement(0);
    for (uint64_t i = 0; i < n; ++i) {
        progress.updateProgress(i);
        multiplyAndReduce(env, dir, x, b, x, uncertaintyResolutionMode);
        if (storm::utility::resources::isTerminate()) {
            STORM_LOG_WARN("Aborting after " << i << " of " << n << " multiplications");
            break;
        }
    }
}

template<typename ValueType, typename SolutionType>
void Multiplier<ValueType, SolutionType>::repeatedMultiplyAndReduceWithFactor(Environment const& env, OptimizationDirection const& dir,
                                                                              std::vector<SolutionType>& x, std::vector<ValueType> const* b, uint64_t n,
                                                                              SolutionType factor,
                                                                              UncertaintyResolutionMode const& uncertaintyResolutionMode) const {
    storm::utility::ProgressMeasurement progress("multiplications");
    progress.setMaxCount(n);
    progress.startNewMeasurement(0);
    for (uint64_t i = 0; i < n; ++i) {
        progress.updateProgress(i);
        std::transform(x.begin(), x.end(), x.begin(), [factor](SolutionType& c) { return c * factor; });
        multiplyAndReduce(env, dir, x, b, x, uncertaintyResolutionMode);
        if (storm::utility::resources::isTerminate()) {
            STORM_LOG_WARN("Aborting after " << i << " of " << n << " multiplications");
            break;
        }
    }
}

template<typename ValueType, typename SolutionType>
void Multiplier<ValueType, SolutionType>::repeatedMultiplyWithFactor(Environment const& env, std::vector<SolutionType>& x, std::vector<ValueType> const* b,
                                                                     uint64_t n, SolutionType factor) const {
    storm::utility::ProgressMeasurement progress("multiplications");
    progress.setMaxCount(n);
    progress.startNewMeasurement(0);
    for (uint64_t i = 0; i < n; ++i) {
        progress.updateProgress(i);
        std::transform(x.begin(), x.end(), x.begin(), [factor](SolutionType& c) { return c * factor; });
        multiply(env, x, b, x);
        if (storm::utility::resources::isTerminate()) {
            STORM_LOG_WARN("Aborting after " << i << " of " << n << " multiplications");
            break;
        }
    }
}

template<typename ValueType, typename SolutionType>
std::vector<SolutionType>& Multiplier<ValueType, SolutionType>::provideCachedVector(uint64_t size) const {
    if (this->cachedVector) {
        this->cachedVector->resize(size);
    } else {
        this->cachedVector = std::make_unique<std::vector<SolutionType>>(size);
    }
    return *this->cachedVector;
}

template<typename ValueType, typename SolutionType>
std::unique_ptr<Multiplier<ValueType, SolutionType>> MultiplierFactory<ValueType, SolutionType>::create(Environment const& env,
                                                                                                        storm::storage::SparseMatrix<ValueType> const& matrix) {
    auto type = env.solver().multiplier().getType();

    // Adjust the type if the ValueType is not supported
    if (type == MultiplierType::ViOperator &&
        (std::is_same_v<ValueType, storm::RationalFunction> || (storm::IsIntervalType<ValueType> && storm::IsIntervalType<SolutionType>))) {
        STORM_LOG_INFO("Switching multiplier type from 'vioperator' to 'native' because the given ValueType is not supported by the VI Operator multiplier.");
        type = MultiplierType::Native;
    }

    switch (type) {
        case MultiplierType::ViOperator:
            if constexpr (std::is_same_v<ValueType, storm::RationalFunction> || (storm::IsIntervalType<ValueType> && storm::IsIntervalType<SolutionType>)) {
                throw storm::exceptions::NotImplementedException() << "VI Operator multiplier not supported with given value type.";
            }
            if (matrix.hasTrivialRowGrouping()) {
                return std::make_unique<ViOperatorMultiplier<ValueType, true, SolutionType>>(matrix);
            } else {
                return std::make_unique<ViOperatorMultiplier<ValueType, false, SolutionType>>(matrix);
            }
        case MultiplierType::Native:
            if constexpr (std::is_same_v<ValueType, SolutionType>) {
                return std::make_unique<NativeMultiplier<ValueType>>(matrix);
            } else {
                STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Native multiplier not implemented for unequal ValueType and SolutionType.");
            }
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Unknown MultiplierType");
}

template class Multiplier<double>;
template class MultiplierFactory<double>;
template class Multiplier<storm::RationalNumber>;
template class MultiplierFactory<storm::RationalNumber>;
template class Multiplier<storm::RationalFunction>;
template class MultiplierFactory<storm::RationalFunction>;
template class Multiplier<storm::Interval>;
template class MultiplierFactory<storm::Interval>;
template class Multiplier<storm::Interval, double>;
template class MultiplierFactory<storm::Interval, double>;
template class Multiplier<storm::RationalInterval, storm::RationalNumber>;
template class MultiplierFactory<storm::RationalInterval, storm::RationalNumber>;

}  // namespace solver
}  // namespace storm
