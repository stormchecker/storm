#pragma once

#include <memory>
#include <optional>

namespace storm::storage {

template<typename ValueType>
class SparseMatrix;

/*!
 * This class caches the backward transitions of a given forward transition matrix.
 * The backward transitions are computed (using SparseMatrix::transpose) the first time they are requested.
 * They have a trivial row grouping, i.e., there is one row for each state. By default, forward transitions with value zero are dropped, i.e., the backward
 * transitions only contain entries for non-zero forward transitions (see parameter keepZeros of the constructors).
 * The forward transition matrix reference needs to remain valid throughout the lifetime of this cache.
 * The cache can be copied and moved (but not assigned), and the actual backward transitions are shared.
 * This means that if the matrix is computed (or cleared) in one of the copies, this is also the case for all other copies.
 * @tparam ValueType
 */
template<typename ValueType>
class BackwardTransitionCache {
   public:
    /*!
     * @param forwardTransitionMatrix the forward transitions
     * @param keepZeros if true, zero entries of the forward transition matrix are also contained in the backward transitions. Otherwise, they are dropped.
     */
    explicit BackwardTransitionCache(SparseMatrix<ValueType> const& forwardTransitionMatrix, bool keepZeros = false);

    /*!
     * @param forwardTransitionMatrix the forward transitions
     * @param backwardTransitionMatrix the (already computed) backward transitions
     * @param keepZeros if true, zero entries are kept when the backward transitions have to be recomputed (after clearing the cache).
     */
    explicit BackwardTransitionCache(SparseMatrix<ValueType> const& forwardTransitionMatrix, SparseMatrix<ValueType>&& backwardTransitionMatrix,
                                     bool keepZeros = false);

    BackwardTransitionCache() = delete;
    BackwardTransitionCache(BackwardTransitionCache const&) = default;
    BackwardTransitionCache(BackwardTransitionCache&&) = default;
    // The cache references the forward transition matrix and is thus not assignable.
    BackwardTransitionCache& operator=(BackwardTransitionCache const&) = delete;
    BackwardTransitionCache& operator=(BackwardTransitionCache&&) = delete;

    /*!
     * Retrieves the backward transitions. They are computed if they are not cached yet.
     */
    SparseMatrix<ValueType> const& get() const;

    /*!
     * Clears the cache (for this and all copies of this cache).
     */
    void clearCache() const;

    /*!
     * Retrieves whether the backward transitions are currently cached.
     */
    bool isCached() const;

   private:
    SparseMatrix<ValueType> const& forwardTransitionMatrixRef;
    bool const keepZeros;
    // The backward transitions are shared between all copies of this cache. For this reason, we use a shared pointer to an optional (and not just a shared
    // pointer that is the nullptr as long as the transitions are not computed): Resetting a shared pointer would only affect this copy of the cache,
    // whereas resetting the optional also clears the cache of all other copies.
    mutable std::shared_ptr<std::optional<SparseMatrix<ValueType>>> backwardTransitions;
};
}  // namespace storm::storage
