#include "NativeMultiplier.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/solver/MultiplierEnvironment.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/solver/UncertaintyResolutionMode.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

template<typename ValueType>
NativeMultiplier<ValueType>::NativeMultiplier(storm::storage::SparseMatrix<ValueType> const& matrix) : Multiplier<ValueType>(matrix) {
    // Intentionally left empty.
}

template<typename ValueType>
void NativeMultiplier<ValueType>::multiply(Environment const& env, std::vector<ValueType> const& x, std::vector<ValueType> const* b,
                                           std::vector<ValueType>& result) const {
    std::vector<ValueType>* target = &result;
    if (&x == &result) {
        target = &this->provideCachedVector(x.size());
    }
    multAdd(x, b, *target);
    if (&x == &result) {
        std::swap(result, *target);
    }
}

template<typename ValueType>
void NativeMultiplier<ValueType>::multiplyGaussSeidel(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType> const* b,
                                                      bool backwards) const {
    if (backwards) {
        this->matrix.multiplyWithVectorBackward(x, x, b);
    } else {
        this->matrix.multiplyWithVectorForward(x, x, b);
    }
}

template<typename ValueType>
void NativeMultiplier<ValueType>::multiplyAndReduce(Environment const& env, OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices,
                                                    std::vector<ValueType> const& x, std::vector<ValueType> const* b, std::vector<ValueType>& result,
                                                    UncertaintyResolutionMode const& uncertaintyResolutionMode, std::vector<uint_fast64_t>* choices) const {
    STORM_LOG_THROW(uncertaintyResolutionMode == UncertaintyResolutionMode::Unset, storm::exceptions::NotSupportedException,
                    "Uncertainty resolution modes other than 'Unset' are not supported by the native multiplier.");

    std::vector<ValueType>* target = &result;
    if (&x == &result) {
        target = &this->provideCachedVector(x.size());
    }
    multAddReduce(dir, rowGroupIndices, x, b, *target, choices);
    if (&x == &result) {
        std::swap(result, *target);
    }
}

template<typename ValueType>
void NativeMultiplier<ValueType>::multiplyAndReduceGaussSeidel(Environment const& env, OptimizationDirection const& dir,
                                                               std::vector<uint64_t> const& rowGroupIndices, std::vector<ValueType>& x,
                                                               std::vector<ValueType> const* b, std::vector<uint_fast64_t>* choices, bool backwards) const {
    if (backwards) {
        this->matrix.multiplyAndReduceBackward(dir, rowGroupIndices, x, b, x, choices);
    } else {
        this->matrix.multiplyAndReduceForward(dir, rowGroupIndices, x, b, x, choices);
    }
}

template<typename ValueType>
void NativeMultiplier<ValueType>::multiplyRow(uint64_t const& rowIndex, std::vector<ValueType> const& x, ValueType& value) const {
    for (auto const& entry : this->matrix.getRow(rowIndex)) {
        value += entry.getValue() * x[entry.getColumn()];
    }
}

template<typename ValueType>
void NativeMultiplier<ValueType>::multiplyRow2(uint64_t const& rowIndex, std::vector<ValueType> const& x1, ValueType& val1, std::vector<ValueType> const& x2,
                                               ValueType& val2) const {
    for (auto const& entry : this->matrix.getRow(rowIndex)) {
        val1 += entry.getValue() * x1[entry.getColumn()];
        val2 += entry.getValue() * x2[entry.getColumn()];
    }
}

template<typename ValueType>
void NativeMultiplier<ValueType>::multAdd(std::vector<ValueType> const& x, std::vector<ValueType> const* b, std::vector<ValueType>& result) const {
    this->matrix.multiplyWithVector(x, result, b);
}

template<typename ValueType>
void NativeMultiplier<ValueType>::multAddReduce(storm::solver::OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices,
                                                std::vector<ValueType> const& x, std::vector<ValueType> const* b, std::vector<ValueType>& result,
                                                std::vector<uint64_t>* choices) const {
    this->matrix.multiplyAndReduce(dir, rowGroupIndices, x, b, result, choices);
}

template class NativeMultiplier<double>;
template class NativeMultiplier<storm::RationalNumber>;
template class NativeMultiplier<storm::RationalFunction>;
template class NativeMultiplier<storm::Interval>;

}  // namespace solver
}  // namespace storm
