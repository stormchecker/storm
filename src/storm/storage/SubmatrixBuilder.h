#pragma once

#include <algorithm>
#include <boost/optional/optional.hpp>
#include <cstdint>
#include <limits>
#include <vector>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/OptionalRef.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::storage {

/*!
 * Helper class to extract submatrices of a fixed sparse matrix.
 * @note The referenced matrix must outlive this object and must not change its column count while this object is used.
 * @tparam ValueType The value type of the matrix.
 */
template<typename ValueType>
class SubmatrixBuilder {
   public:
    using index_type = typename SparseMatrix<ValueType>::index_type;
    using entry_type = MatrixEntry<index_type, ValueType>;

    explicit SubmatrixBuilder(SparseMatrix<ValueType> const& matrix)
        : matrix(matrix), originalToSubColumnIndex(matrix.getColumnCount(), std::numeric_limits<uint64_t>::max()) {
        // Intentionally left empty.
    }

    /*!
     * Creates the submatrix that keeps only the given rows and columns
     * If the matrix has a non-trivial row grouping, the submatrix has one row group for each original group that has at least one selected row.
     *
     * @tparam RowConstraintType, ColumnConstraintType Types (not necessarily equal) that can be iterated over. Iteration must yield the selected indices
     * (convertible to uint64_t) in *strictly ascending order* and must be repeatable (i.e., the constraint is iterated multiple times).
     * @param rowConstraint The rows to keep.
     * @param columnConstraint The columns to keep.
     * @param insertDiagonalElement If true, every row i of the submatrix contains an entry in column i (with value zero if there is no such entry in the
     *                              original matrix), provided that the submatrix has such a column.
     * @param makeZeroColumns If given, entries in these (original) columns are dropped from the submatrix (but the columns remain in the submatrix).
     *                        Must be of size matrix.getColumnCount().
     * @throws InvalidArgumentException if a constraint is empty.
     */
    template<typename RowConstraintType, typename ColumnConstraintType>
    SparseMatrix<ValueType> getByRowConstraint(RowConstraintType const& rowConstraint, ColumnConstraintType const& columnConstraint,
                                               bool const insertDiagonalElement,
                                               storm::OptionalRef<storm::storage::BitVector const> makeZeroColumns = storm::NullRef) const {
        return build<false>(rowConstraint, columnConstraint, insertDiagonalElement, makeZeroColumns);
    }

    /*!
     * Creates the submatrix that keeps only the given row groups and columns
     * The row group structure of the matrix is preserved, i.e., the i'th selected row group becomes row group i of the submatrix.
     * If insertDiagonalElement is true, the rows of row group i contain an entry in column i.
     *
     * @tparam RowConstraintType, ColumnConstraintType See getByRowConstraint.
     * @param rowGroupConstraint The row groups to keep.
     * @see getByRowConstraint for the remaining parameters.
     */
    template<typename RowConstraintType, typename ColumnConstraintType>
    SparseMatrix<ValueType> getByRowGroupConstraint(RowConstraintType const& rowGroupConstraint, ColumnConstraintType const& columnConstraint,
                                                    bool const insertDiagonalElement,
                                                    storm::OptionalRef<storm::storage::BitVector const> makeZeroColumns = storm::NullRef) const {
        return build<true>(rowGroupConstraint, columnConstraint, insertDiagonalElement, makeZeroColumns);
    }

   private:
    static constexpr uint64_t NotInSubmatrix = std::numeric_limits<uint64_t>::max();

    struct Sizes {
        index_type rows{0};
        index_type groups{0};
        index_type entryBound{0};  // upper bound on the number of entries (exact counting would require an additional, cache-unfriendly pass over the entries)
    };

    // Only used in assertions
    template<typename ConstraintType>
    static bool isStrictlyAscending(ConstraintType const& constraint) {
        bool first = true;
        uint64_t previous = 0;
        for (uint64_t index : constraint) {
            if (!first && index <= previous) {
                return false;
            }
            first = false;
            previous = index;
        }
        return true;
    }

    // Resets the used entries of the lookup table when leaving the scope (also if an exception is thrown).
    template<typename ConstraintType>
    struct ColumnMappingGuard {
        ColumnMappingGuard(std::vector<uint64_t>& mapping, ConstraintType const& columnConstraint) : mapping(mapping), columnConstraint(columnConstraint) {}
        ~ColumnMappingGuard() {
            for (uint64_t col : columnConstraint) {
                mapping[col] = NotInSubmatrix;
            }
        }
        std::vector<uint64_t>& mapping;
        ConstraintType const& columnConstraint;
    };

    /*!
     * Calls f(row, diagonalIndex) for every selected row in ascending order. Also calls g(row) right before the first row of a new (nonempty) row group (only
     * if there is a nontrivial row grouping). In group mode, the diagonal index of a row is the index of its group in the submatrix, otherwise it is the index
     * of the row in the submatrix.
     */
    template<bool GroupMode, typename ConstraintType, typename RowFunc, typename NewGroupFunc>
    void forEachSelectedRow(ConstraintType const& rowConstraint, RowFunc&& f, NewGroupFunc&& g) const {
        index_type subIndex = 0;  // index of the current row (row mode) or row group (group mode) in the submatrix
        if (matrix.hasTrivialRowGrouping()) {
            // row==group
            for (uint64_t row : rowConstraint) {
                f(row, subIndex++);
            }
        } else {
            auto const& groups = matrix.getRowGroupIndices();
            if constexpr (GroupMode) {
                for (uint64_t group : rowConstraint) {
                    g(groups[group]);
                    for (uint64_t row = groups[group], end = groups[group + 1]; row < end; ++row) {
                        f(row, subIndex);
                    }
                    ++subIndex;
                }

            } else {
                // groups[currentGroup] <= row < groups[currentGroup + 1] holds for the current row after the update below.
                index_type currentGroup = std::numeric_limits<index_type>::max();
                index_type groupEnd = 0;
                for (uint64_t row : rowConstraint) {
                    if (row >= groupEnd) {
                        // Binary search for the group of the row. Groups are skipped quickly if few rows are selected.
                        auto const searchStart = currentGroup == std::numeric_limits<index_type>::max() ? groups.begin() : groups.begin() + currentGroup + 1;
                        auto const it = std::upper_bound(searchStart, groups.end(), row);
                        currentGroup = std::distance(groups.begin(), it) - 1;
                        groupEnd = groups[currentGroup + 1];
                        g(row);
                    }
                    f(row, subIndex++);
                }
            }
        }
    }

    template<bool GroupMode, typename RowConstraintType, typename ColumnConstraintType>
    SparseMatrix<ValueType> build(RowConstraintType const& rowConstraint, ColumnConstraintType const& columnConstraint, bool const insertDiagonalElement,
                                  storm::OptionalRef<storm::storage::BitVector const> const& makeZeroColumns) const {
        STORM_LOG_THROW(!(rowConstraint.begin() == rowConstraint.end()) && !(columnConstraint.begin() == columnConstraint.end()),
                        storm::exceptions::InvalidArgumentException, "Cannot build empty submatrix.");
        STORM_LOG_ASSERT(isStrictlyAscending(rowConstraint), "Row (group) constraint must be iterated in strictly ascending order.");
        STORM_LOG_ASSERT(isStrictlyAscending(columnConstraint), "Column constraint must be iterated in strictly ascending order.");
        STORM_LOG_ASSERT(!makeZeroColumns || makeZeroColumns->size() == matrix.getColumnCount(), "Dimension mismatch.");
        bool const hasGroups = !matrix.hasTrivialRowGrouping();

        // Step 1: Fill the lookup table.
        // Columns that are made zero keep a sub index (so that column indices are consistent with the submatrix that does not have makeZeroColumns)
        // but are from that point on treated as non-existent.
        index_type subColumnCount = 0;
        for (uint64_t col : columnConstraint) {
            if (!makeZeroColumns || !makeZeroColumns->get(col)) {
                originalToSubColumnIndex[col] = subColumnCount;
            }
            ++subColumnCount;
        }
        ColumnMappingGuard<ColumnConstraintType> guard(originalToSubColumnIndex, columnConstraint);

        // Step 2: Determine the number of rows and groups and an upper bound for the number of entries of the result.
        Sizes sizes;
        forEachSelectedRow<GroupMode>(
            rowConstraint,
            [&](uint64_t row, index_type /*diagonalIndex*/) {
                ++sizes.rows;
                sizes.entryBound += (matrix.end(row) - matrix.begin(row)) + (insertDiagonalElement ? 1 : 0);
            },
            [&](uint64_t) { ++sizes.groups; });

        // Step 3: Fill the result.
        std::vector<index_type> rowIndications;
        rowIndications.reserve(sizes.rows + 1);
        std::vector<entry_type> entries;
        entries.reserve(sizes.entryBound);
        boost::optional<std::vector<index_type>> rowGroupIndices;
        if (hasGroups) {
            rowGroupIndices.emplace();
            rowGroupIndices->reserve(sizes.groups + 1);
        }
        forEachSelectedRow<GroupMode>(
            rowConstraint,
            [&](uint64_t row, index_type diagonalIndex) {
                rowIndications.push_back(entries.size());
                copyRow(row, diagonalIndex, subColumnCount, insertDiagonalElement, entries);
            },
            [&](uint64_t) {
                if (hasGroups) {
                    rowGroupIndices->push_back(rowIndications.size());
                }
            });
        rowIndications.push_back(entries.size());
        if (hasGroups) {
            rowGroupIndices->push_back(rowIndications.size() - 1);
        }
        // Release excess memory if the bound was loose. If it was tight (e.g., when keeping most of the matrix), avoid the copy.
        if (entries.size() < sizes.entryBound - sizes.entryBound / 4) {
            entries.shrink_to_fit();
        }
        return SparseMatrix<ValueType>(subColumnCount, std::move(rowIndications), std::move(entries), std::move(rowGroupIndices));
    }

    /*!
     * Appends the entries of the given row of the submatrix to the given vector.
     */
    void copyRow(uint64_t row, index_type diagonalIndex, index_type subColumnCount, bool insertDiagonalElement, std::vector<entry_type>& entries) const {
        bool diagonalMissing = insertDiagonalElement && diagonalIndex < subColumnCount;
        for (auto it = matrix.begin(row), ite = matrix.end(row); it != ite; ++it) {
            uint64_t const subColumn = originalToSubColumnIndex[it->getColumn()];
            if (subColumn == NotInSubmatrix) {
                continue;
            }
            if (diagonalMissing && subColumn >= diagonalIndex) {
                if (subColumn > diagonalIndex) {
                    entries.emplace_back(diagonalIndex, storm::utility::zero<ValueType>());
                }
                diagonalMissing = false;
            }
            entries.emplace_back(subColumn, it->getValue());
        }
        if (diagonalMissing) {
            entries.emplace_back(diagonalIndex, storm::utility::zero<ValueType>());
        }
    }

    SparseMatrix<ValueType> const& matrix;
    // Maps original column indices to submatrix column indices. Entries are NotInSubmatrix outside of calls to the get... methods.
    mutable std::vector<uint64_t> originalToSubColumnIndex;
};

}  // namespace storm::storage
