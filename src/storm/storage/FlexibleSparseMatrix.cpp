#include "storm/storage/FlexibleSparseMatrix.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {
template<typename ValueType>
FlexibleSparseMatrix<ValueType>::FlexibleSparseMatrix(index_type rows) : data(rows), columnCount(0), nonzeroEntryCount(0) {
    // Intentionally left empty.
}

template<typename ValueType>
FlexibleSparseMatrix<ValueType>::FlexibleSparseMatrix(storm::storage::SparseMatrix<ValueType> const& matrix, bool setAllValuesToOne, bool revertEquationSystem)
    : data(matrix.getRowCount()),
      columnCount(matrix.getColumnCount()),
      nonzeroEntryCount(matrix.getNonzeroEntryCount()),
      trivialRowGrouping(matrix.hasTrivialRowGrouping()) {
    STORM_LOG_THROW(!revertEquationSystem || trivialRowGrouping, storm::exceptions::InvalidArgumentException, "Illegal option for creating flexible matrix.");

    if (!trivialRowGrouping) {
        rowGroupIndices = matrix.getRowGroupIndices();
    }
    for (index_type rowIndex = 0; rowIndex < matrix.getRowCount(); ++rowIndex) {
        typename storm::storage::SparseMatrix<ValueType>::const_rows row = matrix.getRow(rowIndex);
        reserveInRow(rowIndex, row.getNumberOfEntries());
        for (auto const& element : row) {
            // If the probability is zero, we skip this entry.
            if (storm::utility::isZero(element.getValue())) {
                if (revertEquationSystem && rowIndex == element.getColumn()) {
                    getRow(rowIndex).emplace_back(element.getColumn(), storm::utility::one<ValueType>());
                } else {
                    continue;
                }
            }
            if (setAllValuesToOne) {
                if (revertEquationSystem && element.getColumn() == rowIndex && storm::utility::isOne(element.getValue())) {
                    continue;
                } else {
                    getRow(rowIndex).emplace_back(element.getColumn(), storm::utility::one<ValueType>());
                }
            } else {
                if (revertEquationSystem) {
                    if (element.getColumn() == rowIndex) {
                        if (storm::utility::isOne(element.getValue())) {
                            continue;
                        }
                        getRow(rowIndex).emplace_back(element.getColumn(), storm::utility::one<ValueType>() - element.getValue());
                    } else {
                        getRow(rowIndex).emplace_back(element.getColumn(), -element.getValue());
                    }
                } else {
                    getRow(rowIndex).emplace_back(element);
                }
            }
        }
    }
}

template<typename ValueType>
void FlexibleSparseMatrix<ValueType>::reserveInRow(index_type row, index_type numberOfElements) {
    this->data[row].reserve(numberOfElements);
}

template<typename ValueType>
typename FlexibleSparseMatrix<ValueType>::row_type& FlexibleSparseMatrix<ValueType>::getRow(index_type index) {
    return this->data[index];
}

template<typename ValueType>
typename FlexibleSparseMatrix<ValueType>::row_type const& FlexibleSparseMatrix<ValueType>::getRow(index_type index) const {
    return this->data[index];
}

template<typename ValueType>
typename FlexibleSparseMatrix<ValueType>::row_type& FlexibleSparseMatrix<ValueType>::getRow(index_type rowGroup, index_type offset) {
    STORM_LOG_ASSERT(rowGroup < this->getRowGroupCount(), "Invalid rowGroup.");
    STORM_LOG_ASSERT(offset < this->getRowGroupSize(rowGroup), "Invalid offset.");
    return getRow(rowGroupIndices[rowGroup] + offset);
}

template<typename ValueType>
typename FlexibleSparseMatrix<ValueType>::row_type const& FlexibleSparseMatrix<ValueType>::getRow(index_type rowGroup, index_type offset) const {
    STORM_LOG_ASSERT(rowGroup < this->getRowGroupCount(), "Invalid rowGroup.");
    STORM_LOG_ASSERT(offset < this->getRowGroupSize(rowGroup), "Invalid offset.");
    return getRow(rowGroupIndices[rowGroup] + offset);
}

template<typename ValueType>
std::vector<typename FlexibleSparseMatrix<ValueType>::index_type> const& FlexibleSparseMatrix<ValueType>::getRowGroupIndices() const {
    return rowGroupIndices;
}

template<typename ValueType>
typename FlexibleSparseMatrix<ValueType>::index_type FlexibleSparseMatrix<ValueType>::getRowCount() const {
    return this->data.size();
}

template<typename ValueType>
typename FlexibleSparseMatrix<ValueType>::index_type FlexibleSparseMatrix<ValueType>::getColumnCount() const {
    return columnCount;
}

template<typename ValueType>
typename FlexibleSparseMatrix<ValueType>::index_type FlexibleSparseMatrix<ValueType>::getNonzeroEntryCount() const {
    return nonzeroEntryCount;
}

template<typename ValueType>
typename FlexibleSparseMatrix<ValueType>::index_type FlexibleSparseMatrix<ValueType>::getRowGroupCount() const {
    return rowGroupIndices.size() - 1;
}

template<typename ValueType>
typename FlexibleSparseMatrix<ValueType>::index_type FlexibleSparseMatrix<ValueType>::getRowGroupSize(index_type group) const {
    return rowGroupIndices[group + 1] - rowGroupIndices[group];
}

template<typename ValueType>
ValueType FlexibleSparseMatrix<ValueType>::getRowSum(index_type row) const {
    ValueType sum = storm::utility::zero<ValueType>();
    for (auto const& element : getRow(row)) {
        sum += element.getValue();
    }
    return sum;
}

template<typename ValueType>
void FlexibleSparseMatrix<ValueType>::updateDimensions() {
    this->nonzeroEntryCount = 0;
    this->columnCount = 0;
    for (auto const& row : this->data) {
        for (auto const& element : row) {
            STORM_LOG_ASSERT(!storm::utility::isZero(element.getValue()), "Entry is 0.");
            ++this->nonzeroEntryCount;
            this->columnCount = std::max(element.getColumn() + 1, this->columnCount);
        }
    }
}

template<typename ValueType>
bool FlexibleSparseMatrix<ValueType>::empty() const {
    for (auto const& row : this->data) {
        if (!row.empty()) {
            return false;
        }
    }
    return true;
}

template<typename ValueType>
bool FlexibleSparseMatrix<ValueType>::hasTrivialRowGrouping() const {
    return trivialRowGrouping;
}

template<typename ValueType>
void FlexibleSparseMatrix<ValueType>::filterEntries(storm::storage::BitVector const& rowConstraint, storm::storage::BitVector const& columnConstraint) {
    for (uint_fast64_t rowIndex = 0; rowIndex < this->data.size(); ++rowIndex) {
        auto& row = this->data[rowIndex];
        if (!rowConstraint.get(rowIndex)) {
            row.clear();
            row.shrink_to_fit();
            continue;
        }
        row_type newRow;
        for (auto const& element : row) {
            if (columnConstraint.get(element.getColumn())) {
                newRow.push_back(element);
            }
        }
        row = std::move(newRow);
    }
}

template<typename ValueType>
typename FlexibleSparseMatrix<ValueType>::index_type FlexibleSparseMatrix<ValueType>::insertNewRowsAtEnd(
    typename FlexibleSparseMatrix<ValueType>::index_type numRows) {
    STORM_LOG_ERROR_COND(this->columnCount == this->getRowCount(), "insertNewRowsAtEnd only works when the FlexibleMatrix is square but column count is "
                                                                       << columnCount << " and row count is " << this->getRowCount());
    // ... because otherwise assumptions break when creating the SparseMatrix and we get weird entries for some reason
    index_type newRowsIndex = data.size();
    for (index_type i = 0; i < numRows; i++) {
        row_type newRow;
        this->data.push_back(newRow);
    }
    this->columnCount = getRowCount();
    return newRowsIndex;
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> FlexibleSparseMatrix<ValueType>::createSparseMatrix() {
    uint_fast64_t numEntries = 0;
    for (auto const& row : this->data) {
        numEntries += row.size();
    }

    storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder(getRowCount(), getColumnCount(), numEntries, hasTrivialRowGrouping(),
                                                                 hasTrivialRowGrouping() ? 0 : getRowGroupCount());
    uint_fast64_t currRowIndex = 0;
    auto rowGroupIndexIt = getRowGroupIndices().begin();
    for (auto const& row : this->data) {
        if (!hasTrivialRowGrouping()) {
            while (currRowIndex >= *rowGroupIndexIt) {
                matrixBuilder.newRowGroup(currRowIndex);
                ++rowGroupIndexIt;
            }
        }
        for (auto const& entry : row) {
            matrixBuilder.addNextValue(currRowIndex, entry.getColumn(), entry.getValue());
        }
        ++currRowIndex;
    }
    // The matrix might end with one or more empty row groups
    if (!hasTrivialRowGrouping()) {
        while (currRowIndex >= *rowGroupIndexIt) {
            matrixBuilder.newRowGroup(currRowIndex);
            ++rowGroupIndexIt;
        }
    }
    return matrixBuilder.build();
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> FlexibleSparseMatrix<ValueType>::createSparseMatrix(storm::storage::BitVector const& rowConstraint,
                                                                                            storm::storage::BitVector const& columnConstraint) {
    uint_fast64_t numEntries = 0;
    for (auto rowIndex : rowConstraint) {
        auto const& row = data[rowIndex];
        for (auto const& entry : row) {
            if (columnConstraint.get(entry.getColumn())) {
                ++numEntries;
            }
        }
    }
    uint_fast64_t numRowGroups = 0;
    if (!hasTrivialRowGrouping()) {
        auto lastRowGroupIndexIt = getRowGroupIndices().end() - 1;
        auto rowGroupIndexIt = getRowGroupIndices().begin();
        while (rowGroupIndexIt != lastRowGroupIndexIt) {
            // Check whether the rowGroup will be nonempty
            if (rowConstraint.getNextSetIndex(*rowGroupIndexIt) < *(++rowGroupIndexIt)) {
                ++numRowGroups;
            }
        }
    }

    std::vector<uint_fast64_t> oldToNewColumnIndexMapping(getColumnCount(), getColumnCount());
    uint_fast64_t newColumnIndex = 0;
    for (auto oldColumnIndex : columnConstraint) {
        oldToNewColumnIndexMapping[oldColumnIndex] = newColumnIndex++;
    }

    storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder(rowConstraint.getNumberOfSetBits(), newColumnIndex, numEntries, true, !hasTrivialRowGrouping(),
                                                                 numRowGroups);
    uint_fast64_t currRowIndex = 0;
    auto rowGroupIndexIt = getRowGroupIndices().begin();
    for (auto oldRowIndex : rowConstraint) {
        if (!hasTrivialRowGrouping() && oldRowIndex >= *rowGroupIndexIt) {
            matrixBuilder.newRowGroup(currRowIndex);
            // Skip empty row groups
            do {
                ++rowGroupIndexIt;
            } while (oldRowIndex >= *rowGroupIndexIt);
        }
        auto const& row = data[oldRowIndex];
        for (auto const& entry : row) {
            if (columnConstraint.get(entry.getColumn())) {
                matrixBuilder.addNextValue(currRowIndex, oldToNewColumnIndexMapping[entry.getColumn()], entry.getValue());
            }
        }
        ++currRowIndex;
    }
    return matrixBuilder.build();
}

template<typename ValueType>
bool FlexibleSparseMatrix<ValueType>::rowHasDiagonalElement(storm::storage::sparse::state_type state) {
    for (auto const& entry : this->getRow(state)) {
        if (entry.getColumn() < state) {
            continue;
        } else if (entry.getColumn() > state) {
            return false;
        } else if (entry.getColumn() == state) {
            return true;
        }
    }
    return false;
}

template<typename ValueType>
std::ostream& FlexibleSparseMatrix<ValueType>::printRow(std::ostream& out, index_type const& rowIndex) const {
    index_type columnIndex = 0;
    row_type row = this->getRow(rowIndex);
    for (index_type column = 0; column < this->getColumnCount(); ++column) {
        if (columnIndex < row.size() && row[columnIndex].getColumn() == column) {
            // Insert entry
            out << row[columnIndex].getValue() << "\t";
            ++columnIndex;
        } else {
            // Insert zero
            out << "0\t";
        }
    }
    return out;
}

template<typename ValueType>
std::ostream& operator<<(std::ostream& out, FlexibleSparseMatrix<ValueType> const& matrix) {
    typedef typename FlexibleSparseMatrix<ValueType>::index_type FlexibleIndex;

    // Print column numbers in header.
    out << "\t\t";
    for (FlexibleIndex i = 0; i < matrix.getColumnCount(); ++i) {
        out << i << "\t";
    }
    out << '\n';

    if (!matrix.hasTrivialRowGrouping()) {
        // Iterate over all row groups
        FlexibleIndex rowGroupCount = matrix.getRowGroupCount();
        for (FlexibleIndex rowGroup = 0; rowGroup < rowGroupCount; ++rowGroup) {
            out << "\t---- group " << rowGroup << "/" << (rowGroupCount - 1) << " ---- \n";
            FlexibleIndex endRow = matrix.rowGroupIndices[rowGroup + 1];
            // Iterate over all rows.
            for (FlexibleIndex row = matrix.rowGroupIndices[rowGroup]; row < endRow; ++row) {
                // Print the actual row.
                out << rowGroup << "\t(\t";
                matrix.printRow(out, row);
                out << "\t)\t" << rowGroup << '\n';
            }
        }

    } else {
        // Iterate over all rows
        for (FlexibleIndex row = 0; row < matrix.getRowCount(); ++row) {
            // Print the actual row.
            out << row << "\t(\t";
            matrix.printRow(out, row);
            out << "\t)\t" << row << '\n';
        }
    }

    // Print column numbers in footer.
    out << "\t\t";
    for (FlexibleIndex i = 0; i < matrix.getColumnCount(); ++i) {
        out << i << "\t";
    }
    out << '\n';
    return out;
}

// Explicitly instantiate the matrix.
template class FlexibleSparseMatrix<double>;
template std::ostream& operator<<(std::ostream& out, FlexibleSparseMatrix<double> const& matrix);

template class FlexibleSparseMatrix<storm::RationalNumber>;
template std::ostream& operator<<(std::ostream& out, FlexibleSparseMatrix<storm::RationalNumber> const& matrix);

template class FlexibleSparseMatrix<storm::RationalFunction>;
template std::ostream& operator<<(std::ostream& out, FlexibleSparseMatrix<storm::RationalFunction> const& matrix);

}  // namespace storage
}  // namespace storm
