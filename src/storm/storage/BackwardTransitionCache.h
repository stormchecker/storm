#pragma once

#include <memory>

namespace storm::storage {

template<typename ValueType>
class SparseMatrix;

/*!
 * This class caches the backward transitions of a given forward transition matrix.
 * The forward transition matrix reference needs to remain valid throughout the lifetime of this cache.
 * The cache can be copied and moved (but not assigned), and the actual backward transitions are shared.
 * This means that if the matrix is computed in one of the copies, it is available at the other copies as well.
 * @tparam ValueType
 */
template<typename ValueType>
class BackwardTransitionCache {
   public:
    explicit BackwardTransitionCache(SparseMatrix<ValueType> const& forwardTransitionMatrix);

    explicit BackwardTransitionCache(SparseMatrix<ValueType> const& forwardTransitionMatrix, SparseMatrix<ValueType>&& backwardTransitionMatrix);

    BackwardTransitionCache() = delete;
    BackwardTransitionCache(BackwardTransitionCache const&) = default;
    BackwardTransitionCache(BackwardTransitionCache&&) = default;
    // The cache references the forward transition matrix and is thus not assignable.
    BackwardTransitionCache& operator=(BackwardTransitionCache const&) = delete;
    BackwardTransitionCache& operator=(BackwardTransitionCache&&) = delete;

    SparseMatrix<ValueType> const& get() const;

    void clearCache() const;

    bool isCached() const;

   private:
    SparseMatrix<ValueType> const& forwardTransitionMatrixRef;
    mutable std::shared_ptr<SparseMatrix<ValueType>> backwardTransitions;
};
}  // namespace storm::storage
