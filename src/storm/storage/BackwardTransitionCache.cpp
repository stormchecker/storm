#include "storm/storage/BackwardTransitionCache.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/macros.h"

namespace storm::storage {

template<typename ValueType>
BackwardTransitionCache<ValueType>::BackwardTransitionCache(SparseMatrix<ValueType> const& forwardTransitionMatrix, bool keepZeros)
    : forwardTransitionMatrixRef(forwardTransitionMatrix),
      keepZeros(keepZeros),
      backwardTransitions(std::make_shared<std::optional<SparseMatrix<ValueType>>>()) {
    // Intentionally left empty.
}

template<typename ValueType>
BackwardTransitionCache<ValueType>::BackwardTransitionCache(SparseMatrix<ValueType> const& forwardTransitionMatrix,
                                                            SparseMatrix<ValueType>&& backwardTransitionMatrix, bool keepZeros)
    : forwardTransitionMatrixRef(forwardTransitionMatrix),
      keepZeros(keepZeros),
      backwardTransitions(std::make_shared<std::optional<SparseMatrix<ValueType>>>(std::move(backwardTransitionMatrix))) {
    // Intentionally left empty.
}

template<typename ValueType>
SparseMatrix<ValueType> const& BackwardTransitionCache<ValueType>::get() const {
    STORM_LOG_ASSERT(this->backwardTransitions != nullptr, "Backward transitions should never be nullptr");
    if (!isCached()) {
        this->backwardTransitions->emplace(forwardTransitionMatrixRef.transpose(true, keepZeros));
    }
    return **this->backwardTransitions;
}

template<typename ValueType>
void BackwardTransitionCache<ValueType>::clearCache() const {
    STORM_LOG_ASSERT(this->backwardTransitions != nullptr, "Backward transitions should never be nullptr");
    this->backwardTransitions->reset();
}

template<typename ValueType>
bool BackwardTransitionCache<ValueType>::isCached() const {
    STORM_LOG_ASSERT(this->backwardTransitions != nullptr, "Backward transitions should never be nullptr");
    return this->backwardTransitions->has_value();
}

template class BackwardTransitionCache<double>;
template class BackwardTransitionCache<storm::RationalNumber>;

}  // namespace storm::storage
