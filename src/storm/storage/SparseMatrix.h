#pragma once

#include <algorithm>
#include <cstdint>
#include <iosfwd>
#include <iterator>
#include <vector>

#include <boost/functional/hash.hpp>
#include <boost/optional.hpp>
#include <boost/range/irange.hpp>

#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/sparse/StateType.h"

#include "storm/utility/OptionalRef.h"
#include "storm/utility/constants.h"

// Forward declaration for adapter classes.
namespace storm {
namespace adapters {
template<typename ValueType>
class GmmxxAdapter;
class EigenAdapter;
class StormAdapter;
}  // namespace adapters
}  // namespace storm

namespace storm {
namespace storage {

// Forward declare matrix class.
template<typename T>
class SparseMatrix;

typedef storm::storage::sparse::state_type SparseMatrixIndexType;

template<typename IndexType, typename ValueType>
class MatrixEntry {
   public:
    typedef IndexType index_type;
    typedef ValueType value_type;

    /*!
     * Constructs a matrix entry with the given column and value.
     *
     * @param column The column of the matrix entry.
     * @param value The value of the matrix entry.
     */
    MatrixEntry(index_type column, value_type value);

    /*!
     * Move-constructs the matrix entry fro the given column-value pair.
     *
     * @param pair The column-value pair from which to move-construct the matrix entry.
     */
    MatrixEntry(std::pair<index_type, value_type>&& pair);

    MatrixEntry() = default;
    MatrixEntry(MatrixEntry const& other) = default;
    MatrixEntry& operator=(MatrixEntry const& other) = default;
    MatrixEntry(MatrixEntry&& other) = default;
    MatrixEntry& operator=(MatrixEntry&& other) = default;

    /*!
     * Retrieves the column of the matrix entry.
     *
     * @return The column of the matrix entry.
     */
    index_type const& getColumn() const;

    /*!
     * Sets the column of the current entry.
     *
     * @param column The column to set for this entry.
     */
    void setColumn(index_type const& column);

    /*!
     * Retrieves the value of the matrix entry.
     *
     * @return The value of the matrix entry.
     */
    value_type const& getValue() const;

    /*!
     * Sets the value of the entry in the matrix.
     *
     * @param value The value that is to be set for this entry.
     */
    void setValue(value_type const& value);

    /*!
     * Retrieves a pair of column and value that characterizes this entry.
     *
     * @return A column-value pair that characterizes this entry.
     */
    std::pair<index_type, value_type> const& getColumnValuePair() const;

    /*!
     * Multiplies the entry with the given factor and returns the result.
     *
     * @param factor The factor with which to multiply the entry.
     */
    MatrixEntry operator*(value_type factor) const;

    bool operator==(MatrixEntry const& other) const;
    bool operator!=(MatrixEntry const& other) const;

    template<typename IndexTypePrime, typename ValueTypePrime>
    friend std::ostream& operator<<(std::ostream& out, MatrixEntry<IndexTypePrime, ValueTypePrime> const& entry);

   private:
    // The actual matrix entry.
    std::pair<index_type, value_type> entry;
};

/*!
 * Computes the hash value of a matrix entry.
 */
template<typename IndexType, typename ValueType>
std::size_t hash_value(MatrixEntry<IndexType, ValueType> const& matrixEntry) {
    std::size_t seed = 0;
    boost::hash_combine(seed, matrixEntry.getColumn());
    boost::hash_combine(seed, matrixEntry.getValue());
    return seed;
}

/*!
 * A class that can be used to build a sparse matrix by adding value by value.
 */
template<typename ValueType>
class SparseMatrixBuilder {
   public:
    typedef SparseMatrixIndexType index_type;
    typedef ValueType value_type;

    /*!
     * Constructs a sparse matrix builder producing a matrix with the given number of rows, columns and entries.
     * The number of rows, columns and entries is reserved upon creation. If more rows/columns or entries are
     * added, this will possibly lead to a reallocation.
     *
     * @param rows The number of rows of the resulting matrix.
     * @param columns The number of columns of the resulting matrix.
     * @param entries The number of entries of the resulting matrix.
     * @param forceDimensions If this flag is set, the matrix is expected to have exactly the given number of
     * rows, columns and entries for all of these entities that are set to a nonzero value.
     * @param hasCustomRowGrouping A flag indicating whether the builder is used to create a non-canonical
     * grouping of rows for this matrix.
     * @param rowGroups The number of row groups of the resulting matrix. This is only relevant if the matrix
     * has a custom row grouping.
     */
    SparseMatrixBuilder(index_type rows = 0, index_type columns = 0, index_type entries = 0, bool forceDimensions = true, bool hasCustomRowGrouping = false,
                        index_type rowGroups = 0);

    /*!
     * Moves the contents of the given matrix into the matrix builder so that its contents can be modified again.
     * This is, for example, useful if rows need to be added to the matrix.
     *
     * @param matrix The matrix that is to be made editable again.
     */
    SparseMatrixBuilder(SparseMatrix<ValueType>&& matrix);

    /*!
     * Sets the matrix entry at the given row and column to the given value. After all entries have been added,
     * a call to finalize(false) is mandatory.
     *
     * Note: this is a linear setter. That is, it must be called consecutively for each entry, row by row and
     * column by column. As multiple entries per column are admitted, consecutive calls to this method are
     * admitted to mention the same row-column-pair. If rows are skipped entirely, the corresponding rows are
     * treated as empty. If these constraints are not met, an exception is thrown.
     *
     * @param row The row in which the matrix entry is to be set.
     * @param column The column in which the matrix entry is to be set.
     * @param value The value that is to be set at the specified row and column.
     */
    void addNextValue(index_type row, index_type column, value_type const& value);

    /*!
     * Starts a new row group in the matrix. Note that this needs to be called before any entries in the new row
     * group are added.
     *
     * @param startingRow The starting row of the new row group.
     */
    void newRowGroup(index_type startingRow);

    /*
     * Finalizes the sparse matrix to indicate that initialization process has been completed and the matrix
     * may now be used. This must be called after all entries have been added to the matrix via addNextValue.
     *
     * @param overriddenRowCount If this is set to a value that is greater than the current number of rows,
     * this will cause the finalize method to add empty rows at the end of the matrix until the given row count
     * has been matched. Note that this will *not* override the row count that has been given upon construction
     * (if any), but will only take effect if the matrix has been created without the number of rows given.
     * @param overriddenColumnCount If this is set to a value that is greater than the current number of columns,
     * this will cause the finalize method to set the number of columns to the given value. Note that this will
     * *not* override the column count that has been given upon construction (if any), but will only take effect
     * if the matrix has been created without the number of columns given. By construction, the matrix will have
     * no entries in the columns that have been added this way.
     * @param overriddenRowGroupCount If this is set to a value that is greater than the current number of row
     * groups, this will cause the method to set the number of row groups to the given value. Note that this will
     * *not* override the row group count that has been given upon construction (if any), but will only take
     * effect if the matrix has been created without the number of row groups given. By construction, the row
     * groups added this way will be empty.
     */
    SparseMatrix<value_type> build(index_type overriddenRowCount = 0, index_type overriddenColumnCount = 0, index_type overriddenRowGroupCount = 0);

    /*!
     * Retrieves the most recently used row.
     *
     * @return The most recently used row.
     */
    index_type getLastRow() const;

    /*!
     * Retrieves the current row group count.
     *
     * @return The current row group count.
     */
    index_type getCurrentRowGroupCount() const;

    /*!
     * Retrieves the most recently used row.
     *
     * @return The most recently used row.
     */
    index_type getLastColumn() const;

    /*!
     * Replaces all columns with id > offset according to replacements.
     * Every state  with id offset+i is replaced by the id in replacements[i].
     * Afterwards the columns are sorted.
     *
     * @param replacements Mapping indicating the replacements from offset+i -> value of i.
     * @param offset Offset to add to each id in vector index.
     */
    void replaceColumns(std::vector<index_type> const& replacements, index_type offset);

    /*!
     * Makes sure that a diagonal entry will be inserted at the given row.
     * All other entries of this row must be set immediately after calling this (without setting values at other rows in between)
     * The provided row must not be smaller than the row of the most recent insertion.
     * If there is a row grouping, the column of the diagonal entry will correspond to the current row group.
     * If addNextValue is called on the given row and the diagonal column, we take the sum of the two values provided to addDiagonalEntry and addNextValue
     */
    void addDiagonalEntry(index_type row, ValueType const& value);

   private:
    // A flag indicating whether a row count was set upon construction.
    bool initialRowCountSet;

    // The row count that was initially set (if any).
    index_type initialRowCount;

    // A flag indicating whether a column count was set upon construction.
    bool initialColumnCountSet;

    // The column count that was initially set (if any).
    index_type initialColumnCount;

    // A flag indicating whether an entry count was set upon construction.
    bool initialEntryCountSet;

    // The number of entries in the matrix.
    index_type initialEntryCount;

    // A flag indicating whether the initially given dimensions are to be enforced on the resulting matrix.
    bool forceInitialDimensions;

    // A flag indicating whether the builder is to construct a custom row grouping for the matrix.
    bool hasCustomRowGrouping;

    // A flag indicating whether the number of row groups was set upon construction.
    bool initialRowGroupCountSet;

    // The number of row groups in the matrix.
    index_type initialRowGroupCount;

    // The vector that stores the row-group indices (if they are non-trivial).
    boost::optional<std::vector<index_type>> rowGroupIndices;

    // The storage for the columns and values of all entries in the matrix.
    std::vector<MatrixEntry<index_type, value_type>> columnsAndValues;

    // A vector containing the indices at which each given row begins. This index is to be interpreted as an
    // index in the valueStorage and the columnIndications vectors. Put differently, the values of the entries
    // in row i are valueStorage[rowIndications[i]] to valueStorage[rowIndications[i + 1]] where the last
    // entry is not included anymore.
    std::vector<index_type> rowIndications;

    // Stores the current number of entries in the matrix. This is used for inserting an entry into a matrix
    // with preallocated storage.
    index_type currentEntryCount;

    // Stores the row of the last entry in the matrix. This is used for correctly inserting an entry into a
    // matrix.
    index_type lastRow;

    // Stores the column of the currently last entry in the matrix. This is used for correctly inserting an
    // entry into a matrix.
    index_type lastColumn;

    // Stores the highest column at which an entry was inserted into the matrix.
    index_type highestColumn;

    // Stores the currently active row group. This is used for correctly constructing the row grouping of the
    // matrix.
    index_type currentRowGroupCount;

    boost::optional<ValueType> pendingDiagonalEntry;
};

/*!
 * A class that holds a possibly non-square matrix in the compressed row storage format. That is, it is supposed
 * to store non-zero entries only, but zeros may be explicitly stored if necessary for certain operations.
 * Likewise, the matrix is intended to store one value per column only. However, the functions provided by the
 * matrix are implemented in a way that makes it safe to store several entries per column.
 *
 * The creation of a matrix can be done in several ways. If the number of rows, columns and entries is known
 * prior to creating the matrix, the matrix can be constructed using this knowledge, which saves reallocations.
 * On the other hand, if either one of these values is not known a priori, the matrix can be constructed as an
 * empty matrix that needs to perform reallocations as more and more entries are inserted in the matrix.
 *
 * It should be observed that due to the nature of the sparse matrix format, entries can only be inserted in
 * order, i.e. row by row and column by column.
 */
template<typename ValueType>
class SparseMatrix {
   public:
    // Declare adapter classes as friends to use internal data.
    friend class storm::adapters::GmmxxAdapter<ValueType>;
    friend class storm::adapters::EigenAdapter;
    friend class storm::adapters::StormAdapter;
    friend class SparseMatrixBuilder<ValueType>;

    typedef SparseMatrixIndexType index_type;
    typedef ValueType value_type;
    typedef typename std::vector<MatrixEntry<index_type, value_type>>::iterator iterator;
    typedef typename std::vector<MatrixEntry<index_type, value_type>>::const_iterator const_iterator;

    /*!
     * This class represents a number of consecutive rows of the matrix.
     */
    class rows {
       public:
        /*!
         * Constructs an object that represents the rows defined by the value of the first entry, the column
         * of the first entry and the number of entries in this row set.
         *
         * @param begin An iterator that points to the beginning of the row.
         * @param entryCount The number of entrys in the rows.
         */
        rows(iterator begin, index_type entryCount);

        /*!
         * Retrieves an iterator that points to the beginning of the rows.
         *
         * @return An iterator that points to the beginning of the rows.
         */
        iterator begin();

        /*!
         * Retrieves an iterator that points past the last entry of the rows.
         *
         * @return An iterator that points past the last entry of the rows.
         */
        iterator end();

        /*!
         * Retrieves the number of entries in the rows.
         *
         * @return The number of entries in the rows.
         */
        index_type getNumberOfEntries() const;

       private:
        // The pointer to the columnd and value of the first entry.
        iterator beginIterator;

        // The number of non-zero entries in the rows.
        index_type entryCount;
    };

    /*!
     * This class represents a number of consecutive rows of the matrix.
     */
    class const_rows {
       public:
        /*!
         * Constructs an object that represents the rows defined by the value of the first entry, the column
         * of the first entry and the number of entries in this row set.
         *
         * @param begin An iterator that points to the beginning of the row.
         * @param entryCount The number of entrys in the rows.
         */
        const_rows(const_iterator begin, index_type entryCount);

        /*!
         * Retrieves an iterator that points to the beginning of the rows.
         *
         * @return An iterator that points to the beginning of the rows.
         */
        const_iterator begin() const;

        /*!
         * Retrieves an iterator that points past the last entry of the rows.
         *
         * @return An iterator that points past the last entry of the rows.
         */
        const_iterator end() const;

        /*!
         * Retrieves the number of entries in the rows.
         *
         * @return The number of entries in the rows.
         */
        index_type getNumberOfEntries() const;

       private:
        // The pointer to the column and value of the first entry.
        const_iterator beginIterator;

        // The number of non-zero entries in the rows.
        index_type entryCount;
    };

    /*!
     * An enum representing the internal state of the matrix. After creation, the matrix is UNINITIALIZED.
     * Only after a call to finalize(), the status of the matrix is set to INITIALIZED and the matrix can be
     * used.
     */
    enum MatrixStatus { UNINITIALIZED, INITIALIZED };

    /*!
     * Constructs an empty sparse matrix.
     */
    SparseMatrix();

    /*!
     * Constructs a sparse matrix by performing a deep-copy of the given matrix.
     *
     * @param other The matrix from which to copy the content.
     */
    SparseMatrix(SparseMatrix<value_type> const& other);

    /*!
     * Constructs a sparse matrix by performing a deep-copy of the given matrix.
     *
     * @param other The matrix from which to copy the content.
     * @param insertDiagonalElements If set to true, the copy will have all diagonal elements. If they did not
     * exist in the original matrix, they are inserted and set to value zero.
     */
    SparseMatrix(SparseMatrix<value_type> const& other, bool insertDiagonalElements);

    /*!
     * Constructs a sparse matrix by moving the contents of the given matrix to the newly created one.
     *
     * @param other The matrix from which to move the content.
     */
    SparseMatrix(SparseMatrix<value_type>&& other);

    /*!
     * Constructs a sparse matrix by copying the given contents.
     *
     * @param columnCount The number of columns of the matrix.
     * @param rowIndications The row indications vector of the matrix to be constructed.
     * @param columnsAndValues The vector containing the columns and values of the entries in the matrix.
     * @param rowGroupIndices The vector representing the row groups in the matrix.
     */
    SparseMatrix(index_type columnCount, std::vector<index_type> const& rowIndications,
                 std::vector<MatrixEntry<index_type, value_type>> const& columnsAndValues, boost::optional<std::vector<index_type>> const& rowGroupIndices);

    /*!
     * Constructs a sparse matrix by moving the given contents.
     *
     * @param columnCount The number of columns of the matrix.
     * @param rowIndications The row indications vector of the matrix to be constructed.
     * @param columnsAndValues The vector containing the columns and values of the entries in the matrix.
     * @param rowGroupIndices The vector representing the row groups in the matrix.
     */
    SparseMatrix(index_type columnCount, std::vector<index_type>&& rowIndications, std::vector<MatrixEntry<index_type, value_type>>&& columnsAndValues,
                 boost::optional<std::vector<index_type>>&& rowGroupIndices);

    /*!
     * Assigns the contents of the given matrix to the current one by deep-copying its contents.
     *
     * @param other The matrix from which to copy-assign.
     */
    SparseMatrix<value_type>& operator=(SparseMatrix<value_type> const& other);

    /*!
     * Assigns the contents of the given matrix to the current one by moving its contents.
     *
     * @param other The matrix from which to move to contents.
     */
    SparseMatrix<value_type>& operator=(SparseMatrix<value_type>&& other);

    /*!
     * Determines whether the current and the given matrix are semantically equal.
     *
     * @param other The matrix with which to compare the current matrix.
     * @return True iff the given matrix is semantically equal to the current one.
     */
    bool operator==(SparseMatrix<value_type> const& other) const;

    /*!
     * Returns the number of rows of the matrix.
     *
     * @return The number of rows of the matrix.
     */
    index_type getRowCount() const;

    /*!
     * Returns the number of columns of the matrix.
     *
     * @return The number of columns of the matrix.
     */
    index_type getColumnCount() const;

    /*!
     * Returns the number of entries in the matrix.
     *
     * @return The number of entries in the matrix.
     */
    index_type getEntryCount() const;

    /*!
     * Returns the number of entries in the given row group of the matrix.
     *
     * @return The number of entries in the given row group of the matrix.
     */
    index_type getRowGroupEntryCount(index_type const group) const;

    /*!
     * Returns the number of entries in the given row of the matrix.
     * @param row  Which row
     * @return Number of entries
     */
    index_type getRowEntryCount(index_type const row) const;

    /*!
     * Returns the cached number of nonzero entries in the matrix.
     *
     * @see updateNonzeroEntryCount()
     *
     * @return The number of nonzero entries in the matrix.
     */
    index_type getNonzeroEntryCount() const;

    /*!
     * Recompute the nonzero entry count
     */
    void updateNonzeroEntryCount() const;

    /*!
     * Recomputes the number of columns and the number of non-zero entries.
     */
    void updateDimensions() const;

    /*!
     * Change the nonzero entry count by the provided value.
     *
     * @param difference Difference between old and new nonzero entry count.
     */
    void updateNonzeroEntryCount(std::make_signed<index_type>::type difference);

    /*!
     * Returns the number of row groups in the matrix.
     *
     * @return The number of row groups in the matrix.
     */
    index_type getRowGroupCount() const;

    /*!
     * Returns the size of the given row group.
     *
     * @param group The group whose size to retrieve.
     * @return The number of rows that belong to the given row group.
     */
    index_type getRowGroupSize(index_type group) const;

    /*!
     * Returns the size of the largest row group of the matrix
     */
    index_type getSizeOfLargestRowGroup() const;

    /*!
     * Returns the total number of rows that are in one of the specified row groups.
     */
    index_type getNumRowsInRowGroups(storm::storage::BitVector const& groupConstraint) const;

    /*!
     * Returns the grouping of rows of this matrix.
     *
     * @return The grouping of rows of this matrix.
     */
    std::vector<index_type> const& getRowGroupIndices() const;

    /*!
     * Returns the row indices within the given group
     */
    boost::integer_range<index_type> getRowGroupIndices(index_type group) const;

    /*!
     * Returns the entry indices within the given row
     */
    std::vector<index_type> const& getRowIndices() const;

    /*!
     * Swaps the grouping of rows of this matrix.
     *
     * @return The old grouping of rows of this matrix.
     */
    std::vector<index_type> swapRowGroupIndices(std::vector<index_type>&& newRowGrouping);

    /*!
     * Sets the row grouping to the given one.
     * @note It is assumed that the new row grouping is non-trivial.
     *
     * @param newRowGroupIndices The new row group indices.
     */
    void setRowGroupIndices(std::vector<index_type> const& newRowGroupIndices);

    /*!
     * Retrieves whether the matrix has a trivial row grouping.
     *
     * @return True iff the matrix has a trivial row grouping.
     */
    bool hasTrivialRowGrouping() const;

    /*!
     * Makes the row grouping of this matrix trivial.
     * Has no effect when the row grouping is already trivial.
     */
    void makeRowGroupingTrivial();

    /*!
     * Returns a bitvector representing the set of rows,
     * with all indices set that correspond to one of the selected row groups.
     *
     * @param groups the selected row groups
     * @return a bit vector that is true at position i iff the row group of row i is selected.
     */
    storm::storage::BitVector getRowFilter(storm::storage::BitVector const& groupConstraint) const;

    /*!
     * Returns the indices of all rows that
     * * are in a selected group and
     * * only have entries within the selected columns.
     *
     * @param groupConstraint the selected groups
     * @param columnConstraints the selected columns
     * @return a bit vector that is true at position i iff row i satisfies the constraints.
     */
    storm::storage::BitVector getRowFilter(storm::storage::BitVector const& groupConstraint, storm::storage::BitVector const& columnConstraints) const;

    /*!
     * Returns the indices of all row groups selected by the row constraints
     *
     * @param rowConstraint the selected rows
     * @param setIfForAllRowsInGroup if true, a group is selected if the rowConstraint is true for *all* rows within that group. If false, a group is selected
     * if the rowConstraint is true for *some* row within that group
     * @return a bit vector that is true at position i iff row i satisfies the constraints.
     */
    storm::storage::BitVector getRowGroupFilter(storm::storage::BitVector const& rowConstraint, bool setIfForAllRowsInGroup) const;

    /*!
     * This function makes the given rows absorbing.
     *
     * @param rows A bit vector indicating which rows are to be made absorbing.
     * @param dropZeroEntries if true, zero entries resulting from the transformation are dropped from the matrix after the transformation.
     * Dropping zero entries takes time linear in the number of matrix entries.
     */
    void makeRowsAbsorbing(storm::storage::BitVector const& rows, bool dropZeroEntries = false);

    /*!
     * This function makes the groups of rows given by the bit vector absorbing.
     *
     * @param rowGroupConstraint A bit vector indicating which row groups to make absorbing.
     * @param dropZeroEntries if true, zero entries resulting from the transformation are dropped from the matrix after the transformation.
     * Dropping zero entries takes time linear in the number of matrix entries.
     */
    void makeRowGroupsAbsorbing(storm::storage::BitVector const& rowGroupConstraint, bool dropZeroEntries = false);

    /*!
     * This function makes the given row Dirac. This means that all entries will be set to 0 except the one
     * at the specified column, which is set to 1 instead.
     *
     * @param row The row to be made Dirac.
     * @param column The index of the column whose value is to be set to 1.
     * @param dropZeroEntries if true, zero entries resulting from the transformation are dropped from the matrix after the transformation.
     * Dropping zero entries takes time linear in the number of matrix entries.
     */
    void makeRowDirac(index_type row, index_type column, bool dropZeroEntries = false);

    /*!
     * Sums the entries in all rows.
     *
     * @return The vector of sums of the entries in the respective rows.
     */
    std::vector<ValueType> getRowSumVector() const;

    /*!
     * Sums the entries in the given row and columns.
     *
     * @param row The row whose entries to add.
     * @param columns A bit vector that indicates which columns to add.
     * @return The sum of the entries in the given row and columns.
     */
    value_type getConstrainedRowSum(index_type row, storm::storage::BitVector const& columns) const;

    /*!
     * Computes a vector whose i-th entry is the sum of the entries in the i-th selected row where only those
     * entries are added that are in selected columns.
     *
     * @param rowConstraint A bit vector that indicates for which rows to compute the constrained sum.
     * @param columnConstraint A bit vector that indicates which columns to add in the selected rows.
     *
     * @return A vector whose elements are the sums of the selected columns in each row.
     */
    std::vector<value_type> getConstrainedRowSumVector(storm::storage::BitVector const& rowConstraint, storm::storage::BitVector const& columnConstraint) const;

    /*!
     * Computes a vector whose entries represent the sums of selected columns for all rows in selected row
     * groups.
     *
     * @param rowGroupConstraint A bit vector that indicates which row groups are to be considered.
     * @param columnConstraint A bit vector that indicates which columns to sum.
     * @return A vector whose entries represent the sums of selected columns for all rows in selected row
     * groups.
     */
    std::vector<value_type> getConstrainedRowGroupSumVector(storm::storage::BitVector const& rowGroupConstraint,
                                                            storm::storage::BitVector const& columnConstraint) const;

    /*!
     * Creates a submatrix of the current matrix by dropping all rows and columns whose bits are not
     * set to one in the given bit vector.
     *
     * @param useGroups If set to true, the constraint for the rows is interpreted as selecting whole row groups.
     * If it is not set, the row constraint is interpreted over the actual rows. Note that empty row groups will
     * be dropped altogether. That is, if no row of a row group is selected *or* the row group is already empty,
     * the submatrix will not have this row group.
     * @param constraint A bit vector indicating which rows to keep.
     * @param columnConstraint A bit vector indicating which columns to keep.
     * @param insertDiagonalEntries If set to true, the resulting matrix will have zero entries in column i for
     * each row i, if there is no value yet. This can then be used for inserting other values later.
     * @return A matrix corresponding to a submatrix of the current matrix in which only rows and columns given
     * by the constraints are kept and all others are dropped.
     */
    SparseMatrix getSubmatrix(bool useGroups, storm::storage::BitVector const& rowConstraint, storm::storage::BitVector const& columnConstraint,
                              bool insertDiagonalEntries = false, storm::storage::BitVector const& makeZeroColumns = storm::storage::BitVector()) const;

    /*!
     * Restrict rows in grouped rows matrix. Ensures that the number of groups stays the same.
     *
     * @param rowsToKeep A bit vector indicating which rows to keep.
     * @param allowEmptyRowGroups if set to true, the result can potentially have empty row groups.
     *                            Otherwise, it is asserted that there are no empty row groups.
     *
     * @note The resulting matrix will always have a non-trivial row grouping even if the current one is trivial.
     *
     */
    SparseMatrix restrictRows(storm::storage::BitVector const& rowsToKeep, bool allowEmptyRowGroups = false) const;

    /*!
     * Permute rows of the matrix according to the vector.
     * That is, in row i, write the entry of row inversePermutation[i].
     * Consequently, a single row might actually be written into multiple other rows, and the function application is not necessarily a permutation.
     * Notice that this method does *not* touch column entries, nor the row grouping.
     */
    SparseMatrix permuteRows(std::vector<index_type> const& inversePermutation) const;

    /*!
     * Permutes row groups and columns of the matrix according to the given  permutations.
     * That is, in row group i, write the entries of row group inverseRowGroupPermutation[i] and in column columnPermutation[j], write the entries of column j.
     * @pre inverseRowGroupPermutation and columnPermutation must be permutations (i.e., they must contain each index exactly once).
     * @note the permutation for the row groups is inverted while the one for columns is not!
     * @return the permuted matrix.
     */
    SparseMatrix permuteRowGroupsAndColumns(std::vector<index_type> const& inverseRowGroupPermutation, std::vector<index_type> const& columnPermutation) const;

    /*!
     * Returns a copy of this matrix that only considers entries in the selected rows.
     * Non-selected rows will not have any entries
     *
     * @note does not change the dimensions (row-, column-, and rowGroup count) of this matrix
     * @param rowFilter the selected rows
     */
    SparseMatrix filterEntries(storm::storage::BitVector const& rowFilter) const;

    /*!
     * Removes all zero entries from this.
     */
    void dropZeroEntries();

    /**
     * Compares two rows.
     * @param i1 Index of first row
     * @param i2 Index of second row
     * @return True if the rows have identical entries.
     */
    bool compareRows(index_type i1, index_type i2) const;

    /*!
     * Finds duplicate rows in a rowgroup.
     */
    BitVector duplicateRowsInRowgroups() const;

    /**
     * Swaps the two rows.
     * @param row1 Index of first row
     * @param row2 Index of second row
     */
    void swapRows(index_type const& row1, index_type const& row2);

    /*!
     * Selects exactly one row from each row group of this matrix and returns the resulting matrix.
     *
     * @param insertDiagonalEntries If set to true, the resulting matrix will have zero entries in column i for
     * each row in row group i. This can then be used for inserting other values later.
     * @return A submatrix of the current matrix by selecting one row out of each row group.
     */
    SparseMatrix selectRowsFromRowGroups(std::vector<index_type> const& rowGroupToRowIndexMapping, bool insertDiagonalEntries = true) const;

    /*!
     * Selects the rows that are given by the sequence of row indices, allowing to select rows arbitrarily often and with an arbitrary order
     * The resulting matrix will have a trivial row grouping.
     *
     * @param rowIndexSequence the sequence of row indices which specifies, which rows are contained in the new matrix
     * @param insertDiagonalEntries If set to true, the resulting matrix will have zero entries in column i for
     * each row. This can then be used for inserting other values later.
     * @return A matrix which rows are selected from this matrix according to the given index sequence
     */
    SparseMatrix selectRowsFromRowIndexSequence(std::vector<index_type> const& rowIndexSequence, bool insertDiagonalEntries = true) const;

    /*!
     * Transposes the matrix.
     *
     * @param joinGroups A flag indicating whether the row groups are supposed to be treated as single rows.
     * @param keepZeros A flag indicating whether entries with value zero should be kept.
     *
     * @return A sparse matrix that represents the transpose of this matrix.
     */
    storm::storage::SparseMatrix<value_type> transpose(bool joinGroups = false, bool keepZeros = false) const;

    /*!
     * Transposes the matrix w.r.t. the selected rows.
     * This is equivalent to selectRowsFromRowGroups(rowGroupChoices, false).transpose(false, keepZeros) but avoids creating one intermediate matrix.
     *
     * @param rowGroupChoices A mapping from each row group index to a selected row in this group.
     * @param keepZeros A flag indicating whether entries with value zero should be kept.
     *
     */
    SparseMatrix<ValueType> transposeSelectedRowsFromRowGroups(std::vector<uint64_t> const& rowGroupChoices, bool keepZeros = false) const;

    /*!
     * Transforms the matrix into an equation system. That is, it transforms the matrix A into a matrix (1-A).
     */
    void convertToEquationSystem();

    /*!
     * Inverts all entries on the diagonal, i.e. sets the diagonal values to one minus their previous value.
     * Requires the matrix to contain each diagonal entry and to be square.
     */
    void invertDiagonal();

    /*!
     * Negates (w.r.t. addition) all entries that are not on the diagonal.
     */
    void negateAllNonDiagonalEntries();

    /*!
     * Sets all diagonal elements to zero.
     * @param dropZeroEntries if true, zero entries resulting from the transformation are dropped from the matrix after the transformation.
     * Dropping zero entries takes time linear in the number of matrix entries.
     */
    void deleteDiagonalEntries(bool dropZeroEntries = false);

    /*!
     * Calculates the Jacobi decomposition of this sparse matrix. For this operation, the matrix must be square.
     *
     * @return A pair (L+U, D^-1) containing the matrix L+U and the inverted diagonal D^-1 (as a vector).
     */
    std::pair<storm::storage::SparseMatrix<value_type>, std::vector<value_type>> getJacobiDecomposition() const;

    /*!
     * Performs a pointwise multiplication of the entries in the given row of this matrix and the entries of
     * the given row of the other matrix and returns the sum.
     *
     * @param otherMatrix A reference to the matrix with which to perform the pointwise multiplication. This
     * matrix must be a submatrix of the current matrix in the sense that it may not have entries at indices
     * where there is no entry in the current matrix.
     * @return the sum of the product of the entries in the given row.
     */
    template<typename OtherValueType, typename ResultValueType = OtherValueType>
    ResultValueType getPointwiseProductRowSum(storm::storage::SparseMatrix<OtherValueType> const& otherMatrix, index_type const& row) const;

    /*!
     * Performs a pointwise matrix multiplication of the matrix with the given matrix and returns a vector
     * containing the sum of the entries in each row of the resulting matrix.
     *
     * @param otherMatrix A reference to the matrix with which to perform the pointwise multiplication. This
     * matrix must be a submatrix of the current matrix in the sense that it may not have entries at indices
     * where there is no entry in the current matrix.
     * @return A vector containing the sum of the entries in each row of the matrix resulting from pointwise
     * multiplication of the current matrix with the given matrix.
     */
    template<typename OtherValueType, typename ResultValueType = OtherValueType>
    std::vector<ResultValueType> getPointwiseProductRowSumVector(storm::storage::SparseMatrix<OtherValueType> const& otherMatrix) const;

    /*!
     * Multiplies the matrix with the given vector and writes the result to the given result vector.
     *
     * @param vector The vector with which to multiply the matrix.
     * @param result The vector that is supposed to hold the result of the multiplication after the operation.
     * @param summand If given, this summand will be added to the result of the multiplication.
     * @return The product of the matrix and the given vector as the content of the given result vector.
     */
    void multiplyWithVector(std::vector<value_type> const& vector, std::vector<value_type>& result, std::vector<value_type> const* summand = nullptr) const;

    void multiplyWithVectorForward(std::vector<value_type> const& vector, std::vector<value_type>& result,
                                   std::vector<value_type> const* summand = nullptr) const;
    void multiplyWithVectorBackward(std::vector<value_type> const& vector, std::vector<value_type>& result,
                                    std::vector<value_type> const* summand = nullptr) const;

    /*!
     * Multiplies the matrix with the given vector, reduces it according to the given direction and and writes
     * the result to the given result vector.
     *
     * @param dir The optimization direction for the reduction.
     * @param rowGroupIndices The row groups for the reduction
     * @param vector The vector with which to multiply the matrix.
     * @param summand If given, this summand will be added to the result of the multiplication.
     * @param result The vector that is supposed to hold the result of the multiplication after the operation.
     * @param choices If given, the choices made in the reduction process will be written to this vector. Note
     * that if the direction is maximize, the choice for a row group is only updated if the value obtained with
     * the 'new' choice has a value strictly better (wrt. to the optimization direction) value.
     * @return The resulting vector the content of the given result vector.
     */
    void multiplyAndReduce(storm::solver::OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices, std::vector<ValueType> const& vector,
                           std::vector<ValueType> const* summand, std::vector<ValueType>& result, std::vector<uint64_t>* choices) const;

    void multiplyAndReduceForward(storm::solver::OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices,
                                  std::vector<ValueType> const& vector, std::vector<ValueType> const* b, std::vector<ValueType>& result,
                                  std::vector<uint64_t>* choices) const;
    template<typename Compare>
    void multiplyAndReduceForward(std::vector<uint64_t> const& rowGroupIndices, std::vector<ValueType> const& vector, std::vector<ValueType> const* summand,
                                  std::vector<ValueType>& result, std::vector<uint64_t>* choices) const;

    void multiplyAndReduceBackward(storm::solver::OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices,
                                   std::vector<ValueType> const& vector, std::vector<ValueType> const* b, std::vector<ValueType>& result,
                                   std::vector<uint64_t>* choices) const;
    template<typename Compare>
    void multiplyAndReduceBackward(std::vector<uint64_t> const& rowGroupIndices, std::vector<ValueType> const& vector, std::vector<ValueType> const* b,
                                   std::vector<ValueType>& result, std::vector<uint64_t>* choices) const;

    /*!
     * Multiplies a single row of the matrix with the given vector and returns the result
     *
     * @param row The index of the row with which to multiply
     * @param vector The vector with which to multiply the row.
     * @return the result of the multiplication.
     */
    value_type multiplyRowWithVector(index_type row, std::vector<value_type> const& vector) const;

    /*!
     * Multiplies the vector to the matrix from the left and writes the result to the given result vector.
     *
     * @param vector The vector with which the matrix is to be multiplied. This vector is interpreted as being
     * a row vector.
     * @param result The vector that is supposed to hold the result of the multiplication after the operation.
     * @return The product of the matrix and the given vector as the content of the given result vector. The
     * result is to be interpreted as a row vector.
     */
    void multiplyVectorWithMatrix(std::vector<value_type> const& vector, std::vector<value_type>& result) const;

    /*!
     * Scales each row of the matrix, i.e., multiplies each element in row i with factors[i]
     *
     * @param factors The factors with which each row is scaled.
     */
    void scaleRowsInPlace(std::vector<value_type> const& factors);

    /*!
     * Divides each row of the matrix, i.e., divides each element in row i with divisors[i]
     *
     * @param divisors The divisors with which each row is divided.
     */
    void divideRowsInPlace(std::vector<value_type> const& divisors);

    /*!
     * Performs one step of the successive over-relaxation technique.
     *
     * @param omega The Omega parameter for SOR.
     * @param x The current solution vector. The result will be written to the very same vector.
     * @param b The 'right-hand side' of the problem.
     */
    void performSuccessiveOverRelaxationStep(ValueType omega, std::vector<ValueType>& x, std::vector<ValueType> const& b) const;

    /*!
     * Performs one step of the Walker-Chae technique.
     *
     * @param x The current solution vector.
     * @param columnSums The sums the individual columns.
     * @param b The 'right-hand side' of the problem.
     * @param ax A vector resulting from multiplying the current matrix with the vector x.
     * @param result The vector to which to write the result.
     */
    void performWalkerChaeStep(std::vector<ValueType> const& x, std::vector<ValueType> const& columnSums, std::vector<ValueType> const& b,
                               std::vector<ValueType> const& ax, std::vector<ValueType>& result) const;

    /*!
     * Computes the sum of the entries in a given row.
     *
     * @param row The row that is to be summed.
     * @return The sum of the selected row.
     */
    value_type getRowSum(index_type row) const;

    /*!
     * Returns the number of non-constant entries
     */
    index_type getNonconstantEntryCount() const;

    /*!
     * Returns the number of rowGroups that contain a non-constant value
     */
    index_type getNonconstantRowGroupCount() const;

    /*!
     * Checks for each row whether (i) each entry is between zero and one and (ii) all entries sum to one.
     * For interval models, we check if each interval contains a value between zero and one and if the sum contains one.
     * For rational functions, we only do these checks if they are constant, i.e., do not contain any parameter.
     * @param tolerance The tolerance used when comparing values to zero and one.
     * @param reason if the matrix is not probabilistic, this string will be filled with a human-readable explanation of why the matrix is not probabilistic.
     */
    bool isProbabilistic(ValueType const& tolerance, storm::OptionalRef<std::string> reason = {}) const;

    /*!
     * Checks whether each present entry is strictly positive (omitted entries are not considered).
     */
    bool hasOnlyPositiveEntries() const;

    /*!
     * Checks if the current matrix is a submatrix of the given matrix, where a matrix A is called a submatrix
     * of B if B has no entries in position where A has none. Additionally, the matrices must be of equal size.
     *
     * @param matrix The matrix that possibly is a supermatrix of the current matrix.
     * @return True iff the current matrix is a submatrix of the given matrix.
     */
    template<typename OtherValueType>
    bool isSubmatrixOf(SparseMatrix<OtherValueType> const& matrix) const;

    // Returns true if the matrix is the identity matrix
    bool isIdentityMatrix() const;

    template<typename TPrime>
    friend std::ostream& operator<<(std::ostream& out, SparseMatrix<TPrime> const& matrix);

    /*!
     * Returns a string describing the dimensions of the matrix.
     */
    std::string getDimensionsAsString() const;

    /*!
     * Prints the matrix in a dense format, as also used by e.g. Matlab.
     * Notice that the format does not support multiple rows in a rowgroup.
     *
     * @out The stream to output to.
     */
    void printAsMatlabMatrix(std::ostream& out) const;

    /*!
     * Calculates a hash value over all values contained in the matrix.
     *
     * @return size_t A hash value for this matrix.
     */
    std::size_t hash() const;

    /*!
     * Returns an object representing the consecutive rows given by the parameters.
     *
     * @param startRow The starting row.
     * @param endRow The ending row (which is *not* included in the result).
     * @return An object representing the consecutive rows given by the parameters.
     */
    const_rows getRows(index_type startRow, index_type endRow) const;

    /*!
     * Returns an object representing the consecutive rows given by the parameters.
     *
     * @param startRow The starting row.
     * @param endRow The ending row (which is *not* included in the result).
     * @return An object representing the consecutive rows given by the parameters.
     */
    rows getRows(index_type startRow, index_type endRow);

    /*!
     * Returns an object representing the given row.
     *
     * @param row The row to get.
     * @return An object representing the given row.
     */
    const_rows getRow(index_type row) const;

    /*!
     * Returns an object representing the given row.
     *
     * @param row The row to get.
     * @return An object representing the given row.
     */
    rows getRow(index_type row);

    /*!
     * Returns an object representing the offset'th row in the rowgroup
     * @param rowGroup the row group
     * @param offset which row in the group
     * @return An object representing the given row.
     */
    const_rows getRow(index_type rowGroup, index_type offset) const;

    /*!
     * Returns an object representing the offset'th row in the rowgroup
     * @param rowGroup the row group
     * @param offset which row in the group
     * @return An object representing the given row.
     */
    rows getRow(index_type rowGroup, index_type offset);

    /*!
     * Returns an object representing the given row group.
     *
     * @param rowGroup The row group to get.
     * @return An object representing the given row group.
     */
    const_rows getRowGroup(index_type rowGroup) const;

    /*!
     * Returns an object representing the given row group.
     *
     * @param rowGroup The row group to get.
     * @return An object representing the given row group.
     */
    rows getRowGroup(index_type rowGroup);

    /*!
     * Retrieves an iterator that points to the beginning of the given row.
     *
     * @param row The row to the beginning of which the iterator has to point.
     * @return An iterator that points to the beginning of the given row.
     */
    const_iterator begin(index_type row) const;

    /*!
     * Retrieves an iterator that points to the beginning of the given row.
     *
     * @param row The row to the beginning of which the iterator has to point.
     * @return An iterator that points to the beginning of the given row.
     */
    iterator begin(index_type row);

    /*!
     * Retrieves an iterator that points to the beginning of the first row of the matrix.
     *
     * @return An iterator that points to the beginning of the first row of the matrix.
     */
    const_iterator begin() const;

    /*!
     * Retrieves an iterator that points to the beginning of the first row of the matrix.
     *
     * @return An iterator that points to the beginning of the first row of the matrix.
     */
    iterator begin();

    /*!
     * Retrieves an iterator that points past the end of the given row.
     *
     * @param row The row past the end of which the iterator has to point.
     * @return An iterator that points past the end of the given row.
     */
    const_iterator end(index_type row) const;

    /*!
     * Retrieves an iterator that points past the end of the given row.
     *
     * @param row The row past the end of which the iterator has to point.
     * @return An iterator that points past the end of the given row.
     */
    iterator end(index_type row);

    /*!
     * Retrieves an iterator that points past the end of the last row of the matrix.
     *
     * @return An iterator that points past the end of the last row of the matrix.
     */
    const_iterator end() const;

    /*!
     * Retrieves an iterator that points past the end of the last row of the matrix.
     *
     * @return An iterator that points past the end of the last row of the matrix.
     */
    iterator end();

    /*!
     * Returns a copy of the matrix with the chosen internal data type
     */
    template<typename NewValueType>
    SparseMatrix<NewValueType> toValueType() const {
        std::vector<MatrixEntry<SparseMatrix::index_type, NewValueType>> newColumnsAndValues;
        std::vector<SparseMatrix::index_type> newRowIndications(rowIndications);
        boost::optional<std::vector<SparseMatrix::index_type>> newRowGroupIndices(rowGroupIndices);

        newColumnsAndValues.resize(columnsAndValues.size());
        std::transform(
            columnsAndValues.begin(), columnsAndValues.end(), newColumnsAndValues.begin(), [](MatrixEntry<SparseMatrix::index_type, ValueType> const& a) {
                return MatrixEntry<SparseMatrix::index_type, NewValueType>(a.getColumn(), storm::utility::convertNumber<NewValueType, ValueType>(a.getValue()));
            });

        return SparseMatrix<NewValueType>(columnCount, std::move(newRowIndications), std::move(newColumnsAndValues), std::move(newRowGroupIndices));
    }

   private:
    /*!
     * Creates a submatrix of the current matrix by keeping only row groups and columns in the given row group
     * and column constraint, respectively.
     *
     * @param rowGroupConstraint A bit vector indicating which row groups to keep.
     * @param columnConstraint A bit vector indicating which columns to keep.
     * @param rowGroupIndices A vector indicating which rows belong to a given row group.
     * @param insertDiagonalEntries If set to true, the resulting matrix will have zero entries in column i for
     * each row in row group i. This can then be used for inserting other values later.
     * @return A matrix corresponding to a submatrix of the current matrix in which only row groups and columns
     * given by the row group constraint are kept and all others are dropped.
     */
    SparseMatrix getSubmatrix(storm::storage::BitVector const& rowGroupConstraint, storm::storage::BitVector const& columnConstraint,
                              std::vector<index_type> const& rowGroupIndices, bool insertDiagonalEntries = false,
                              storm::storage::BitVector const& makeZeroColumns = storm::storage::BitVector()) const;

    // The number of rows of the matrix.
    index_type rowCount;

    // The number of columns of the matrix.
    mutable index_type columnCount;

    // The number of entries in the matrix.
    index_type entryCount;

    // The number of nonzero entries in the matrix.
    mutable index_type nonzeroEntryCount;

    // The storage for the columns and values of all entries in the matrix.
    std::vector<MatrixEntry<index_type, value_type>> columnsAndValues;

    // A vector containing the indices at which each given row begins. This index is to be interpreted as an
    // index in the valueStorage and the columnIndications vectors. Put differently, the values of the entries
    // in row i are valueStorage[rowIndications[i]] to valueStorage[rowIndications[i + 1]] where the last
    // entry is not included anymore.
    std::vector<index_type> rowIndications;

    // A flag indicating whether the matrix has a trivial row grouping. Note that this may be true and yet
    // there may be row group indices, because they were requested from the outside.
    bool trivialRowGrouping;

    // A vector indicating the row groups of the matrix. This needs to be mutible in case we create it on-the-fly.
    mutable boost::optional<std::vector<index_type>> rowGroupIndices;
};

}  // namespace storage
}  // namespace storm
