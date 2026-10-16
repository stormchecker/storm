#include "storm/storage/BackwardTransitionCache.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/macros.h"

namespace storm::storage {

template<typename ValueType>
BackwardTransitionCache<ValueType>::BackwardTransitionCache(SparseMatrix<ValueType> const& forwardTransitionMatrix)
    : forwardTransitionMatrixRef(forwardTransitionMatrix), backwardTransitions(std::make_shared<SparseMatrix<ValueType>>()) {
    // Intentionally left empty.
}

template<typename ValueType>
BackwardTransitionCache<ValueType>::BackwardTransitionCache(SparseMatrix<ValueType> const& forwardTransitionMatrix,
                                                            SparseMatrix<ValueType>&& backwardTransitionMatrix)
    : forwardTransitionMatrixRef(forwardTransitionMatrix), backwardTransitions(std::make_shared<SparseMatrix<ValueType>>(std::move(backwardTransitionMatrix))) {
    // Intentionally left empty.
}

template<typename ValueType>
SparseMatrix<ValueType> const& BackwardTransitionCache<ValueType>::get() const {
    if (!isCached()) {
        *this->backwardTransitions = forwardTransitionMatrixRef.transpose(true, false);  // will drop zeroes
    }
    return *backwardTransitions;
}

template<typename ValueType>
void BackwardTransitionCache<ValueType>::clearCache() const {
    STORM_LOG_ASSERT(this->backwardTransitions != nullptr, "Backward transitions should never be nullptr");
    *this->backwardTransitions = SparseMatrix<ValueType>();
}

template<typename ValueType>
bool BackwardTransitionCache<ValueType>::isCached() const {
    STORM_LOG_ASSERT(this->backwardTransitions != nullptr, "Backward transitions should never be nullptr");
    return this->backwardTransitions->getRowCount() > 0;
}

template class BackwardTransitionCache<double>;
template class BackwardTransitionCache<storm::RationalNumber>;

}  // namespace storm::storage
