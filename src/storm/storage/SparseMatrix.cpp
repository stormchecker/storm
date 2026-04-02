#include "storm/storage/SparseMatrix.h"

#include <iterator>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/sparse/StateType.h"
#include "storm/utility/ConstantsComparator.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/permutation.h"
#include "storm/utility/vector.h"

namespace storm {
namespace storage {

template<typename IndexType, typename ValueType>
MatrixEntry<IndexType, ValueType>::MatrixEntry(IndexType column, ValueType value) : entry(column, value) {
    // Intentionally left empty.
}

template<typename IndexType, typename ValueType>
MatrixEntry<IndexType, ValueType>::MatrixEntry(std::pair<IndexType, ValueType>&& pair) : entry(std::move(pair)) {
    // Intentionally left empty.
}

template<typename IndexType, typename ValueType>
IndexType const& MatrixEntry<IndexType, ValueType>::getColumn() const {
    return this->entry.first;
}

template<typename IndexType, typename ValueType>
void MatrixEntry<IndexType, ValueType>::setColumn(IndexType const& column) {
    this->entry.first = column;
}

template<typename IndexType, typename ValueType>
ValueType const& MatrixEntry<IndexType, ValueType>::getValue() const {
    return this->entry.second;
}

template<typename IndexType, typename ValueType>
void MatrixEntry<IndexType, ValueType>::setValue(ValueType const& value) {
    this->entry.second = value;
}

template<typename IndexType, typename ValueType>
std::pair<IndexType, ValueType> const& MatrixEntry<IndexType, ValueType>::getColumnValuePair() const {
    return this->entry;
}

template<typename IndexType, typename ValueType>
MatrixEntry<IndexType, ValueType> MatrixEntry<IndexType, ValueType>::operator*(value_type factor) const {
    return MatrixEntry(this->getColumn(), this->getValue() * factor);
}

template<typename IndexType, typename ValueType>
bool MatrixEntry<IndexType, ValueType>::operator==(MatrixEntry<IndexType, ValueType> const& other) const {
    return this->entry.first == other.entry.first && this->entry.second == other.entry.second;
}

template<typename IndexType, typename ValueType>
bool MatrixEntry<IndexType, ValueType>::operator!=(MatrixEntry<IndexType, ValueType> const& other) const {
    return !(*this == other);
}

template<typename IndexTypePrime, typename ValueTypePrime>
std::ostream& operator<<(std::ostream& out, MatrixEntry<IndexTypePrime, ValueTypePrime> const& entry) {
    out << "(" << entry.getColumn() << ", " << entry.getValue() << ")";
    return out;
}

template<typename ValueType>
SparseMatrixBuilder<ValueType>::SparseMatrixBuilder(index_type rows, index_type columns, index_type entries, bool forceDimensions, bool hasCustomRowGrouping,
                                                    index_type rowGroups)
    : initialRowCountSet(rows != 0),
      initialRowCount(rows),
      initialColumnCountSet(columns != 0),
      initialColumnCount(columns),
      initialEntryCountSet(entries != 0),
      initialEntryCount(entries),
      forceInitialDimensions(forceDimensions),
      hasCustomRowGrouping(hasCustomRowGrouping),
      initialRowGroupCountSet(rowGroups != 0),
      initialRowGroupCount(rowGroups),
      rowGroupIndices(),
      columnsAndValues(),
      rowIndications(),
      currentEntryCount(0),
      lastRow(0),
      lastColumn(0),
      highestColumn(0),
      currentRowGroupCount(0) {
    // Prepare the internal storage.
    if (initialRowCountSet) {
        rowIndications.reserve(initialRowCount + 1);
    }
    if (initialEntryCountSet) {
        columnsAndValues.reserve(initialEntryCount);
    }
    if (hasCustomRowGrouping) {
        rowGroupIndices = std::vector<index_type>();
    }
    if (initialRowGroupCountSet && hasCustomRowGrouping) {
        rowGroupIndices.get().reserve(initialRowGroupCount + 1);
    }
    rowIndications.push_back(0);
}

template<typename ValueType>
SparseMatrixBuilder<ValueType>::SparseMatrixBuilder(SparseMatrix<ValueType>&& matrix)
    : initialRowCountSet(false),
      initialRowCount(0),
      initialColumnCountSet(false),
      initialColumnCount(0),
      initialEntryCountSet(false),
      initialEntryCount(0),
      forceInitialDimensions(false),
      hasCustomRowGrouping(!matrix.trivialRowGrouping),
      initialRowGroupCountSet(false),
      initialRowGroupCount(0),
      rowGroupIndices(),
      columnsAndValues(std::move(matrix.columnsAndValues)),
      rowIndications(std::move(matrix.rowIndications)),
      currentEntryCount(matrix.entryCount),
      currentRowGroupCount() {
    lastRow = matrix.rowCount == 0 ? 0 : matrix.rowCount - 1;
    lastColumn = columnsAndValues.empty() ? 0 : columnsAndValues.back().getColumn();
    highestColumn = matrix.getColumnCount() == 0 ? 0 : matrix.getColumnCount() - 1;

    // If the matrix has a custom row grouping, we move it and remove the last element to make it 'open' again.
    if (hasCustomRowGrouping) {
        rowGroupIndices = std::move(matrix.rowGroupIndices);
        if (!rowGroupIndices->empty()) {
            rowGroupIndices.get().pop_back();
        }
        currentRowGroupCount = rowGroupIndices->empty() ? 0 : rowGroupIndices.get().size() - 1;
    }

    // Likewise, we need to 'open' the row indications again.
    if (!rowIndications.empty()) {
        rowIndications.pop_back();
    }
}

template<typename ValueType>
void SparseMatrixBuilder<ValueType>::addNextValue(index_type row, index_type column, ValueType const& value) {
    // Check that we did not move backwards wrt. the row.
    STORM_LOG_THROW(row >= lastRow, storm::exceptions::InvalidArgumentException,
                    "Adding an element in row " << row << ", but an element in row " << lastRow << " has already been added.");
    STORM_LOG_ASSERT(columnsAndValues.size() == currentEntryCount, "Unexpected size of columnsAndValues vector.");

    // Check if a diagonal entry shall be inserted before
    if (pendingDiagonalEntry) {
        index_type diagColumn = hasCustomRowGrouping ? currentRowGroupCount - 1 : lastRow;
        if (row > lastRow || column >= diagColumn) {
            ValueType diagValue = std::move(pendingDiagonalEntry.get());
            pendingDiagonalEntry = boost::none;
            // Add the pending diagonal value now
            if (row == lastRow && column == diagColumn) {
                // The currently added value coincides with the diagonal entry!
                // We add up the values and repeat this call.
                addNextValue(row, column, diagValue + value);
                // We return here because the above call already did all the work.
                return;
            } else {
                addNextValue(lastRow, diagColumn, diagValue);
            }
        }
    }

    // If the element is in the same row, but was not inserted in the correct order, we need to fix the row after
    // the insertion.
    bool fixCurrentRow = row == lastRow && column < lastColumn;
    // If the element is in the same row and column as the previous entry, we add them up...
    // unless there is no entry in this row yet, which might happen either for the very first entry or when only a diagonal value has been added
    if (row == lastRow && column == lastColumn && rowIndications.back() < currentEntryCount) {
        columnsAndValues.back().setValue(columnsAndValues.back().getValue() + value);
    } else {
        // If we switched to another row, we have to adjust the missing entries in the row indices vector.
        if (row != lastRow) {
            // Otherwise, we need to push the correct values to the vectors, which might trigger reallocations.
            assert(rowIndications.size() == lastRow + 1);
            rowIndications.resize(row + 1, currentEntryCount);
            lastRow = row;
        }

        lastColumn = column;

        // Finally, set the element and increase the current size.
        columnsAndValues.emplace_back(column, value);
        highestColumn = std::max(highestColumn, column);
        ++currentEntryCount;

        // If we need to fix the row, do so now.
        if (fixCurrentRow) {
            // TODO we fix this row directly after the out-of-order insertion, but the code does not exploit that fact.
            STORM_LOG_TRACE("Fix row " << row << " as column " << column << " is added out-of-order.");
            // First, we sort according to columns.
            std::sort(columnsAndValues.begin() + rowIndications.back(), columnsAndValues.end(),
                      [](storm::storage::MatrixEntry<index_type, ValueType> const& a, storm::storage::MatrixEntry<index_type, ValueType> const& b) {
                          return a.getColumn() < b.getColumn();
                      });

            auto insertIt = columnsAndValues.begin() + rowIndications.back();
            uint64_t elementsToRemove = 0;
            for (auto it = insertIt + 1; it != columnsAndValues.end(); ++it) {
                // Iterate over all entries in this last row and detect duplicates.
                if (it->getColumn() == insertIt->getColumn()) {
                    // This entry is a duplicate of the column. Update the previous entry.
                    insertIt->setValue(insertIt->getValue() + it->getValue());
                    elementsToRemove++;
                } else {
                    insertIt = it;
                }
            }
            // Then, we eliminate those duplicate entries.
            // We cast the result of std::unique to void to silence a warning issued due to a [[nodiscard]] attribute
            static_cast<void>(std::unique(columnsAndValues.begin() + rowIndications.back(), columnsAndValues.end(),
                                          [](storm::storage::MatrixEntry<index_type, ValueType> const& a,
                                             storm::storage::MatrixEntry<index_type, ValueType> const& b) { return a.getColumn() == b.getColumn(); }));

            if (elementsToRemove > 0) {
                STORM_LOG_WARN("Unordered insertion into matrix builder caused duplicate entries.");
                currentEntryCount -= elementsToRemove;
                columnsAndValues.resize(columnsAndValues.size() - elementsToRemove);
            }
            lastColumn = columnsAndValues.back().getColumn();
        }
    }

    // In case we did not expect this value, we throw an exception.
    if (forceInitialDimensions) {
        STORM_LOG_THROW(!initialRowCountSet || lastRow < initialRowCount, storm::exceptions::OutOfRangeException,
                        "Cannot insert value at illegal row " << lastRow << ".");
        STORM_LOG_THROW(!initialColumnCountSet || lastColumn < initialColumnCount, storm::exceptions::OutOfRangeException,
                        "Cannot insert value at illegal column " << lastColumn << ".");
        STORM_LOG_THROW(!initialEntryCountSet || currentEntryCount <= initialEntryCount, storm::exceptions::OutOfRangeException,
                        "Too many entries in matrix, expected only " << initialEntryCount << ".");
    }
}

template<typename ValueType>
void SparseMatrixBuilder<ValueType>::newRowGroup(index_type startingRow) {
    STORM_LOG_THROW(hasCustomRowGrouping, storm::exceptions::InvalidStateException, "Matrix was not created to have a custom row grouping.");
    STORM_LOG_THROW(startingRow >= lastRow, storm::exceptions::InvalidStateException, "Illegal row group with negative size.");

    // If there still is a pending diagonal entry, we need to add it now (otherwise, the correct diagonal column will be unclear)
    if (pendingDiagonalEntry) {
        STORM_LOG_ASSERT(currentRowGroupCount > 0, "Diagonal entry was set before opening the first row group.");
        index_type diagColumn = currentRowGroupCount - 1;
        ValueType diagValue = std::move(pendingDiagonalEntry.get());
        pendingDiagonalEntry = boost::none;  // clear now, so addNextValue works properly
        addNextValue(lastRow, diagColumn, diagValue);
    }

    rowGroupIndices.get().push_back(startingRow);
    ++currentRowGroupCount;

    // Handle the case where the previous row group ends with one or more empty rows
    if (lastRow + 1 < startingRow) {
        // Close all rows from the most recent one to the starting row.
        assert(rowIndications.size() == lastRow + 1);
        rowIndications.resize(startingRow, currentEntryCount);
        // Reset the most recently seen row/column to allow for proper insertion of the following elements.
        lastRow = startingRow - 1;
        lastColumn = 0;
    }
}

template<typename ValueType>
SparseMatrix<ValueType> SparseMatrixBuilder<ValueType>::build(index_type overriddenRowCount, index_type overriddenColumnCount,
                                                              index_type overriddenRowGroupCount) {
    // If there still is a pending diagonal entry, we need to add it now
    if (pendingDiagonalEntry) {
        index_type diagColumn = hasCustomRowGrouping ? currentRowGroupCount - 1 : lastRow;
        ValueType diagValue = std::move(pendingDiagonalEntry.get());
        pendingDiagonalEntry = boost::none;  // clear now, so addNextValue works properly
        addNextValue(lastRow, diagColumn, diagValue);
    }

    bool hasEntries = currentEntryCount != 0;

    uint_fast64_t rowCount = hasEntries ? lastRow + 1 : 0;

    // If the last row group was empty, we need to add one more to the row count, because otherwise this empty row is not counted.
    if (hasCustomRowGrouping) {
        if (lastRow < rowGroupIndices->back()) {
            ++rowCount;
        }
    }

    if (initialRowCountSet && forceInitialDimensions) {
        STORM_LOG_THROW(rowCount <= initialRowCount, storm::exceptions::InvalidStateException,
                        "Expected not more than " << initialRowCount << " rows, but got " << rowCount << ".");
        rowCount = std::max(rowCount, initialRowCount);
    }

    rowCount = std::max(rowCount, overriddenRowCount);

    // If the current row count was overridden, we may need to add empty rows.
    for (index_type i = lastRow + 1; i < rowCount; ++i) {
        rowIndications.push_back(currentEntryCount);
    }

    // We put a sentinel element at the last position of the row indices array. This eases iteration work,
    // as now the indices of row i are always between rowIndications[i] and rowIndications[i + 1], also for
    // the first and last row.
    if (rowCount > 0) {
        rowIndications.push_back(currentEntryCount);
    }
    STORM_LOG_ASSERT(rowCount == rowIndications.size() - 1,
                     "Wrong sizes of row indications vector: (Rowcount) " << rowCount << " != " << (rowIndications.size() - 1) << " (Vector).");
    uint_fast64_t columnCount = hasEntries ? highestColumn + 1 : 0;
    if (initialColumnCountSet && forceInitialDimensions) {
        STORM_LOG_THROW(columnCount <= initialColumnCount, storm::exceptions::InvalidStateException,
                        "Expected not more than " << initialColumnCount << " columns, but got " << columnCount << ".");
        columnCount = std::max(columnCount, initialColumnCount);
    }
    columnCount = std::max(columnCount, overriddenColumnCount);

    uint_fast64_t entryCount = currentEntryCount;
    if (initialEntryCountSet && forceInitialDimensions) {
        STORM_LOG_THROW(entryCount == initialEntryCount, storm::exceptions::InvalidStateException,
                        "Expected " << initialEntryCount << " entries, but got " << entryCount << ".");
    }

    // Check whether row groups are missing some entries.
    if (hasCustomRowGrouping) {
        uint_fast64_t rowGroupCount = currentRowGroupCount;
        if (initialRowGroupCountSet && forceInitialDimensions) {
            STORM_LOG_THROW(rowGroupCount <= initialRowGroupCount, storm::exceptions::InvalidStateException,
                            "Expected not more than " << initialRowGroupCount << " row groups, but got " << rowGroupCount << ".");
            rowGroupCount = std::max(rowGroupCount, initialRowGroupCount);
        }
        rowGroupCount = std::max(rowGroupCount, overriddenRowGroupCount);

        for (index_type i = currentRowGroupCount; i <= rowGroupCount; ++i) {
            rowGroupIndices.get().push_back(rowCount);
        }
    }

    return SparseMatrix<ValueType>(columnCount, std::move(rowIndications), std::move(columnsAndValues), std::move(rowGroupIndices));
}

template<typename ValueType>
typename SparseMatrixBuilder<ValueType>::index_type SparseMatrixBuilder<ValueType>::getLastRow() const {
    return lastRow;
}

template<typename ValueType>
typename SparseMatrixBuilder<ValueType>::index_type SparseMatrixBuilder<ValueType>::getCurrentRowGroupCount() const {
    if (this->hasCustomRowGrouping) {
        return currentRowGroupCount;
    } else {
        return getLastRow() + 1;
    }
}

template<typename ValueType>
typename SparseMatrixBuilder<ValueType>::index_type SparseMatrixBuilder<ValueType>::getLastColumn() const {
    return lastColumn;
}

// Debug method for printing the current matrix
template<typename ValueType>
void print(std::vector<typename SparseMatrix<ValueType>::index_type> const& rowGroupIndices,
           std::vector<MatrixEntry<typename SparseMatrix<ValueType>::index_type, typename SparseMatrix<ValueType>::value_type>> const& columnsAndValues,
           std::vector<typename SparseMatrix<ValueType>::index_type> const& rowIndications) {
    typename SparseMatrix<ValueType>::index_type endGroups;
    typename SparseMatrix<ValueType>::index_type endRows;
    // Iterate over all row groups.
    for (typename SparseMatrix<ValueType>::index_type group = 0; group < rowGroupIndices.size(); ++group) {
        std::cout << "\t---- group " << group << "/" << (rowGroupIndices.size() - 1) << " ---- \n";
        endGroups = group < rowGroupIndices.size() - 1 ? rowGroupIndices[group + 1] : rowIndications.size();
        // Iterate over all rows in a row group
        for (typename SparseMatrix<ValueType>::index_type i = rowGroupIndices[group]; i < endGroups; ++i) {
            endRows = i < rowIndications.size() - 1 ? rowIndications[i + 1] : columnsAndValues.size();
            // Print the actual row.
            std::cout << "Row " << i << " (" << rowIndications[i] << " - " << endRows << ")" << ": ";
            for (typename SparseMatrix<ValueType>::index_type pos = rowIndications[i]; pos < endRows; ++pos) {
                std::cout << "(" << columnsAndValues[pos].getColumn() << ": " << columnsAndValues[pos].getValue() << ") ";
            }
            std::cout << '\n';
        }
    }
}

template<typename ValueType>
void SparseMatrixBuilder<ValueType>::replaceColumns(std::vector<index_type> const& replacements, index_type offset) {
    index_type maxColumn = 0;

    for (index_type row = 0; row < rowIndications.size(); ++row) {
        bool changed = false;
        auto startRow = std::next(columnsAndValues.begin(), rowIndications[row]);
        auto endRow = row < rowIndications.size() - 1 ? std::next(columnsAndValues.begin(), rowIndications[row + 1]) : columnsAndValues.end();
        for (auto entry = startRow; entry != endRow; ++entry) {
            if (entry->getColumn() >= offset) {
                // Change column
                entry->setColumn(replacements[entry->getColumn() - offset]);
                changed = true;
            }
            maxColumn = std::max(maxColumn, entry->getColumn());
        }
        if (changed) {
            // Sort columns in row
            std::sort(startRow, endRow,
                      [](MatrixEntry<index_type, value_type> const& a, MatrixEntry<index_type, value_type> const& b) { return a.getColumn() < b.getColumn(); });
            // Assert no equal elements
            STORM_LOG_ASSERT(std::is_sorted(startRow, endRow,
                                            [](MatrixEntry<index_type, value_type> const& a, MatrixEntry<index_type, value_type> const& b) {
                                                return a.getColumn() < b.getColumn();
                                            }),
                             "Columns not sorted.");
        }
    }

    highestColumn = maxColumn;
    lastColumn = columnsAndValues.empty() ? 0 : columnsAndValues.back().getColumn();
}

template<typename ValueType>
void SparseMatrixBuilder<ValueType>::addDiagonalEntry(index_type row, ValueType const& value) {
    STORM_LOG_THROW(row >= lastRow, storm::exceptions::InvalidArgumentException,
                    "Adding a diagonal element in row " << row << ", but an element in row " << lastRow << " has already been added.");
    if (pendingDiagonalEntry) {
        if (row == lastRow) {
            // Add the two diagonal entries, nothing else to be done.
            pendingDiagonalEntry.get() += value;
            return;
        } else {
            // add the pending entry
            index_type column = hasCustomRowGrouping ? currentRowGroupCount - 1 : lastRow;
            ValueType diagValue = std::move(pendingDiagonalEntry.get());
            pendingDiagonalEntry = boost::none;  // clear now, so addNextValue works properly
            addNextValue(lastRow, column, diagValue);
        }
    }
    pendingDiagonalEntry = value;
    if (lastRow != row) {
        assert(rowIndications.size() == lastRow + 1);
        rowIndications.resize(row + 1, currentEntryCount);
        lastRow = row;
        lastColumn = 0;
    }
}

template<typename ValueType>
SparseMatrix<ValueType>::rows::rows(iterator begin, index_type entryCount) : beginIterator(begin), entryCount(entryCount) {
    // Intentionally left empty.
}

template<typename ValueType>
typename SparseMatrix<ValueType>::iterator SparseMatrix<ValueType>::rows::begin() {
    return beginIterator;
}

template<typename ValueType>
typename SparseMatrix<ValueType>::iterator SparseMatrix<ValueType>::rows::end() {
    return beginIterator + entryCount;
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::rows::getNumberOfEntries() const {
    return this->entryCount;
}

template<typename ValueType>
SparseMatrix<ValueType>::const_rows::const_rows(const_iterator begin, index_type entryCount) : beginIterator(begin), entryCount(entryCount) {
    // Intentionally left empty.
}

template<typename ValueType>
typename SparseMatrix<ValueType>::const_iterator SparseMatrix<ValueType>::const_rows::begin() const {
    return beginIterator;
}

template<typename ValueType>
typename SparseMatrix<ValueType>::const_iterator SparseMatrix<ValueType>::const_rows::end() const {
    return beginIterator + entryCount;
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::const_rows::getNumberOfEntries() const {
    return this->entryCount;
}

template<typename ValueType>
SparseMatrix<ValueType>::SparseMatrix()
    : rowCount(0), columnCount(0), entryCount(0), nonzeroEntryCount(0), columnsAndValues(), rowIndications(), rowGroupIndices() {
    // Intentionally left empty.
}

template<typename ValueType>
SparseMatrix<ValueType>::SparseMatrix(SparseMatrix<ValueType> const& other)
    : rowCount(other.rowCount),
      columnCount(other.columnCount),
      entryCount(other.entryCount),
      nonzeroEntryCount(other.nonzeroEntryCount),
      columnsAndValues(other.columnsAndValues),
      rowIndications(other.rowIndications),
      trivialRowGrouping(other.trivialRowGrouping),
      rowGroupIndices(other.rowGroupIndices) {
    // Intentionally left empty.
}

template<typename ValueType>
SparseMatrix<ValueType>::SparseMatrix(SparseMatrix<value_type> const& other, bool insertDiagonalElements) {
    storm::storage::BitVector rowConstraint(other.getRowCount(), true);
    storm::storage::BitVector columnConstraint(other.getColumnCount(), true);
    *this = other.getSubmatrix(false, rowConstraint, columnConstraint, insertDiagonalElements);
}

template<typename ValueType>
SparseMatrix<ValueType>::SparseMatrix(SparseMatrix<ValueType>&& other)
    : rowCount(other.rowCount),
      columnCount(other.columnCount),
      entryCount(other.entryCount),
      nonzeroEntryCount(other.nonzeroEntryCount),
      columnsAndValues(std::move(other.columnsAndValues)),
      rowIndications(std::move(other.rowIndications)),
      trivialRowGrouping(other.trivialRowGrouping),
      rowGroupIndices(std::move(other.rowGroupIndices)) {
    // Now update the source matrix
    other.rowCount = 0;
    other.columnCount = 0;
    other.entryCount = 0;
}

template<typename ValueType>
SparseMatrix<ValueType>::SparseMatrix(index_type columnCount, std::vector<index_type> const& rowIndications,
                                      std::vector<MatrixEntry<index_type, ValueType>> const& columnsAndValues,
                                      boost::optional<std::vector<index_type>> const& rowGroupIndices)
    : rowCount(rowIndications.size() - 1),
      columnCount(columnCount),
      entryCount(columnsAndValues.size()),
      nonzeroEntryCount(0),
      columnsAndValues(columnsAndValues),
      rowIndications(rowIndications),
      trivialRowGrouping(!rowGroupIndices),
      rowGroupIndices(rowGroupIndices) {
    this->updateNonzeroEntryCount();
}

template<typename ValueType>
SparseMatrix<ValueType>::SparseMatrix(index_type columnCount, std::vector<index_type>&& rowIndications,
                                      std::vector<MatrixEntry<index_type, ValueType>>&& columnsAndValues,
                                      boost::optional<std::vector<index_type>>&& rowGroupIndices)
    : columnCount(columnCount),
      nonzeroEntryCount(0),
      columnsAndValues(std::move(columnsAndValues)),
      rowIndications(std::move(rowIndications)),
      rowGroupIndices(std::move(rowGroupIndices)) {
    // Initialize some variables here which depend on other variables
    // This way we are more robust against different initialization orders
    this->rowCount = this->rowIndications.size() - 1;
    this->entryCount = this->columnsAndValues.size();
    this->trivialRowGrouping = !this->rowGroupIndices;
    this->updateNonzeroEntryCount();
}

template<typename ValueType>
SparseMatrix<ValueType>& SparseMatrix<ValueType>::operator=(SparseMatrix<ValueType> const& other) {
    // Only perform assignment if source and target are not the same.
    if (this != &other) {
        rowCount = other.rowCount;
        columnCount = other.columnCount;
        entryCount = other.entryCount;
        nonzeroEntryCount = other.nonzeroEntryCount;

        columnsAndValues = other.columnsAndValues;
        rowIndications = other.rowIndications;
        rowGroupIndices = other.rowGroupIndices;
        trivialRowGrouping = other.trivialRowGrouping;
    }
    return *this;
}

template<typename ValueType>
SparseMatrix<ValueType>& SparseMatrix<ValueType>::operator=(SparseMatrix<ValueType>&& other) {
    // Only perform assignment if source and target are not the same.
    if (this != &other) {
        rowCount = other.rowCount;
        columnCount = other.columnCount;
        entryCount = other.entryCount;
        nonzeroEntryCount = other.nonzeroEntryCount;

        columnsAndValues = std::move(other.columnsAndValues);
        rowIndications = std::move(other.rowIndications);
        rowGroupIndices = std::move(other.rowGroupIndices);
        trivialRowGrouping = other.trivialRowGrouping;
    }
    return *this;
}

template<typename ValueType>
bool SparseMatrix<ValueType>::operator==(SparseMatrix<ValueType> const& other) const {
    if (this == &other) {
        return true;
    }

    bool equalityResult = true;

    equalityResult &= this->getRowCount() == other.getRowCount();
    if (!equalityResult) {
        return false;
    }
    equalityResult &= this->getColumnCount() == other.getColumnCount();
    if (!equalityResult) {
        return false;
    }
    if (!this->hasTrivialRowGrouping() && !other.hasTrivialRowGrouping()) {
        equalityResult &= this->getRowGroupIndices() == other.getRowGroupIndices();
    } else {
        equalityResult &= this->hasTrivialRowGrouping() && other.hasTrivialRowGrouping();
    }
    if (!equalityResult) {
        return false;
    }

    // For the actual contents, we need to do a little bit more work, because we want to ignore elements that
    // are set to zero, please they may be represented implicitly in the other matrix.
    for (index_type row = 0; row < this->getRowCount(); ++row) {
        for (const_iterator it1 = this->begin(row), ite1 = this->end(row), it2 = other.begin(row), ite2 = other.end(row); it1 != ite1 && it2 != ite2;
             ++it1, ++it2) {
            // Skip over all zero entries in both matrices.
            while (it1 != ite1 && storm::utility::isZero(it1->getValue())) {
                ++it1;
            }
            while (it2 != ite2 && storm::utility::isZero(it2->getValue())) {
                ++it2;
            }
            if ((it1 == ite1) || (it2 == ite2)) {
                equalityResult = (it1 == ite1) ^ (it2 == ite2);
                break;
            } else {
                if (it1->getColumn() != it2->getColumn() || it1->getValue() != it2->getValue()) {
                    equalityResult = false;
                    break;
                }
            }
        }
        if (!equalityResult) {
            return false;
        }
    }

    return equalityResult;
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::getRowCount() const {
    return rowCount;
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::getColumnCount() const {
    return columnCount;
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::getEntryCount() const {
    return entryCount;
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::getRowGroupEntryCount(index_type const group) const {
    if (!this->hasTrivialRowGrouping()) {
        index_type result = 0;
        for (auto row : this->getRowGroupIndices(group)) {
            result += (this->rowIndications[row + 1] - this->rowIndications[row]);
        }
        return result;
    } else {
        return (this->rowIndications[group + 1] - this->rowIndications[group]);
    }
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::getRowEntryCount(index_type const row) const {
    return (this->rowIndications[row + 1] - this->rowIndications[row]);
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::getNonzeroEntryCount() const {
    return nonzeroEntryCount;
}

template<typename ValueType>
void SparseMatrix<ValueType>::updateNonzeroEntryCount() const {
    this->nonzeroEntryCount = 0;
    for (auto const& element : *this) {
        if (element.getValue() != storm::utility::zero<ValueType>()) {
            ++this->nonzeroEntryCount;
        }
    }
}

template<typename ValueType>
void SparseMatrix<ValueType>::updateNonzeroEntryCount(std::make_signed<index_type>::type difference) {
    this->nonzeroEntryCount += difference;
}

template<typename ValueType>
void SparseMatrix<ValueType>::updateDimensions() const {
    this->nonzeroEntryCount = 0;
    this->columnCount = 0;
    for (auto const& element : *this) {
        if (element.getValue() != storm::utility::zero<ValueType>()) {
            ++this->nonzeroEntryCount;
            this->columnCount = std::max(element.getColumn() + 1, this->columnCount);
        }
    }
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::getRowGroupCount() const {
    if (!this->hasTrivialRowGrouping()) {
        return rowGroupIndices.get().size() - 1;
    } else {
        return rowCount;
    }
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::getRowGroupSize(index_type group) const {
    return this->getRowGroupIndices()[group + 1] - this->getRowGroupIndices()[group];
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::getSizeOfLargestRowGroup() const {
    if (this->hasTrivialRowGrouping()) {
        return 1;
    }
    index_type res = 0;
    index_type previousGroupStart = 0;
    for (auto const& i : rowGroupIndices.get()) {
        res = std::max(res, i - previousGroupStart);
        previousGroupStart = i;
    }
    return res;
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::getNumRowsInRowGroups(storm::storage::BitVector const& groupConstraint) const {
    if (this->hasTrivialRowGrouping()) {
        return groupConstraint.getNumberOfSetBits();
    }
    index_type numRows = 0;
    index_type rowGroupIndex = groupConstraint.getNextSetIndex(0);
    while (rowGroupIndex < this->getRowGroupCount()) {
        index_type start = this->getRowGroupIndices()[rowGroupIndex];
        rowGroupIndex = groupConstraint.getNextUnsetIndex(rowGroupIndex + 1);
        index_type end = this->getRowGroupIndices()[rowGroupIndex];
        // All rows with index in [start,end) are selected.
        numRows += end - start;
        rowGroupIndex = groupConstraint.getNextSetIndex(rowGroupIndex + 1);
    }
    return numRows;
}

template<typename ValueType>
std::vector<typename SparseMatrix<ValueType>::index_type> const& SparseMatrix<ValueType>::getRowGroupIndices() const {
    // If there is no current row grouping, we need to create it.
    if (!this->rowGroupIndices) {
        STORM_LOG_ASSERT(trivialRowGrouping, "Only trivial row-groupings can be constructed on-the-fly.");
        this->rowGroupIndices = storm::utility::vector::buildVectorForRange(static_cast<index_type>(0), this->getRowGroupCount() + 1);
    }
    return rowGroupIndices.get();
}

template<typename ValueType>
std::vector<typename SparseMatrix<ValueType>::index_type> const& SparseMatrix<ValueType>::getRowIndices() const {
    return rowIndications;
}

template<typename ValueType>
boost::integer_range<typename SparseMatrix<ValueType>::index_type> SparseMatrix<ValueType>::getRowGroupIndices(index_type group) const {
    STORM_LOG_ASSERT(group < this->getRowGroupCount(),
                     "Invalid row group index:" << group << ". Only " << this->getRowGroupCount() << " row groups available.");
    if (this->rowGroupIndices) {
        return boost::irange(rowGroupIndices.get()[group], rowGroupIndices.get()[group + 1]);
    } else {
        return boost::irange(group, group + 1);
    }
}

template<typename ValueType>
std::vector<typename SparseMatrix<ValueType>::index_type> SparseMatrix<ValueType>::swapRowGroupIndices(std::vector<index_type>&& newRowGrouping) {
    std::vector<index_type> result;
    if (this->rowGroupIndices) {
        result = std::move(rowGroupIndices.get());
        rowGroupIndices = std::move(newRowGrouping);
    }
    return result;
}

template<typename ValueType>
void SparseMatrix<ValueType>::setRowGroupIndices(std::vector<index_type> const& newRowGroupIndices) {
    trivialRowGrouping = false;
    rowGroupIndices = newRowGroupIndices;
}

template<typename ValueType>
bool SparseMatrix<ValueType>::hasTrivialRowGrouping() const {
    return trivialRowGrouping;
}

template<typename ValueType>
void SparseMatrix<ValueType>::makeRowGroupingTrivial() {
    if (trivialRowGrouping) {
        STORM_LOG_ASSERT(
            !rowGroupIndices || rowGroupIndices.get() == storm::utility::vector::buildVectorForRange(static_cast<index_type>(0), this->getRowGroupCount() + 1),
            "Row grouping is supposed to be trivial but actually it is not.");
    } else {
        trivialRowGrouping = true;
        rowGroupIndices = boost::none;
    }
}

template<typename ValueType>
storm::storage::BitVector SparseMatrix<ValueType>::getRowFilter(storm::storage::BitVector const& groupConstraint) const {
    storm::storage::BitVector res(this->getRowCount(), false);
    for (auto group : groupConstraint) {
        res.setMultiple(getRowGroupIndices()[group], getRowGroupSize(group));
    }
    return res;
}

template<typename ValueType>
storm::storage::BitVector SparseMatrix<ValueType>::getRowFilter(storm::storage::BitVector const& groupConstraint,
                                                                storm::storage::BitVector const& columnConstraint) const {
    storm::storage::BitVector result(this->getRowCount(), false);
    for (auto group : groupConstraint) {
        for (auto row : this->getRowGroupIndices(group)) {
            bool choiceSatisfiesColumnConstraint = true;
            for (auto const& entry : this->getRow(row)) {
                if (!columnConstraint.get(entry.getColumn())) {
                    choiceSatisfiesColumnConstraint = false;
                    break;
                }
            }
            if (choiceSatisfiesColumnConstraint) {
                result.set(row, true);
            }
        }
    }
    return result;
}

template<typename ValueType>
storm::storage::BitVector SparseMatrix<ValueType>::getRowGroupFilter(storm::storage::BitVector const& rowConstraint, bool setIfForAllRowsInGroup) const {
    STORM_LOG_ASSERT(!this->hasTrivialRowGrouping(), "Tried to get a row group filter but this matrix does not have row groups");
    storm::storage::BitVector result(this->getRowGroupCount(), false);
    auto const& groupIndices = this->getRowGroupIndices();
    if (setIfForAllRowsInGroup) {
        for (uint64_t group = 0; group < this->getRowGroupCount(); ++group) {
            if (rowConstraint.getNextUnsetIndex(groupIndices[group]) >= groupIndices[group + 1]) {
                // All rows within this group are set
                result.set(group, true);
            }
        }
    } else {
        for (uint64_t group = 0; group < this->getRowGroupCount(); ++group) {
            if (rowConstraint.getNextSetIndex(groupIndices[group]) < groupIndices[group + 1]) {
                // Some row is set
                result.set(group, true);
            }
        }
    }
    return result;
}

template<typename ValueType>
void SparseMatrix<ValueType>::makeRowsAbsorbing(storm::storage::BitVector const& rows, bool dropZeroEntries) {
    // First transform ALL rows without dropping zero entries, then drop zero entries once
    // This prevents iteration over the whole matrix every time an entry is set to zero.
    for (auto row : rows) {
        makeRowDirac(row, row, false);
    }
    if (dropZeroEntries) {
        this->dropZeroEntries();
    }
}

template<typename ValueType>
void SparseMatrix<ValueType>::makeRowGroupsAbsorbing(storm::storage::BitVector const& rowGroupConstraint, bool dropZeroEntries) {
    // First transform ALL rows without dropping zero entries, then drop zero entries once.
    // This prevents iteration over the whole matrix every time an entry is set to zero.
    if (!this->hasTrivialRowGrouping()) {
        for (auto rowGroup : rowGroupConstraint) {
            for (index_type row = this->getRowGroupIndices()[rowGroup]; row < this->getRowGroupIndices()[rowGroup + 1]; ++row) {
                makeRowDirac(row, rowGroup, false);
            }
        }
    } else {
        for (auto rowGroup : rowGroupConstraint) {
            makeRowDirac(rowGroup, rowGroup, false);
        }
    }
    if (dropZeroEntries) {
        this->dropZeroEntries();
    }
}

template<typename ValueType>
void SparseMatrix<ValueType>::makeRowDirac(index_type row, index_type column, bool dropZeroEntries) {
    iterator columnValuePtr = this->begin(row);
    iterator columnValuePtrEnd = this->end(row);

    // If the row has no elements in it, we cannot make it absorbing, because we would need to move all elements
    // in the vector of nonzeros otherwise.
    if (columnValuePtr >= columnValuePtrEnd) {
        throw storm::exceptions::InvalidStateException()
            << "Illegal call to SparseMatrix::makeRowDirac: cannot make row " << row << " absorbing, because there is no entry in this row.";
    }
    iterator lastColumnValuePtr = this->end(row) - 1;

    // If there is at least one entry in this row, we can set it to one, modify its column value to the
    // one given by the parameter and set all subsequent elements of this row to zero.
    // However, we want to preserve that column indices within a row are ascending, so we pick an entry that is close to the desired column index
    while (columnValuePtr->getColumn() < column && columnValuePtr != lastColumnValuePtr) {
        if (!storm::utility::isZero(columnValuePtr->getValue())) {
            --this->nonzeroEntryCount;
        }
        columnValuePtr->setValue(storm::utility::zero<ValueType>());
        ++columnValuePtr;
    }
    // At this point, we have found the first entry whose column is >= the desired column (or the last entry of the row, if no such column exist)
    if (storm::utility::isZero(columnValuePtr->getValue())) {
        ++this->nonzeroEntryCount;
    }
    columnValuePtr->setValue(storm::utility::one<ValueType>());
    columnValuePtr->setColumn(column);
    for (++columnValuePtr; columnValuePtr != columnValuePtrEnd; ++columnValuePtr) {
        if (!storm::utility::isZero(columnValuePtr->getValue())) {
            --this->nonzeroEntryCount;
        }
        columnValuePtr->setValue(storm::utility::zero<ValueType>());
    }
    if (dropZeroEntries) {
        this->dropZeroEntries();
    }
}

template<typename ValueType>
bool SparseMatrix<ValueType>::compareRows(index_type i1, index_type i2) const {
    const_iterator end1 = this->end(i1);
    const_iterator end2 = this->end(i2);
    const_iterator it1 = this->begin(i1);
    const_iterator it2 = this->begin(i2);
    for (; it1 != end1 && it2 != end2; ++it1, ++it2) {
        if (*it1 != *it2) {
            return false;
        }
    }
    if (it1 == end1 && it2 == end2) {
        return true;
    }
    return false;
}

template<typename ValueType>
BitVector SparseMatrix<ValueType>::duplicateRowsInRowgroups() const {
    BitVector bv(this->getRowCount());
    for (size_t rowgroup = 0; rowgroup < this->getRowGroupCount(); ++rowgroup) {
        for (size_t row1 = this->getRowGroupIndices().at(rowgroup); row1 < this->getRowGroupIndices().at(rowgroup + 1); ++row1) {
            for (size_t row2 = row1; row2 < this->getRowGroupIndices().at(rowgroup + 1); ++row2) {
                if (compareRows(row1, row2)) {
                    bv.set(row2);
                }
            }
        }
    }
    return bv;
}

template<typename ValueType>
void SparseMatrix<ValueType>::swapRows(index_type const& row1, index_type const& row2) {
    if (row1 == row2) {
        return;
    }

    // Get the index of the row that has more / less entries than the other.
    index_type largerRow = getRow(row1).getNumberOfEntries() > getRow(row2).getNumberOfEntries() ? row1 : row2;
    index_type smallerRow = largerRow == row1 ? row2 : row1;
    index_type rowSizeDifference = getRow(largerRow).getNumberOfEntries() - getRow(smallerRow).getNumberOfEntries();

    // Save contents of larger row.
    auto copyRow = getRow(largerRow);
    std::vector<MatrixEntry<index_type, value_type>> largerRowContents(copyRow.begin(), copyRow.end());

    if (largerRow < smallerRow) {
        auto writeIt = getRows(largerRow, smallerRow + 1).begin();

        // Write smaller row to its new position.
        for (auto& smallerRowEntry : getRow(smallerRow)) {
            *writeIt = std::move(smallerRowEntry);
            ++writeIt;
        }

        // Write the intermediate rows into their correct position.
        if (!storm::utility::isZero(rowSizeDifference)) {
            for (auto& intermediateRowEntry : getRows(largerRow + 1, smallerRow)) {
                *writeIt = std::move(intermediateRowEntry);
                ++writeIt;
            }
        } else {
            // skip the intermediate rows
            writeIt = getRow(smallerRow).begin();
        }

        // Write the larger row to its new position.
        for (auto& largerRowEntry : largerRowContents) {
            *writeIt = std::move(largerRowEntry);
            ++writeIt;
        }

        STORM_LOG_ASSERT(writeIt == getRow(smallerRow).end(), "Unexpected position of write iterator.");

        // Update the row indications to account for the shift of indices at where the rows now start.
        if (!storm::utility::isZero(rowSizeDifference)) {
            for (index_type row = largerRow + 1; row <= smallerRow; ++row) {
                rowIndications[row] -= rowSizeDifference;
            }
        }
    } else {
        auto writeIt = getRows(smallerRow, largerRow + 1).end() - 1;

        // Write smaller row to its new position
        auto copyRow = getRow(smallerRow);
        for (auto smallerRowEntryIt = copyRow.end() - 1; smallerRowEntryIt != copyRow.begin() - 1; --smallerRowEntryIt) {
            *writeIt = std::move(*smallerRowEntryIt);
            --writeIt;
        }

        // Write the intermediate rows into their correct position.
        if (!storm::utility::isZero(rowSizeDifference)) {
            for (auto intermediateRowEntryIt = getRows(smallerRow + 1, largerRow).end() - 1;
                 intermediateRowEntryIt != getRows(smallerRow + 1, largerRow).begin() - 1; --intermediateRowEntryIt) {
                *writeIt = std::move(*intermediateRowEntryIt);
                --writeIt;
            }
        } else {
            // skip the intermediate rows
            writeIt = getRow(smallerRow).end() - 1;
        }

        // Write the larger row to its new position.
        for (auto largerRowEntryIt = largerRowContents.rbegin(); largerRowEntryIt != largerRowContents.rend(); ++largerRowEntryIt) {
            *writeIt = std::move(*largerRowEntryIt);
            --writeIt;
        }

        STORM_LOG_ASSERT(writeIt == getRow(smallerRow).begin() - 1, "Unexpected position of write iterator.");

        // Update row indications.
        // Update the row indications to account for the shift of indices at where the rows now start.
        if (!storm::utility::isZero(rowSizeDifference)) {
            for (index_type row = smallerRow + 1; row <= largerRow; ++row) {
                rowIndications[row] += rowSizeDifference;
            }
        }
    }
}

template<typename ValueType>
std::vector<ValueType> SparseMatrix<ValueType>::getRowSumVector() const {
    std::vector<ValueType> result(this->getRowCount());

    index_type row = 0;
    for (auto resultIt = result.begin(), resultIte = result.end(); resultIt != resultIte; ++resultIt, ++row) {
        *resultIt = getRowSum(row);
    }

    return result;
}

template<typename ValueType>
ValueType SparseMatrix<ValueType>::getConstrainedRowSum(index_type row, storm::storage::BitVector const& constraint) const {
    ValueType result = storm::utility::zero<ValueType>();
    for (const_iterator it = this->begin(row), ite = this->end(row); it != ite; ++it) {
        if (constraint.get(it->getColumn())) {
            result += it->getValue();
        }
    }
    return result;
}

template<typename ValueType>
std::vector<ValueType> SparseMatrix<ValueType>::getConstrainedRowSumVector(storm::storage::BitVector const& rowConstraint,
                                                                           storm::storage::BitVector const& columnConstraint) const {
    std::vector<ValueType> result(rowConstraint.getNumberOfSetBits());
    index_type currentRowCount = 0;
    for (auto row : rowConstraint) {
        result[currentRowCount++] = getConstrainedRowSum(row, columnConstraint);
    }
    return result;
}

template<typename ValueType>
std::vector<ValueType> SparseMatrix<ValueType>::getConstrainedRowGroupSumVector(storm::storage::BitVector const& rowGroupConstraint,
                                                                                storm::storage::BitVector const& columnConstraint) const {
    std::vector<ValueType> result;
    result.reserve(this->getNumRowsInRowGroups(rowGroupConstraint));
    if (!this->hasTrivialRowGrouping()) {
        for (auto rowGroup : rowGroupConstraint) {
            for (index_type row = this->getRowGroupIndices()[rowGroup]; row < this->getRowGroupIndices()[rowGroup + 1]; ++row) {
                result.push_back(getConstrainedRowSum(row, columnConstraint));
            }
        }
    } else {
        for (auto rowGroup : rowGroupConstraint) {
            result.push_back(getConstrainedRowSum(rowGroup, columnConstraint));
        }
    }
    return result;
}

template<typename ValueType>
SparseMatrix<ValueType> SparseMatrix<ValueType>::getSubmatrix(bool useGroups, storm::storage::BitVector const& rowConstraint,
                                                              storm::storage::BitVector const& columnConstraint, bool insertDiagonalElements,
                                                              storm::storage::BitVector const& makeZeroColumns) const {
    if (useGroups) {
        return getSubmatrix(rowConstraint, columnConstraint, this->getRowGroupIndices(), insertDiagonalElements, makeZeroColumns);
    } else {
        // Create a fake row grouping to reduce this to a call to a more general method.
        std::vector<index_type> fakeRowGroupIndices(rowCount + 1);
        index_type i = 0;
        for (std::vector<index_type>::iterator it = fakeRowGroupIndices.begin(); it != fakeRowGroupIndices.end(); ++it, ++i) {
            *it = i;
        }
        auto res = getSubmatrix(rowConstraint, columnConstraint, fakeRowGroupIndices, insertDiagonalElements, makeZeroColumns);

        // Create a new row grouping that reflects the new sizes of the row groups if the current matrix has a
        // non trivial row-grouping.
        if (!this->hasTrivialRowGrouping()) {
            std::vector<index_type> newRowGroupIndices;
            newRowGroupIndices.push_back(0);
            auto selectedRowIt = rowConstraint.begin();

            // For this, we need to count how many rows were preserved in every group.
            for (index_type group = 0; group < this->getRowGroupCount(); ++group) {
                index_type newRowCount = 0;
                while (*selectedRowIt < this->getRowGroupIndices()[group + 1]) {
                    ++selectedRowIt;
                    ++newRowCount;
                }
                if (newRowCount > 0) {
                    newRowGroupIndices.push_back(newRowGroupIndices.back() + newRowCount);
                }
            }

            res.trivialRowGrouping = false;
            res.rowGroupIndices = newRowGroupIndices;
        }

        return res;
    }
}

template<typename ValueType>
SparseMatrix<ValueType> SparseMatrix<ValueType>::getSubmatrix(storm::storage::BitVector const& rowGroupConstraint,
                                                              storm::storage::BitVector const& columnConstraint, std::vector<index_type> const& rowGroupIndices,
                                                              bool insertDiagonalEntries, storm::storage::BitVector const& makeZeroColumns) const {
    STORM_LOG_THROW(!rowGroupConstraint.empty() && !columnConstraint.empty(), storm::exceptions::InvalidArgumentException, "Cannot build empty submatrix.");
    index_type submatrixColumnCount = columnConstraint.getNumberOfSetBits();

    // Start by creating a temporary vector that stores for each index whose bit is set to true the number of
    // bits that were set before that particular index.
    std::vector<index_type> columnBitsSetBeforeIndex = columnConstraint.getNumberOfSetBitsBeforeIndices();
    std::unique_ptr<std::vector<index_type>> tmp;
    if (rowGroupConstraint != columnConstraint) {
        tmp = std::make_unique<std::vector<index_type>>(rowGroupConstraint.getNumberOfSetBitsBeforeIndices());
    }
    std::vector<index_type> const& rowBitsSetBeforeIndex = tmp ? *tmp : columnBitsSetBeforeIndex;

    // Then, we need to determine the number of entries and the number of rows of the submatrix.
    index_type subEntries = 0;
    index_type subRows = 0;
    index_type rowGroupCount = 0;
    for (auto index : rowGroupConstraint) {
        subRows += rowGroupIndices[index + 1] - rowGroupIndices[index];
        for (index_type i = rowGroupIndices[index]; i < rowGroupIndices[index + 1]; ++i) {
            bool foundDiagonalElement = false;

            for (const_iterator it = this->begin(i), ite = this->end(i); it != ite; ++it) {
                if (columnConstraint.get(it->getColumn()) && (makeZeroColumns.size() == 0 || !makeZeroColumns.get(it->getColumn()))) {
                    ++subEntries;

                    if (columnBitsSetBeforeIndex[it->getColumn()] == rowBitsSetBeforeIndex[index]) {
                        foundDiagonalElement = true;
                    }
                }
            }

            // If requested, we need to reserve one entry more for inserting the diagonal zero entry.
            if (insertDiagonalEntries && !foundDiagonalElement && rowGroupCount < submatrixColumnCount) {
                ++subEntries;
            }
        }
        ++rowGroupCount;
    }

    // Create and initialize resulting matrix.
    SparseMatrixBuilder<ValueType> matrixBuilder(subRows, submatrixColumnCount, subEntries, true, !this->hasTrivialRowGrouping());

    // Copy over selected entries.
    rowGroupCount = 0;
    index_type rowCount = 0;
    subEntries = 0;
    for (auto index : rowGroupConstraint) {
        if (!this->hasTrivialRowGrouping()) {
            matrixBuilder.newRowGroup(rowCount);
        }
        for (index_type i = rowGroupIndices[index]; i < rowGroupIndices[index + 1]; ++i) {
            bool insertedDiagonalElement = false;

            for (const_iterator it = this->begin(i), ite = this->end(i); it != ite; ++it) {
                if (columnConstraint.get(it->getColumn()) && (makeZeroColumns.size() == 0 || !makeZeroColumns.get(it->getColumn()))) {
                    if (columnBitsSetBeforeIndex[it->getColumn()] == rowBitsSetBeforeIndex[index]) {
                        insertedDiagonalElement = true;
                    } else if (insertDiagonalEntries && !insertedDiagonalElement && columnBitsSetBeforeIndex[it->getColumn()] > rowBitsSetBeforeIndex[index]) {
                        matrixBuilder.addNextValue(rowCount, rowGroupCount, storm::utility::zero<ValueType>());
                        insertedDiagonalElement = true;
                    }
                    ++subEntries;
                    matrixBuilder.addNextValue(rowCount, columnBitsSetBeforeIndex[it->getColumn()], it->getValue());
                }
            }
            if (insertDiagonalEntries && !insertedDiagonalElement && rowGroupCount < submatrixColumnCount) {
                matrixBuilder.addNextValue(rowCount, rowGroupCount, storm::utility::zero<ValueType>());
            }
            ++rowCount;
        }
        ++rowGroupCount;
    }

    return matrixBuilder.build();
}

template<typename ValueType>
SparseMatrix<ValueType> SparseMatrix<ValueType>::restrictRows(storm::storage::BitVector const& rowsToKeep, bool allowEmptyRowGroups) const {
    STORM_LOG_ASSERT(rowsToKeep.size() == this->getRowCount(), "Dimensions mismatch.");

    // Count the number of entries of the resulting matrix
    index_type entryCount = 0;
    for (auto row : rowsToKeep) {
        entryCount += this->getRow(row).getNumberOfEntries();
    }

    // Get the smallest row group index such that all row groups with at least this index are empty.
    index_type firstTrailingEmptyRowGroup = this->getRowGroupCount();
    for (auto groupIndexIt = this->getRowGroupIndices().rbegin() + 1; groupIndexIt != this->getRowGroupIndices().rend(); ++groupIndexIt) {
        if (rowsToKeep.getNextSetIndex(*groupIndexIt) != rowsToKeep.size()) {
            break;
        }
        --firstTrailingEmptyRowGroup;
    }
    STORM_LOG_THROW(allowEmptyRowGroups || firstTrailingEmptyRowGroup == this->getRowGroupCount(), storm::exceptions::InvalidArgumentException,
                    "Empty rows are not allowed, but row group " << firstTrailingEmptyRowGroup << " is empty.");

    // build the matrix. The row grouping will always be considered as nontrivial.
    SparseMatrixBuilder<ValueType> builder(rowsToKeep.getNumberOfSetBits(), this->getColumnCount(), entryCount, true, true, this->getRowGroupCount());
    index_type newRow = 0;
    for (index_type rowGroup = 0; rowGroup < firstTrailingEmptyRowGroup; ++rowGroup) {
        // Add a new row group
        builder.newRowGroup(newRow);
        bool rowGroupEmpty = true;
        for (index_type row = rowsToKeep.getNextSetIndex(this->getRowGroupIndices()[rowGroup]); row < this->getRowGroupIndices()[rowGroup + 1];
             row = rowsToKeep.getNextSetIndex(row + 1)) {
            rowGroupEmpty = false;
            for (auto const& entry : this->getRow(row)) {
                builder.addNextValue(newRow, entry.getColumn(), entry.getValue());
            }
            ++newRow;
        }
        STORM_LOG_THROW(allowEmptyRowGroups || !rowGroupEmpty, storm::exceptions::InvalidArgumentException,
                        "Empty rows are not allowed, but row group " << rowGroup << " is empty.");
    }

    // The all remaining row groups will be empty. Note that it is not allowed to call builder.addNewGroup(...) if there are no more rows afterwards.
    SparseMatrix<ValueType> res = builder.build();
    return res;
}

template<typename ValueType>
SparseMatrix<ValueType> SparseMatrix<ValueType>::filterEntries(storm::storage::BitVector const& rowFilter) const {
    // Count the number of entries in the resulting matrix.
    index_type entryCount = 0;
    for (auto row : rowFilter) {
        entryCount += getRow(row).getNumberOfEntries();
    }

    // Build the resulting matrix.
    SparseMatrixBuilder<ValueType> builder(getRowCount(), getColumnCount(), entryCount);
    for (auto row : rowFilter) {
        for (auto const& entry : getRow(row)) {
            builder.addNextValue(row, entry.getColumn(), entry.getValue());
        }
    }
    SparseMatrix<ValueType> result = builder.build();

    // Add a row grouping if necessary.
    if (!hasTrivialRowGrouping()) {
        result.setRowGroupIndices(getRowGroupIndices());
    }
    return result;
}

template<typename ValueType>
void SparseMatrix<ValueType>::dropZeroEntries() {
    updateNonzeroEntryCount();
    if (getNonzeroEntryCount() != getEntryCount()) {
        SparseMatrixBuilder<ValueType> builder(getRowCount(), getColumnCount(), getNonzeroEntryCount(), true);
        for (index_type row = 0; row < getRowCount(); ++row) {
            for (auto const& entry : getRow(row)) {
                if (!storm::utility::isZero(entry.getValue())) {
                    builder.addNextValue(row, entry.getColumn(), entry.getValue());
                }
            }
        }
        SparseMatrix<ValueType> result = builder.build();
        // Add a row grouping if necessary.
        if (!hasTrivialRowGrouping()) {
            result.setRowGroupIndices(getRowGroupIndices());
        }
        *this = std::move(result);
    }
}

template<typename ValueType>
SparseMatrix<ValueType> SparseMatrix<ValueType>::selectRowsFromRowGroups(std::vector<index_type> const& rowGroupToRowIndexMapping,
                                                                         bool insertDiagonalEntries) const {
    // First, we need to count how many non-zero entries the resulting matrix will have and reserve space for
    // diagonal entries if requested.
    index_type subEntries = 0;
    for (index_type rowGroupIndex = 0, rowGroupIndexEnd = rowGroupToRowIndexMapping.size(); rowGroupIndex < rowGroupIndexEnd; ++rowGroupIndex) {
        // Determine which row we need to select from the current row group.
        STORM_LOG_ASSERT(rowGroupToRowIndexMapping[rowGroupIndex] < this->getRowGroupSize(rowGroupIndex),
                         "Cannot point to row offset " << rowGroupToRowIndexMapping[rowGroupIndex] << " for rowGroup " << rowGroupIndex << " which starts at "
                                                       << this->getRowGroupIndices()[rowGroupIndex] << " and ends at "
                                                       << this->getRowGroupIndices()[rowGroupIndex + 1] << ".");
        index_type rowToCopy = this->getRowGroupIndices()[rowGroupIndex] + rowGroupToRowIndexMapping[rowGroupIndex];

        // Iterate through that row and count the number of slots we have to reserve for copying.
        bool foundDiagonalElement = false;
        for (const_iterator it = this->begin(rowToCopy), ite = this->end(rowToCopy); it != ite; ++it) {
            if (it->getColumn() == rowGroupIndex) {
                foundDiagonalElement = true;
            }
            ++subEntries;
        }
        if (insertDiagonalEntries && !foundDiagonalElement) {
            ++subEntries;
        }
    }

    // Now create the matrix to be returned with the appropriate size.
    SparseMatrixBuilder<ValueType> matrixBuilder(rowGroupIndices.get().size() - 1, columnCount, subEntries);

    // Copy over the selected lines from the source matrix.
    for (index_type rowGroupIndex = 0, rowGroupIndexEnd = rowGroupToRowIndexMapping.size(); rowGroupIndex < rowGroupIndexEnd; ++rowGroupIndex) {
        // Determine which row we need to select from the current row group.
        index_type rowToCopy = this->getRowGroupIndices()[rowGroupIndex] + rowGroupToRowIndexMapping[rowGroupIndex];

        // Iterate through that row and copy the entries. This also inserts a zero element on the diagonal if
        // there is no entry yet.
        bool insertedDiagonalElement = false;
        for (const_iterator it = this->begin(rowToCopy), ite = this->end(rowToCopy); it != ite; ++it) {
            if (it->getColumn() == rowGroupIndex) {
                insertedDiagonalElement = true;
            } else if (insertDiagonalEntries && !insertedDiagonalElement && it->getColumn() > rowGroupIndex) {
                matrixBuilder.addNextValue(rowGroupIndex, rowGroupIndex, storm::utility::zero<ValueType>());
                insertedDiagonalElement = true;
            }
            matrixBuilder.addNextValue(rowGroupIndex, it->getColumn(), it->getValue());
        }
        if (insertDiagonalEntries && !insertedDiagonalElement) {
            matrixBuilder.addNextValue(rowGroupIndex, rowGroupIndex, storm::utility::zero<ValueType>());
        }
    }

    // Finalize created matrix and return result.
    return matrixBuilder.build();
}

template<typename ValueType>
SparseMatrix<ValueType> SparseMatrix<ValueType>::selectRowsFromRowIndexSequence(std::vector<index_type> const& rowIndexSequence,
                                                                                bool insertDiagonalEntries) const {
    // First, we need to count how many non-zero entries the resulting matrix will have and reserve space for
    // diagonal entries if requested.
    index_type newEntries = 0;
    for (index_type row = 0, rowEnd = rowIndexSequence.size(); row < rowEnd; ++row) {
        bool foundDiagonalElement = false;
        for (const_iterator it = this->begin(rowIndexSequence[row]), ite = this->end(rowIndexSequence[row]); it != ite; ++it) {
            if (it->getColumn() == row) {
                foundDiagonalElement = true;
            }
            ++newEntries;
        }
        if (insertDiagonalEntries && !foundDiagonalElement) {
            ++newEntries;
        }
    }

    // Now create the matrix to be returned with the appropriate size.
    SparseMatrixBuilder<ValueType> matrixBuilder(rowIndexSequence.size(), columnCount, newEntries);

    // Copy over the selected rows from the source matrix.
    for (index_type row = 0, rowEnd = rowIndexSequence.size(); row < rowEnd; ++row) {
        bool insertedDiagonalElement = false;
        for (const_iterator it = this->begin(rowIndexSequence[row]), ite = this->end(rowIndexSequence[row]); it != ite; ++it) {
            if (it->getColumn() == row) {
                insertedDiagonalElement = true;
            } else if (insertDiagonalEntries && !insertedDiagonalElement && it->getColumn() > row) {
                matrixBuilder.addNextValue(row, row, storm::utility::zero<ValueType>());
                insertedDiagonalElement = true;
            }
            matrixBuilder.addNextValue(row, it->getColumn(), it->getValue());
        }
        if (insertDiagonalEntries && !insertedDiagonalElement) {
            matrixBuilder.addNextValue(row, row, storm::utility::zero<ValueType>());
        }
    }

    // Finally create matrix and return result.
    return matrixBuilder.build();
}

template<typename ValueType>
SparseMatrix<ValueType> SparseMatrix<ValueType>::permuteRows(std::vector<index_type> const& inversePermutation) const {
    // Now create the matrix to be returned with the appropriate size.
    // The entry size is only adequate if this is indeed a permutation.
    SparseMatrixBuilder<ValueType> matrixBuilder(inversePermutation.size(), columnCount, entryCount);

    // Copy over the selected rows from the source matrix.

    for (index_type writeTo = 0; writeTo < inversePermutation.size(); ++writeTo) {
        index_type const& readFrom = inversePermutation[writeTo];
        auto row = this->getRow(readFrom);
        for (auto const& entry : row) {
            matrixBuilder.addNextValue(writeTo, entry.getColumn(), entry.getValue());
        }
    }
    // Finally create matrix and return result.
    auto result = matrixBuilder.build();
    if (this->rowGroupIndices) {
        result.setRowGroupIndices(this->rowGroupIndices.get());
    }
    return result;
}

template<typename ValueType>
SparseMatrix<ValueType> SparseMatrix<ValueType>::permuteRowGroupsAndColumns(std::vector<index_type> const& inverseRowGroupPermutation,
                                                                            std::vector<index_type> const& columnPermutation) const {
    STORM_LOG_ASSERT(storm::utility::permutation::isValidPermutation(inverseRowGroupPermutation), "Row group permutation is not a permutation.");
    STORM_LOG_ASSERT(storm::utility::permutation::isValidPermutation(columnPermutation), "Column permutation is not a permutation.");
    index_type const rowCount = getRowCount();
    SparseMatrixBuilder<ValueType> matrixBuilder(rowCount, getColumnCount(), getEntryCount(), true, !hasTrivialRowGrouping(), getRowGroupCount());
    auto oldGroupIt = inverseRowGroupPermutation.cbegin();
    index_type newRowIndex = 0;
    while (newRowIndex < rowCount) {
        if (!hasTrivialRowGrouping()) {
            matrixBuilder.newRowGroup(newRowIndex);
        }
        for (auto oldRowIndex : getRowGroupIndices(*oldGroupIt)) {
            for (auto const& oldEntry : getRow(oldRowIndex)) {
                matrixBuilder.addNextValue(newRowIndex, columnPermutation[oldEntry.getColumn()], oldEntry.getValue());
            }
            ++newRowIndex;
        }
        ++oldGroupIt;
    }
    return matrixBuilder.build();
}

template<typename ValueType>
SparseMatrix<ValueType> SparseMatrix<ValueType>::transpose(bool joinGroups, bool keepZeros) const {
    index_type rowCount = this->getColumnCount();
    index_type columnCount = joinGroups ? this->getRowGroupCount() : this->getRowCount();
    index_type entryCount;
    if (keepZeros) {
        entryCount = this->getEntryCount();
    } else {
        this->updateNonzeroEntryCount();
        entryCount = this->getNonzeroEntryCount();
    }

    std::vector<index_type> rowIndications(rowCount + 1);
    std::vector<MatrixEntry<index_type, ValueType>> columnsAndValues(entryCount);

    // First, we need to count how many entries each column has.
    for (index_type group = 0; group < columnCount; ++group) {
        for (auto const& transition : joinGroups ? this->getRowGroup(group) : this->getRow(group)) {
            if (transition.getValue() != storm::utility::zero<ValueType>() || keepZeros) {
                ++rowIndications[transition.getColumn() + 1];
            }
        }
    }

    // Now compute the accumulated offsets.
    for (index_type i = 1; i < rowCount + 1; ++i) {
        rowIndications[i] = rowIndications[i - 1] + rowIndications[i];
    }

    // Create an array that stores the index for the next value to be added for
    // each row in the transposed matrix. Initially this corresponds to the previously
    // computed accumulated offsets.
    std::vector<index_type> nextIndices = rowIndications;

    // Now we are ready to actually fill in the values of the transposed matrix.
    for (index_type group = 0; group < columnCount; ++group) {
        for (auto const& transition : joinGroups ? this->getRowGroup(group) : this->getRow(group)) {
            if (transition.getValue() != storm::utility::zero<ValueType>() || keepZeros) {
                columnsAndValues[nextIndices[transition.getColumn()]] = std::make_pair(group, transition.getValue());
                nextIndices[transition.getColumn()]++;
            }
        }
    }

    storm::storage::SparseMatrix<ValueType> transposedMatrix(columnCount, std::move(rowIndications), std::move(columnsAndValues), boost::none);

    return transposedMatrix;
}

template<typename ValueType>
SparseMatrix<ValueType> SparseMatrix<ValueType>::transposeSelectedRowsFromRowGroups(std::vector<uint64_t> const& rowGroupChoices, bool keepZeros) const {
    index_type rowCount = this->getColumnCount();
    index_type columnCount = this->getRowGroupCount();

    // Get the overall entry count as well as the number of entries of each column
    index_type entryCount = 0;
    std::vector<index_type> rowIndications(columnCount + 1);
    auto rowGroupChoiceIt = rowGroupChoices.begin();
    for (index_type rowGroup = 0; rowGroup < columnCount; ++rowGroup, ++rowGroupChoiceIt) {
        for (auto const& entry : this->getRow(rowGroup, *rowGroupChoiceIt)) {
            if (keepZeros || !storm::utility::isZero(entry.getValue())) {
                ++entryCount;
                ++rowIndications[entry.getColumn() + 1];
            }
        }
    }

    // Now compute the accumulated offsets.
    for (index_type i = 1; i < rowCount + 1; ++i) {
        rowIndications[i] = rowIndications[i - 1] + rowIndications[i];
    }

    std::vector<MatrixEntry<index_type, ValueType>> columnsAndValues(entryCount);

    // Create an array that stores the index for the next value to be added for
    // each row in the transposed matrix. Initially this corresponds to the previously
    // computed accumulated offsets.
    std::vector<index_type> nextIndices = rowIndications;

    // Now we are ready to actually fill in the values of the transposed matrix.
    rowGroupChoiceIt = rowGroupChoices.begin();
    for (index_type rowGroup = 0; rowGroup < columnCount; ++rowGroup, ++rowGroupChoiceIt) {
        for (auto const& entry : this->getRow(rowGroup, *rowGroupChoiceIt)) {
            if (keepZeros || !storm::utility::isZero(entry.getValue())) {
                columnsAndValues[nextIndices[entry.getColumn()]] = std::make_pair(rowGroup, entry.getValue());
                ++nextIndices[entry.getColumn()];
            }
        }
    }

    return storm::storage::SparseMatrix<ValueType>(std::move(columnCount), std::move(rowIndications), std::move(columnsAndValues), boost::none);
}

template<typename ValueType>
void SparseMatrix<ValueType>::convertToEquationSystem() {
    invertDiagonal();
    negateAllNonDiagonalEntries();
}

template<typename ValueType>
void SparseMatrix<ValueType>::invertDiagonal() {
    // Now iterate over all row groups and set the diagonal elements to the inverted value.
    // If there is a row without the diagonal element, an exception is thrown.
    ValueType one = storm::utility::one<ValueType>();
    ValueType zero = storm::utility::zero<ValueType>();
    bool foundDiagonalElement = false;
    for (index_type group = 0; group < this->getRowGroupCount(); ++group) {
        for (auto& entry : this->getRowGroup(group)) {
            if (entry.getColumn() == group) {
                if (entry.getValue() == one) {
                    --this->nonzeroEntryCount;
                    entry.setValue(zero);
                } else if (entry.getValue() == zero) {
                    ++this->nonzeroEntryCount;
                    entry.setValue(one);
                } else {
                    entry.setValue(one - entry.getValue());
                }
                foundDiagonalElement = true;
            }
        }

        // Throw an exception if a row did not have an element on the diagonal.
        if (!foundDiagonalElement) {
            throw storm::exceptions::InvalidArgumentException() << "Illegal call to SparseMatrix::invertDiagonal: matrix is missing diagonal entries.";
        }
    }
}

template<typename ValueType>
void SparseMatrix<ValueType>::negateAllNonDiagonalEntries() {
    // Iterate over all row groups and negate all the elements that are not on the diagonal.
    for (index_type group = 0; group < this->getRowGroupCount(); ++group) {
        for (auto& entry : this->getRowGroup(group)) {
            if (entry.getColumn() != group) {
                entry.setValue(-entry.getValue());
            }
        }
    }
}

template<typename ValueType>
void SparseMatrix<ValueType>::deleteDiagonalEntries(bool dropZeroEntries) {
    // Iterate over all rows and negate all the elements that are not on the diagonal.
    for (index_type group = 0; group < this->getRowGroupCount(); ++group) {
        for (auto& entry : this->getRowGroup(group)) {
            if (entry.getColumn() == group) {
                --this->nonzeroEntryCount;
                entry.setValue(storm::utility::zero<ValueType>());
            }
        }
    }
    if (dropZeroEntries) {
        this->dropZeroEntries();
    }
}

template<typename ValueType>
typename std::pair<storm::storage::SparseMatrix<ValueType>, std::vector<ValueType>> SparseMatrix<ValueType>::getJacobiDecomposition() const {
    STORM_LOG_THROW(this->getRowCount() == this->getColumnCount(), storm::exceptions::InvalidArgumentException,
                    "Canno compute Jacobi decomposition of non-square matrix.");

    // Prepare the resulting data structures.
    SparseMatrixBuilder<ValueType> luBuilder(this->getRowCount(), this->getColumnCount());
    std::vector<ValueType> invertedDiagonal(rowCount);

    // Copy entries to the appropriate matrices.
    for (index_type rowNumber = 0; rowNumber < rowCount; ++rowNumber) {
        for (const_iterator it = this->begin(rowNumber), ite = this->end(rowNumber); it != ite; ++it) {
            if (it->getColumn() == rowNumber) {
                invertedDiagonal[rowNumber] = storm::utility::one<ValueType>() / it->getValue();
            } else {
                luBuilder.addNextValue(rowNumber, it->getColumn(), it->getValue());
            }
        }
    }

    return std::make_pair(luBuilder.build(), std::move(invertedDiagonal));
}

template<>
typename std::pair<storm::storage::SparseMatrix<Interval>, std::vector<Interval>> SparseMatrix<Interval>::getJacobiDecomposition() const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "This operation is not supported.");
}

template<>
typename std::pair<storm::storage::SparseMatrix<RationalFunction>, std::vector<RationalFunction>> SparseMatrix<RationalFunction>::getJacobiDecomposition()
    const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "This operation is not supported.");
}

template<typename ValueType>
template<typename OtherValueType, typename ResultValueType>
ResultValueType SparseMatrix<ValueType>::getPointwiseProductRowSum(storm::storage::SparseMatrix<OtherValueType> const& otherMatrix,
                                                                   index_type const& row) const {
    typename storm::storage::SparseMatrix<ValueType>::const_iterator it1 = this->begin(row);
    typename storm::storage::SparseMatrix<ValueType>::const_iterator ite1 = this->end(row);
    typename storm::storage::SparseMatrix<OtherValueType>::const_iterator it2 = otherMatrix.begin(row);
    typename storm::storage::SparseMatrix<OtherValueType>::const_iterator ite2 = otherMatrix.end(row);

    ResultValueType result = storm::utility::zero<ResultValueType>();
    for (; it1 != ite1 && it2 != ite2; ++it1) {
        if (it1->getColumn() < it2->getColumn()) {
            continue;
        } else {
            // If the precondition of this method (i.e. that the given matrix is a submatrix
            // of the current one) was fulfilled, we know now that the two elements are in
            // the same column, so we can multiply and add them to the row sum vector.
            STORM_LOG_ASSERT(it1->getColumn() == it2->getColumn(), "The given matrix is not a submatrix of this one.");
            result += it2->getValue() * OtherValueType(it1->getValue());
            ++it2;
        }
    }
    return result;
}

template<typename ValueType>
template<typename OtherValueType, typename ResultValueType>
std::vector<ResultValueType> SparseMatrix<ValueType>::getPointwiseProductRowSumVector(storm::storage::SparseMatrix<OtherValueType> const& otherMatrix) const {
    std::vector<ResultValueType> result;
    result.reserve(rowCount);
    for (index_type row = 0; row < rowCount && row < otherMatrix.getRowCount(); ++row) {
        result.push_back(getPointwiseProductRowSum<OtherValueType, ResultValueType>(otherMatrix, row));
    }
    return result;
}

template<typename ValueType>
void SparseMatrix<ValueType>::multiplyWithVector(std::vector<ValueType> const& vector, std::vector<ValueType>& result,
                                                 std::vector<value_type> const* summand) const {
    // If the vector and the result are aliases and this is not set to be allowed, we need and temporary vector.
    std::vector<ValueType>* target;
    std::vector<ValueType> temporary;
    if (&vector == &result) {
        STORM_LOG_WARN("Vectors are aliased. Using temporary, which is potentially slow.");
        temporary = std::vector<ValueType>(vector.size());
        target = &temporary;
    } else {
        target = &result;
    }

    this->multiplyWithVectorForward(vector, *target, summand);

    if (target == &temporary) {
        std::swap(result, *target);
    }
}

template<typename ValueType>
void SparseMatrix<ValueType>::multiplyWithVectorForward(std::vector<ValueType> const& vector, std::vector<ValueType>& result,
                                                        std::vector<value_type> const* summand) const {
    const_iterator it = this->begin();
    const_iterator ite;
    std::vector<index_type>::const_iterator rowIterator = rowIndications.begin();
    typename std::vector<ValueType>::iterator resultIterator = result.begin();
    typename std::vector<ValueType>::iterator resultIteratorEnd = result.end();
    typename std::vector<ValueType>::const_iterator summandIterator;
    if (summand) {
        summandIterator = summand->begin();
    }

    for (; resultIterator != resultIteratorEnd; ++rowIterator, ++resultIterator, ++summandIterator) {
        ValueType newValue;
        if (summand) {
            newValue = *summandIterator;
        } else {
            newValue = storm::utility::zero<ValueType>();
        }

        for (ite = this->begin() + *(rowIterator + 1); it != ite; ++it) {
            newValue += it->getValue() * vector[it->getColumn()];
        }

        *resultIterator = newValue;
    }
}

template<typename ValueType>
void SparseMatrix<ValueType>::multiplyWithVectorBackward(std::vector<ValueType> const& vector, std::vector<ValueType>& result,
                                                         std::vector<value_type> const* summand) const {
    const_iterator it = this->end() - 1;
    const_iterator ite;
    std::vector<index_type>::const_iterator rowIterator = rowIndications.end() - 2;
    typename std::vector<ValueType>::iterator resultIterator = result.end() - 1;
    typename std::vector<ValueType>::iterator resultIteratorEnd = result.begin() - 1;
    typename std::vector<ValueType>::const_iterator summandIterator;
    if (summand) {
        summandIterator = summand->end() - 1;
    }

    for (; resultIterator != resultIteratorEnd; --rowIterator, --resultIterator, --summandIterator) {
        ValueType newValue;
        if (summand) {
            newValue = *summandIterator;
        } else {
            newValue = storm::utility::zero<ValueType>();
        }

        for (ite = this->begin() + *rowIterator - 1; it != ite; --it) {
            newValue += (it->getValue() * vector[it->getColumn()]);
        }

        *resultIterator = newValue;
    }
}

template<typename ValueType>
ValueType SparseMatrix<ValueType>::multiplyRowWithVector(index_type row, std::vector<ValueType> const& vector) const {
    ValueType result = storm::utility::zero<ValueType>();

    for (auto const& entry : this->getRow(row)) {
        result += entry.getValue() * vector[entry.getColumn()];
    }
    return result;
}

template<typename ValueType>
void SparseMatrix<ValueType>::performSuccessiveOverRelaxationStep(ValueType omega, std::vector<ValueType>& x, std::vector<ValueType> const& b) const {
    const_iterator it = this->end() - 1;
    const_iterator ite;
    std::vector<index_type>::const_iterator rowIterator = rowIndications.end() - 2;
    typename std::vector<ValueType>::const_iterator bIt = b.end() - 1;
    typename std::vector<ValueType>::iterator resultIterator = x.end() - 1;
    typename std::vector<ValueType>::iterator resultIteratorEnd = x.begin() - 1;

    index_type currentRow = getRowCount();
    for (; resultIterator != resultIteratorEnd; --rowIterator, --resultIterator, --bIt) {
        --currentRow;
        ValueType tmpValue = storm::utility::zero<ValueType>();
        ValueType diagonalElement = storm::utility::zero<ValueType>();

        for (ite = this->begin() + *rowIterator - 1; it != ite; --it) {
            if (it->getColumn() != currentRow) {
                tmpValue += it->getValue() * x[it->getColumn()];
            } else {
                diagonalElement += it->getValue();
            }
        }
        assert(!storm::utility::isZero(diagonalElement));
        *resultIterator = ((storm::utility::one<ValueType>() - omega) * *resultIterator) + (omega / diagonalElement) * (*bIt - tmpValue);
    }
}

template<>
void SparseMatrix<Interval>::performSuccessiveOverRelaxationStep(Interval, std::vector<Interval>&, std::vector<Interval> const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This operation is not supported.");
}

template<typename ValueType>
void SparseMatrix<ValueType>::performWalkerChaeStep(std::vector<ValueType> const& x, std::vector<ValueType> const& columnSums, std::vector<ValueType> const& b,
                                                    std::vector<ValueType> const& ax, std::vector<ValueType>& result) const {
    const_iterator it = this->begin();
    const_iterator ite;
    std::vector<index_type>::const_iterator rowIterator = rowIndications.begin();

    // Clear all previous entries.
    ValueType zero = storm::utility::zero<ValueType>();
    for (auto& entry : result) {
        entry = zero;
    }

    for (index_type row = 0; row < rowCount; ++row, ++rowIterator) {
        for (ite = this->begin() + *(rowIterator + 1); it != ite; ++it) {
            result[it->getColumn()] += it->getValue() * (b[row] / ax[row]);
        }
    }

    auto xIterator = x.begin();
    auto sumsIterator = columnSums.begin();
    for (auto& entry : result) {
        entry *= *xIterator / *sumsIterator;
        ++xIterator;
        ++sumsIterator;
    }
}

template<>
void SparseMatrix<Interval>::performWalkerChaeStep(std::vector<Interval> const& x, std::vector<Interval> const& rowSums, std::vector<Interval> const& b,
                                                   std::vector<Interval> const& ax, std::vector<Interval>& result) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This operation is not supported.");
}

template<typename ValueType>
void SparseMatrix<ValueType>::multiplyAndReduceForward(OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices,
                                                       std::vector<ValueType> const& vector, std::vector<ValueType> const* summand,
                                                       std::vector<ValueType>& result, std::vector<uint64_t>* choices) const {
    if (dir == OptimizationDirection::Minimize) {
        multiplyAndReduceForward<storm::utility::ElementLess<ValueType>>(rowGroupIndices, vector, summand, result, choices);
    } else {
        multiplyAndReduceForward<storm::utility::ElementGreater<ValueType>>(rowGroupIndices, vector, summand, result, choices);
    }
}

template<typename ValueType>
template<typename Compare>
void SparseMatrix<ValueType>::multiplyAndReduceForward(std::vector<uint64_t> const& rowGroupIndices, std::vector<ValueType> const& vector,
                                                       std::vector<ValueType> const* summand, std::vector<ValueType>& result,
                                                       std::vector<uint64_t>* choices) const {
    Compare compare;
    auto elementIt = this->begin();
    auto rowGroupIt = rowGroupIndices.begin();
    auto rowIt = rowIndications.begin();
    typename std::vector<ValueType>::const_iterator summandIt;
    if (summand) {
        summandIt = summand->begin();
    }
    typename std::vector<uint64_t>::iterator choiceIt;
    if (choices) {
        choiceIt = choices->begin();
    }

    // Variables for correctly tracking choices (only update if new choice is strictly better).
    ValueType oldSelectedChoiceValue;
    uint64_t selectedChoice;

    uint64_t currentRow = 0;
    for (auto resultIt = result.begin(), resultIte = result.end(); resultIt != resultIte; ++resultIt, ++choiceIt, ++rowGroupIt) {
        ValueType currentValue = storm::utility::zero<ValueType>();

        // Only multiply and reduce if there is at least one row in the group.
        if (*rowGroupIt < *(rowGroupIt + 1)) {
            if (summand) {
                currentValue = *summandIt;
                ++summandIt;
            }

            for (auto elementIte = this->begin() + *(rowIt + 1); elementIt != elementIte; ++elementIt) {
                currentValue += elementIt->getValue() * vector[elementIt->getColumn()];
            }

            if (choices) {
                selectedChoice = 0;
                if (*choiceIt == 0) {
                    oldSelectedChoiceValue = currentValue;
                }
            }

            ++rowIt;
            ++currentRow;

            for (; currentRow < *(rowGroupIt + 1); ++rowIt, ++currentRow) {
                ValueType newValue = summand ? *summandIt : storm::utility::zero<ValueType>();
                for (auto elementIte = this->begin() + *(rowIt + 1); elementIt != elementIte; ++elementIt) {
                    newValue += elementIt->getValue() * vector[elementIt->getColumn()];
                }

                if (choices && currentRow == *choiceIt + *rowGroupIt) {
                    oldSelectedChoiceValue = newValue;
                }

                if (compare(newValue, currentValue)) {
                    currentValue = newValue;
                    if (choices) {
                        selectedChoice = currentRow - *rowGroupIt;
                    }
                }
                if (summand) {
                    ++summandIt;
                }
            }

            // Finally write value to target vector.
            *resultIt = currentValue;
            if (choices && compare(currentValue, oldSelectedChoiceValue)) {
                *choiceIt = selectedChoice;
            }
        }
    }
}

template<>
void SparseMatrix<storm::RationalFunction>::multiplyAndReduceForward(OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices,
                                                                     std::vector<storm::RationalFunction> const& vector,
                                                                     std::vector<storm::RationalFunction> const* b,
                                                                     std::vector<storm::RationalFunction>& result, std::vector<uint64_t>* choices) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This operation is not supported.");
}

template<typename ValueType>
void SparseMatrix<ValueType>::multiplyAndReduceBackward(OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices,
                                                        std::vector<ValueType> const& vector, std::vector<ValueType> const* summand,
                                                        std::vector<ValueType>& result, std::vector<uint64_t>* choices) const {
    if (dir == storm::OptimizationDirection::Minimize) {
        multiplyAndReduceBackward<storm::utility::ElementLess<ValueType>>(rowGroupIndices, vector, summand, result, choices);
    } else {
        multiplyAndReduceBackward<storm::utility::ElementGreater<ValueType>>(rowGroupIndices, vector, summand, result, choices);
    }
}

template<typename ValueType>
template<typename Compare>
void SparseMatrix<ValueType>::multiplyAndReduceBackward(std::vector<uint64_t> const& rowGroupIndices, std::vector<ValueType> const& vector,
                                                        std::vector<ValueType> const* summand, std::vector<ValueType>& result,
                                                        std::vector<uint64_t>* choices) const {
    Compare compare;
    auto elementIt = this->end() - 1;
    auto rowGroupIt = rowGroupIndices.end() - 2;
    auto rowIt = rowIndications.end() - 2;
    typename std::vector<ValueType>::const_iterator summandIt;
    if (summand) {
        summandIt = summand->end() - 1;
    }
    typename std::vector<uint64_t>::iterator choiceIt;
    if (choices) {
        choiceIt = choices->end() - 1;
    }

    // Variables for correctly tracking choices (only update if new choice is strictly better).
    ValueType oldSelectedChoiceValue;
    uint64_t selectedChoice;

    uint64_t currentRow = this->getRowCount() - 1;
    for (auto resultIt = result.end() - 1, resultIte = result.begin() - 1; resultIt != resultIte; --resultIt, --choiceIt, --rowGroupIt) {
        ValueType currentValue = storm::utility::zero<ValueType>();

        // Only multiply and reduce if there is at least one row in the group.
        if (*rowGroupIt < *(rowGroupIt + 1)) {
            if (summand) {
                currentValue = *summandIt;
                --summandIt;
            }

            for (auto elementIte = this->begin() + *rowIt - 1; elementIt != elementIte; --elementIt) {
                currentValue += elementIt->getValue() * vector[elementIt->getColumn()];
            }
            if (choices) {
                selectedChoice = currentRow - *rowGroupIt;
                if (*choiceIt == selectedChoice) {
                    oldSelectedChoiceValue = currentValue;
                }
            }
            --rowIt;
            --currentRow;

            for (uint64_t i = *rowGroupIt + 1, end = *(rowGroupIt + 1); i < end; --rowIt, --currentRow, ++i, --summandIt) {
                ValueType newValue = summand ? *summandIt : storm::utility::zero<ValueType>();
                for (auto elementIte = this->begin() + *rowIt - 1; elementIt != elementIte; --elementIt) {
                    newValue += elementIt->getValue() * vector[elementIt->getColumn()];
                }

                if (choices && currentRow == *choiceIt + *rowGroupIt) {
                    oldSelectedChoiceValue = newValue;
                }

                if (compare(newValue, currentValue)) {
                    currentValue = newValue;
                    if (choices) {
                        selectedChoice = currentRow - *rowGroupIt;
                    }
                }
            }

            // Finally write value to target vector.
            *resultIt = currentValue;
            if (choices && compare(currentValue, oldSelectedChoiceValue)) {
                *choiceIt = selectedChoice;
            }
        }
    }
}

template<>
void SparseMatrix<storm::RationalFunction>::multiplyAndReduceBackward(OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices,
                                                                      std::vector<storm::RationalFunction> const& vector,
                                                                      std::vector<storm::RationalFunction> const* b,
                                                                      std::vector<storm::RationalFunction>& result, std::vector<uint64_t>* choices) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This operation is not supported.");
}

template<typename ValueType>
void SparseMatrix<ValueType>::multiplyAndReduce(OptimizationDirection const& dir, std::vector<uint64_t> const& rowGroupIndices,
                                                std::vector<ValueType> const& vector, std::vector<ValueType> const* summand, std::vector<ValueType>& result,
                                                std::vector<uint64_t>* choices) const {
    // If the vector and the result are aliases, we need and temporary vector.
    std::vector<ValueType>* target;
    std::vector<ValueType> temporary;
    if (&vector == &result) {
        STORM_LOG_WARN("Vectors are aliased but are not allowed to be. Using temporary, which is potentially slow.");
        temporary = std::vector<ValueType>(vector.size());
        target = &temporary;
    } else {
        target = &result;
    }

    this->multiplyAndReduceForward(dir, rowGroupIndices, vector, summand, *target, choices);

    if (target == &temporary) {
        std::swap(temporary, result);
    }
}

template<typename ValueType>
void SparseMatrix<ValueType>::multiplyVectorWithMatrix(std::vector<value_type> const& vector, std::vector<value_type>& result) const {
    const_iterator it = this->begin();
    const_iterator ite;
    std::vector<index_type>::const_iterator rowIterator = rowIndications.begin();
    std::vector<index_type>::const_iterator rowIteratorEnd = rowIndications.end();

    index_type currentRow = 0;
    for (; rowIterator != rowIteratorEnd - 1; ++rowIterator) {
        for (ite = this->begin() + *(rowIterator + 1); it != ite; ++it) {
            result[it->getColumn()] += it->getValue() * vector[currentRow];
        }
        ++currentRow;
    }
}

template<typename ValueType>
void SparseMatrix<ValueType>::scaleRowsInPlace(std::vector<ValueType> const& factors) {
    STORM_LOG_ASSERT(factors.size() == this->getRowCount(), "Can not scale rows: Number of rows and number of scaling factors do not match.");
    index_type row = 0;
    for (auto const& factor : factors) {
        for (auto& entry : getRow(row)) {
            entry.setValue(entry.getValue() * factor);
        }
        ++row;
    }
}

template<typename ValueType>
void SparseMatrix<ValueType>::divideRowsInPlace(std::vector<ValueType> const& divisors) {
    STORM_LOG_ASSERT(divisors.size() == this->getRowCount(), "Can not divide rows: Number of rows and number of divisors do not match.");
    index_type row = 0;
    for (auto const& divisor : divisors) {
        STORM_LOG_ASSERT(!storm::utility::isZero(divisor), "Can not divide row " << row << " by 0.");
        for (auto& entry : getRow(row)) {
            entry.setValue(entry.getValue() / divisor);
        }
        ++row;
    }
}

template<>
void SparseMatrix<Interval>::divideRowsInPlace(std::vector<Interval> const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "This operation is not supported.");
}

template<typename ValueType>
typename SparseMatrix<ValueType>::const_rows SparseMatrix<ValueType>::getRows(index_type startRow, index_type endRow) const {
    return const_rows(this->columnsAndValues.begin() + this->rowIndications[startRow], this->rowIndications[endRow] - this->rowIndications[startRow]);
}

template<typename ValueType>
typename SparseMatrix<ValueType>::rows SparseMatrix<ValueType>::getRows(index_type startRow, index_type endRow) {
    return rows(this->columnsAndValues.begin() + this->rowIndications[startRow], this->rowIndications[endRow] - this->rowIndications[startRow]);
}

template<typename ValueType>
typename SparseMatrix<ValueType>::const_rows SparseMatrix<ValueType>::getRow(index_type row) const {
    return getRows(row, row + 1);
}

template<typename ValueType>
typename SparseMatrix<ValueType>::rows SparseMatrix<ValueType>::getRow(index_type row) {
    return getRows(row, row + 1);
}

template<typename ValueType>
typename SparseMatrix<ValueType>::const_rows SparseMatrix<ValueType>::getRow(index_type rowGroup, index_type offset) const {
    STORM_LOG_ASSERT(rowGroup < this->getRowGroupCount(), "Row group is out-of-bounds.");
    STORM_LOG_ASSERT(offset < this->getRowGroupSize(rowGroup), "Row offset in row-group is out-of-bounds.");
    if (!this->hasTrivialRowGrouping()) {
        return getRow(this->getRowGroupIndices()[rowGroup] + offset);
    } else {
        return getRow(this->getRowGroupIndices()[rowGroup] + offset);
    }
}

template<typename ValueType>
typename SparseMatrix<ValueType>::rows SparseMatrix<ValueType>::getRow(index_type rowGroup, index_type offset) {
    STORM_LOG_ASSERT(rowGroup < this->getRowGroupCount(), "Row group is out-of-bounds.");
    STORM_LOG_ASSERT(offset < this->getRowGroupSize(rowGroup), "Row offset in row-group is out-of-bounds.");
    if (!this->hasTrivialRowGrouping()) {
        return getRow(this->getRowGroupIndices()[rowGroup] + offset);
    } else {
        STORM_LOG_ASSERT(offset == 0, "Invalid offset.");
        return getRow(rowGroup + offset);
    }
}

template<typename ValueType>
typename SparseMatrix<ValueType>::const_rows SparseMatrix<ValueType>::getRowGroup(index_type rowGroup) const {
    STORM_LOG_ASSERT(rowGroup < this->getRowGroupCount(), "Row group is out-of-bounds.");
    if (!this->hasTrivialRowGrouping()) {
        return getRows(this->getRowGroupIndices()[rowGroup], this->getRowGroupIndices()[rowGroup + 1]);
    } else {
        return getRows(rowGroup, rowGroup + 1);
    }
}

template<typename ValueType>
typename SparseMatrix<ValueType>::rows SparseMatrix<ValueType>::getRowGroup(index_type rowGroup) {
    STORM_LOG_ASSERT(rowGroup < this->getRowGroupCount(), "Row group is out-of-bounds.");
    if (!this->hasTrivialRowGrouping()) {
        return getRows(this->getRowGroupIndices()[rowGroup], this->getRowGroupIndices()[rowGroup + 1]);
    } else {
        return getRows(rowGroup, rowGroup + 1);
    }
}

template<typename ValueType>
typename SparseMatrix<ValueType>::const_iterator SparseMatrix<ValueType>::begin(index_type row) const {
    STORM_LOG_ASSERT(row < this->getRowCount(), "Row " << row << " exceeds row count " << this->getRowCount() << ".");
    return this->columnsAndValues.begin() + this->rowIndications[row];
}

template<typename ValueType>
typename SparseMatrix<ValueType>::iterator SparseMatrix<ValueType>::begin(index_type row) {
    STORM_LOG_ASSERT(row < this->getRowCount(), "Row " << row << " exceeds row count " << this->getRowCount() << ".");
    return this->columnsAndValues.begin() + this->rowIndications[row];
}

template<typename ValueType>
typename SparseMatrix<ValueType>::const_iterator SparseMatrix<ValueType>::begin() const {
    return this->columnsAndValues.begin();
}

template<typename ValueType>
typename SparseMatrix<ValueType>::iterator SparseMatrix<ValueType>::begin() {
    return this->columnsAndValues.begin();
}

template<typename ValueType>
typename SparseMatrix<ValueType>::const_iterator SparseMatrix<ValueType>::end(index_type row) const {
    STORM_LOG_ASSERT(row < this->getRowCount(), "Row " << row << " exceeds row count " << this->getRowCount() << ".");
    return this->columnsAndValues.begin() + this->rowIndications[row + 1];
}

template<typename ValueType>
typename SparseMatrix<ValueType>::iterator SparseMatrix<ValueType>::end(index_type row) {
    STORM_LOG_ASSERT(row < this->getRowCount(), "Row " << row << " exceeds row count " << this->getRowCount() << ".");
    return this->columnsAndValues.begin() + this->rowIndications[row + 1];
}

template<typename ValueType>
typename SparseMatrix<ValueType>::const_iterator SparseMatrix<ValueType>::end() const {
    return this->columnsAndValues.begin() + this->rowIndications[rowCount];
}

template<typename ValueType>
typename SparseMatrix<ValueType>::iterator SparseMatrix<ValueType>::end() {
    return this->columnsAndValues.begin() + this->rowIndications[rowCount];
}

template<typename ValueType>
ValueType SparseMatrix<ValueType>::getRowSum(index_type row) const {
    ValueType sum = storm::utility::zero<ValueType>();
    for (const_iterator it = this->begin(row), ite = this->end(row); it != ite; ++it) {
        sum += it->getValue();
    }
    return sum;
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::getNonconstantEntryCount() const {
    index_type nonConstEntries = 0;
    for (auto const& entry : *this) {
        if (!storm::utility::isConstant(entry.getValue())) {
            ++nonConstEntries;
        }
    }
    return nonConstEntries;
}

template<typename ValueType>
typename SparseMatrix<ValueType>::index_type SparseMatrix<ValueType>::getNonconstantRowGroupCount() const {
    index_type nonConstRowGroups = 0;
    for (index_type rowGroup = 0; rowGroup < this->getRowGroupCount(); ++rowGroup) {
        for (auto const& entry : this->getRowGroup(rowGroup)) {
            if (!storm::utility::isConstant(entry.getValue())) {
                ++nonConstRowGroups;
                break;
            }
        }
    }
    return nonConstRowGroups;
}

template<typename ValueType>
bool SparseMatrix<ValueType>::isProbabilistic(ValueType const& tolerance, storm::OptionalRef<std::string> reason) const {
    using BaseType =
        std::conditional_t<std::is_same_v<ValueType, storm::RationalFunction>, storm::RationalFunctionCoefficient, storm::IntervalBaseType<ValueType>>;
    auto toBaseType = [](ValueType const& value) {
        if constexpr (std::is_same_v<ValueType, BaseType>) {
            return value;
        } else {
            return storm::utility::convertNumber<BaseType>(value);
        }
    };
    STORM_LOG_ASSERT(storm::utility::isConstant(tolerance), "Expected constant tolerance. Got " << tolerance);
    BaseType const zeroMinusTolerance = storm::utility::zero<BaseType>() - toBaseType(tolerance);
    BaseType const onePlusTolerance = storm::utility::one<BaseType>() + toBaseType(tolerance);
    BaseType const oneMinusTolerance = storm::utility::one<BaseType>() - toBaseType(tolerance);

    auto isContained = [&toBaseType](ValueType const& value, BaseType const& lower, BaseType const& upper) {
        // surpress unused lambda capture warning for toBaseType in case it is not needed for the given ValueType.
        (void)toBaseType;
        if constexpr (storm::IsIntervalType<ValueType>) {
            // check if the interval contains some value in [lower,upper]
            return value.lower() <= upper && value.upper() >= lower;
        } else if constexpr (std::is_same_v<ValueType, storm::RationalFunction>) {
            // for rational functions, we only perform a check if the value is constant.
            if (storm::utility::isConstant(value)) {
                auto const constValue = toBaseType(value);
                return constValue <= upper && constValue >= lower;
            }
            return true;
        } else {
            // in all other cases, we expect the value to be constant
            STORM_LOG_ASSERT(storm::utility::isConstant(value), "Expected constant value. Got " << value);
            return value <= upper && value >= lower;
        }
    };

    auto toString = [](ValueType const& value) {
        std::stringstream s;
        s << value;
        return s.str();
    };

    for (index_type row = 0; row < this->rowCount; ++row) {
        auto rowSum = storm::utility::zero<ValueType>();
        for (auto const& entry : getRow(row)) {
            if (!isContained(entry.getValue(), zeroMinusTolerance, onePlusTolerance)) {
                if (reason) {
                    *reason = "Entry in row " + std::to_string(row) + " is not a probability: " + toString(entry.getValue());
                }
                return false;
            }
            rowSum += entry.getValue();
        }
        if (!isContained(rowSum, oneMinusTolerance, onePlusTolerance)) {
            if (reason) {
                // print sum-1 to ensure that the reason is informative even if the sum is very close to one.
                *reason = "Sum of entries in row " + std::to_string(row) + " is not one: sum-1=" + toString(rowSum - storm::utility::one<ValueType>());
            }
            return false;
        }
    }
    return true;
}

template<typename ValueType>
bool SparseMatrix<ValueType>::hasOnlyPositiveEntries() const {
    for (auto const& entry : *this) {
        if (!storm::utility::isPositive(entry.getValue())) {
            return false;
        }
    }
    return true;
}

template<typename ValueType>
template<typename OtherValueType>
bool SparseMatrix<ValueType>::isSubmatrixOf(SparseMatrix<OtherValueType> const& matrix) const {
    // Check for matching sizes.
    if (this->getRowCount() != matrix.getRowCount())
        return false;
    if (this->getColumnCount() != matrix.getColumnCount())
        return false;
    if (this->hasTrivialRowGrouping() && !matrix.hasTrivialRowGrouping())
        return false;
    if (!this->hasTrivialRowGrouping() && matrix.hasTrivialRowGrouping())
        return false;
    if (!this->hasTrivialRowGrouping() && !matrix.hasTrivialRowGrouping() && this->getRowGroupIndices() != matrix.getRowGroupIndices())
        return false;
    if (this->getRowGroupIndices() != matrix.getRowGroupIndices())
        return false;

    // Check the subset property for all rows individually.
    for (index_type row = 0; row < this->getRowCount(); ++row) {
        auto it2 = matrix.begin(row);
        auto ite2 = matrix.end(row);
        for (const_iterator it1 = this->begin(row), ite1 = this->end(row); it1 != ite1; ++it1) {
            // Skip over all entries of the other matrix that are before the current entry in the current matrix.
            while (it2 != ite2 && it2->getColumn() < it1->getColumn()) {
                ++it2;
            }
            if (it2 == ite2 || it1->getColumn() != it2->getColumn()) {
                return false;
            }
        }
    }
    return true;
}

template<typename ValueType>
bool SparseMatrix<ValueType>::isIdentityMatrix() const {
    if (this->getRowCount() != this->getColumnCount()) {
        return false;
    }
    if (this->getNonzeroEntryCount() != this->getRowCount()) {
        return false;
    }
    for (uint64_t row = 0; row < this->getRowCount(); ++row) {
        bool rowHasEntry = false;
        for (auto const& entry : this->getRow(row)) {
            if (entry.getColumn() == row) {
                if (!storm::utility::isOne(entry.getValue())) {
                    return false;
                }
                rowHasEntry = true;
            } else {
                if (!storm::utility::isZero(entry.getValue())) {
                    return false;
                }
            }
        }
        if (!rowHasEntry) {
            return false;
        }
    }
    return true;
}

template<typename ValueType>
std::string SparseMatrix<ValueType>::getDimensionsAsString() const {
    std::string result =
        std::to_string(getRowCount()) + "x" + std::to_string(getColumnCount()) + " matrix (" + std::to_string(getNonzeroEntryCount()) + " non-zeroes";
    if (!hasTrivialRowGrouping()) {
        result += ", " + std::to_string(getRowGroupCount()) + " groups";
    }
    result += ")";
    return result;
}

template<typename ValueType>
std::ostream& operator<<(std::ostream& out, SparseMatrix<ValueType> const& matrix) {
    // Print column numbers in header.
    out << "\t\t";
    for (typename SparseMatrix<ValueType>::index_type i = 0; i < matrix.getColumnCount(); ++i) {
        out << i << "\t";
    }
    out << '\n';

    // Iterate over all row groups.
    for (typename SparseMatrix<ValueType>::index_type group = 0; group < matrix.getRowGroupCount(); ++group) {
        out << "\t---- group " << group << "/" << (matrix.getRowGroupCount() - 1) << " ---- \n";
        typename SparseMatrix<ValueType>::index_type start = matrix.hasTrivialRowGrouping() ? group : matrix.getRowGroupIndices()[group];
        typename SparseMatrix<ValueType>::index_type end = matrix.hasTrivialRowGrouping() ? group + 1 : matrix.getRowGroupIndices()[group + 1];

        for (typename SparseMatrix<ValueType>::index_type i = start; i < end; ++i) {
            typename SparseMatrix<ValueType>::index_type nextIndex = matrix.rowIndications[i];

            // Print the actual row.
            out << i << "\t(\t";
            typename SparseMatrix<ValueType>::index_type currentRealIndex = 0;
            while (currentRealIndex < matrix.columnCount) {
                if (nextIndex < matrix.rowIndications[i + 1] && currentRealIndex == matrix.columnsAndValues[nextIndex].getColumn()) {
                    out << matrix.columnsAndValues[nextIndex].getValue() << "\t";
                    ++nextIndex;
                } else {
                    out << "0\t";
                }
                ++currentRealIndex;
            }
            out << "\t)\t" << i << '\n';
        }
    }

    // Print column numbers in footer.
    out << "\t\t";
    for (typename SparseMatrix<ValueType>::index_type i = 0; i < matrix.getColumnCount(); ++i) {
        out << i << "\t";
    }
    out << '\n';

    return out;
}

template<typename ValueType>
void SparseMatrix<ValueType>::printAsMatlabMatrix(std::ostream& out) const {
    // Iterate over all row groups.
    for (typename SparseMatrix<ValueType>::index_type group = 0; group < this->getRowGroupCount(); ++group) {
        STORM_LOG_ASSERT(this->getRowGroupSize(group) == 1, "Incorrect row group size.");
        for (typename SparseMatrix<ValueType>::index_type i = this->getRowGroupIndices()[group]; i < this->getRowGroupIndices()[group + 1]; ++i) {
            typename SparseMatrix<ValueType>::index_type nextIndex = this->rowIndications[i];

            // Print the actual row.
            out << i << "\t(";
            typename SparseMatrix<ValueType>::index_type currentRealIndex = 0;
            while (currentRealIndex < this->columnCount) {
                if (nextIndex < this->rowIndications[i + 1] && currentRealIndex == this->columnsAndValues[nextIndex].getColumn()) {
                    out << this->columnsAndValues[nextIndex].getValue() << " ";
                    ++nextIndex;
                } else {
                    out << "0 ";
                }
                ++currentRealIndex;
            }
            out << ";\n";
        }
    }
}

template<typename ValueType>
std::size_t SparseMatrix<ValueType>::hash() const {
    std::size_t result = 0;

    boost::hash_combine(result, this->getRowCount());
    boost::hash_combine(result, this->getColumnCount());
    boost::hash_combine(result, this->getEntryCount());
    boost::hash_combine(result, boost::hash_range(columnsAndValues.begin(), columnsAndValues.end()));
    boost::hash_combine(result, boost::hash_range(rowIndications.begin(), rowIndications.end()));
    if (!this->hasTrivialRowGrouping()) {
        boost::hash_combine(result, boost::hash_range(rowGroupIndices.get().begin(), rowGroupIndices.get().end()));
    }

    return result;
}

// Explicitly instantiate the entry, builder and the matrix.
// double
template class MatrixEntry<typename SparseMatrix<double>::index_type, double>;
template std::ostream& operator<<(std::ostream& out, MatrixEntry<typename SparseMatrix<double>::index_type, double> const& entry);
template class SparseMatrixBuilder<double>;
template class SparseMatrix<double>;
template std::ostream& operator<<(std::ostream& out, SparseMatrix<double> const& matrix);
template double SparseMatrix<double>::getPointwiseProductRowSum(storm::storage::SparseMatrix<double> const& otherMatrix,
                                                                typename SparseMatrix<double>::index_type const& row) const;
template std::vector<double> SparseMatrix<double>::getPointwiseProductRowSumVector(storm::storage::SparseMatrix<double> const& otherMatrix) const;
template bool SparseMatrix<double>::isSubmatrixOf(SparseMatrix<double> const& matrix) const;

template class MatrixEntry<uint32_t, double>;
template std::ostream& operator<<(std::ostream& out, MatrixEntry<uint32_t, double> const& entry);

// int
template class MatrixEntry<typename SparseMatrix<int>::index_type, int>;
template std::ostream& operator<<(std::ostream& out, MatrixEntry<typename SparseMatrix<int>::index_type, int> const& entry);
template class SparseMatrixBuilder<int>;
template class SparseMatrix<int>;
template std::ostream& operator<<(std::ostream& out, SparseMatrix<int> const& matrix);
template bool SparseMatrix<int>::isSubmatrixOf(SparseMatrix<int> const& matrix) const;

// state_type
template class MatrixEntry<typename SparseMatrix<storm::storage::sparse::state_type>::index_type, storm::storage::sparse::state_type>;
template std::ostream& operator<<(
    std::ostream& out, MatrixEntry<typename SparseMatrix<storm::storage::sparse::state_type>::index_type, storm::storage::sparse::state_type> const& entry);
template class SparseMatrixBuilder<storm::storage::sparse::state_type>;
template class SparseMatrix<storm::storage::sparse::state_type>;
template std::ostream& operator<<(std::ostream& out, SparseMatrix<storm::storage::sparse::state_type> const& matrix);
template bool SparseMatrix<int>::isSubmatrixOf(SparseMatrix<storm::storage::sparse::state_type> const& matrix) const;

// Rational Numbers

#if defined(STORM_HAVE_CLN)
template class MatrixEntry<typename SparseMatrix<ClnRationalNumber>::index_type, ClnRationalNumber>;
template std::ostream& operator<<(std::ostream& out, MatrixEntry<typename SparseMatrix<ClnRationalNumber>::index_type, ClnRationalNumber> const& entry);
template class SparseMatrixBuilder<ClnRationalNumber>;
template class SparseMatrix<ClnRationalNumber>;
template std::ostream& operator<<(std::ostream& out, SparseMatrix<ClnRationalNumber> const& matrix);
template storm::ClnRationalNumber SparseMatrix<storm::ClnRationalNumber>::getPointwiseProductRowSum(
    storm::storage::SparseMatrix<storm::ClnRationalNumber> const& otherMatrix, typename SparseMatrix<storm::ClnRationalNumber>::index_type const& row) const;
template std::vector<storm::ClnRationalNumber> SparseMatrix<ClnRationalNumber>::getPointwiseProductRowSumVector(
    storm::storage::SparseMatrix<storm::ClnRationalNumber> const& otherMatrix) const;
template bool SparseMatrix<storm::ClnRationalNumber>::isSubmatrixOf(SparseMatrix<storm::ClnRationalNumber> const& matrix) const;
#endif

#if defined(STORM_HAVE_GMP)
template class MatrixEntry<typename SparseMatrix<GmpRationalNumber>::index_type, GmpRationalNumber>;
template std::ostream& operator<<(std::ostream& out, MatrixEntry<typename SparseMatrix<GmpRationalNumber>::index_type, GmpRationalNumber> const& entry);
template class SparseMatrixBuilder<GmpRationalNumber>;
template class SparseMatrix<GmpRationalNumber>;
template std::ostream& operator<<(std::ostream& out, SparseMatrix<GmpRationalNumber> const& matrix);
template storm::GmpRationalNumber SparseMatrix<storm::GmpRationalNumber>::getPointwiseProductRowSum(
    storm::storage::SparseMatrix<storm::GmpRationalNumber> const& otherMatrix, typename SparseMatrix<storm::GmpRationalNumber>::index_type const& row) const;
template std::vector<storm::GmpRationalNumber> SparseMatrix<GmpRationalNumber>::getPointwiseProductRowSumVector(
    storm::storage::SparseMatrix<storm::GmpRationalNumber> const& otherMatrix) const;
template bool SparseMatrix<storm::GmpRationalNumber>::isSubmatrixOf(SparseMatrix<storm::GmpRationalNumber> const& matrix) const;
#endif

// Rational Function
template class MatrixEntry<typename SparseMatrix<RationalFunction>::index_type, RationalFunction>;
template std::ostream& operator<<(std::ostream& out, MatrixEntry<typename SparseMatrix<RationalFunction>::index_type, RationalFunction> const& entry);
template class SparseMatrixBuilder<RationalFunction>;
template class SparseMatrix<RationalFunction>;
template std::ostream& operator<<(std::ostream& out, SparseMatrix<RationalFunction> const& matrix);
template storm::RationalFunction SparseMatrix<storm::RationalFunction>::getPointwiseProductRowSum(
    storm::storage::SparseMatrix<storm::RationalFunction> const& otherMatrix, typename SparseMatrix<storm::RationalFunction>::index_type const& row) const;
template storm::RationalFunction SparseMatrix<double>::getPointwiseProductRowSum(storm::storage::SparseMatrix<storm::RationalFunction> const& otherMatrix,
                                                                                 typename SparseMatrix<storm::RationalFunction>::index_type const& row) const;
template storm::RationalFunction SparseMatrix<int>::getPointwiseProductRowSum(storm::storage::SparseMatrix<storm::RationalFunction> const& otherMatrix,
                                                                              typename SparseMatrix<storm::RationalFunction>::index_type const& row) const;
template std::vector<storm::RationalFunction> SparseMatrix<RationalFunction>::getPointwiseProductRowSumVector(
    storm::storage::SparseMatrix<storm::RationalFunction> const& otherMatrix) const;
template std::vector<storm::RationalFunction> SparseMatrix<double>::getPointwiseProductRowSumVector(
    storm::storage::SparseMatrix<storm::RationalFunction> const& otherMatrix) const;
template std::vector<storm::RationalFunction> SparseMatrix<int>::getPointwiseProductRowSumVector(
    storm::storage::SparseMatrix<storm::RationalFunction> const& otherMatrix) const;
template bool SparseMatrix<storm::RationalFunction>::isSubmatrixOf(SparseMatrix<storm::RationalFunction> const& matrix) const;

// Intervals
template std::vector<storm::Interval> SparseMatrix<double>::getPointwiseProductRowSumVector(
    storm::storage::SparseMatrix<storm::Interval> const& otherMatrix) const;
template class MatrixEntry<typename SparseMatrix<Interval>::index_type, Interval>;
template std::ostream& operator<<(std::ostream& out, MatrixEntry<typename SparseMatrix<Interval>::index_type, Interval> const& entry);
template class SparseMatrixBuilder<Interval>;
template class SparseMatrix<Interval>;
template std::ostream& operator<<(std::ostream& out, SparseMatrix<Interval> const& matrix);
template std::vector<storm::Interval> SparseMatrix<Interval>::getPointwiseProductRowSumVector(
    storm::storage::SparseMatrix<storm::Interval> const& otherMatrix) const;
template bool SparseMatrix<storm::Interval>::isSubmatrixOf(SparseMatrix<storm::Interval> const& matrix) const;

template bool SparseMatrix<storm::Interval>::isSubmatrixOf(SparseMatrix<double> const& matrix) const;

// Rational Intervals
template std::vector<storm::RationalInterval> SparseMatrix<storm::RationalNumber>::getPointwiseProductRowSumVector(
    storm::storage::SparseMatrix<storm::RationalInterval> const& otherMatrix) const;
template class MatrixEntry<typename SparseMatrix<RationalInterval>::index_type, RationalInterval>;
template std::ostream& operator<<(std::ostream& out, MatrixEntry<typename SparseMatrix<RationalInterval>::index_type, RationalInterval> const& entry);
template class SparseMatrixBuilder<RationalInterval>;
template class SparseMatrix<RationalInterval>;
template std::ostream& operator<<(std::ostream& out, SparseMatrix<RationalInterval> const& matrix);
template std::vector<storm::RationalInterval> SparseMatrix<RationalInterval>::getPointwiseProductRowSumVector(
    storm::storage::SparseMatrix<storm::RationalInterval> const& otherMatrix) const;
template bool SparseMatrix<storm::RationalInterval>::isSubmatrixOf(SparseMatrix<storm::RationalInterval> const& matrix) const;

template bool SparseMatrix<storm::RationalInterval>::isSubmatrixOf(SparseMatrix<storm::RationalNumber> const& matrix) const;

}  // namespace storage
}  // namespace storm
