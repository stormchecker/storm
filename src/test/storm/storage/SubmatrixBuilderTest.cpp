#include "storm-config.h"
#include "test/storm_gtest.h"

#include <random>
#include <vector>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/SubmatrixBuilder.h"

namespace {

storm::storage::SparseMatrix<double> createRandomMatrix(std::mt19937_64& rng, uint64_t groups, uint64_t columns, bool grouped, double density) {
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    std::uniform_int_distribution<uint64_t> groupSize(1, 3);
    storm::storage::SparseMatrixBuilder<double> builder(0, columns, 0, true, grouped);
    uint64_t row = 0;
    for (uint64_t g = 0; g < groups; ++g) {
        uint64_t const size = grouped ? groupSize(rng) : 1;
        if (grouped) {
            builder.newRowGroup(row);
        }
        for (uint64_t i = 0; i < size; ++i, ++row) {
            for (uint64_t c = 0; c < columns; ++c) {
                if (coin(rng) < density) {
                    builder.addNextValue(row, c, coin(rng) + 0.5);
                }
            }
        }
    }
    return builder.build(row, columns, grouped ? groups : 0);
}

storm::storage::BitVector randomBitVector(std::mt19937_64& rng, uint64_t size, double p) {
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    storm::storage::BitVector result(size);
    for (uint64_t i = 0; i < size; ++i) {
        result.set(i, coin(rng) < p);
    }
    if (result.empty()) {
        result.set(rng() % size);
    }
    return result;
}

// Straightforward reference implementation (the former implementation of SparseMatrix::getSubmatrix).
storm::storage::SparseMatrix<double> referenceSubmatrix(storm::storage::SparseMatrix<double> const& matrix, bool useGroups,
                                                        storm::storage::BitVector const& rowConstraint, storm::storage::BitVector const& columnConstraint,
                                                        bool insertDiagonalEntries, storm::storage::BitVector const& makeZeroColumns) {
    using index_type = uint64_t;
    std::vector<index_type> rowGroupIndices;
    if (useGroups) {
        rowGroupIndices = matrix.getRowGroupIndices();
    } else {
        for (index_type i = 0; i <= matrix.getRowCount(); ++i) {
            rowGroupIndices.push_back(i);
        }
    }
    index_type const submatrixColumnCount = columnConstraint.getNumberOfSetBits();
    std::vector<index_type> columnBitsSetBeforeIndex = columnConstraint.getNumberOfSetBitsBeforeIndices();
    std::vector<index_type> rowBitsSetBeforeIndex = rowConstraint.getNumberOfSetBitsBeforeIndices();
    auto keep = [&](index_type col) { return columnConstraint.get(col) && (makeZeroColumns.size() == 0 || !makeZeroColumns.get(col)); };

    index_type subEntries = 0, subRows = 0, rowGroupCount = 0;
    for (uint64_t index : rowConstraint) {
        subRows += rowGroupIndices[index + 1] - rowGroupIndices[index];
        for (index_type i = rowGroupIndices[index]; i < rowGroupIndices[index + 1]; ++i) {
            bool foundDiagonalElement = false;
            for (auto it = matrix.begin(i); it != matrix.end(i); ++it) {
                if (keep(it->getColumn())) {
                    ++subEntries;
                    if (columnBitsSetBeforeIndex[it->getColumn()] == rowBitsSetBeforeIndex[index]) {
                        foundDiagonalElement = true;
                    }
                }
            }
            if (insertDiagonalEntries && !foundDiagonalElement && rowGroupCount < submatrixColumnCount) {
                ++subEntries;
            }
        }
        ++rowGroupCount;
    }
    bool const groupedResult = !matrix.hasTrivialRowGrouping();
    storm::storage::SparseMatrixBuilder<double> builder(subRows, submatrixColumnCount, subEntries, true, groupedResult);
    rowGroupCount = 0;
    index_type rowCount = 0;
    for (uint64_t index : rowConstraint) {
        if (groupedResult) {
            builder.newRowGroup(rowCount);
        }
        for (index_type i = rowGroupIndices[index]; i < rowGroupIndices[index + 1]; ++i) {
            bool insertedDiagonalElement = false;
            for (auto it = matrix.begin(i); it != matrix.end(i); ++it) {
                if (keep(it->getColumn())) {
                    if (columnBitsSetBeforeIndex[it->getColumn()] == rowBitsSetBeforeIndex[index]) {
                        insertedDiagonalElement = true;
                    } else if (insertDiagonalEntries && !insertedDiagonalElement && columnBitsSetBeforeIndex[it->getColumn()] > rowBitsSetBeforeIndex[index]) {
                        builder.addNextValue(rowCount, rowGroupCount, 0.0);
                        insertedDiagonalElement = true;
                    }
                    builder.addNextValue(rowCount, columnBitsSetBeforeIndex[it->getColumn()], it->getValue());
                }
            }
            if (insertDiagonalEntries && !insertedDiagonalElement && rowGroupCount < submatrixColumnCount) {
                builder.addNextValue(rowCount, rowGroupCount, 0.0);
            }
            ++rowCount;
        }
        ++rowGroupCount;
    }
    auto result = builder.build();
    if (groupedResult && !useGroups) {
        // Row groups of the result: one group for each original group that has at least one selected row.
        std::vector<index_type> newGroups{0};
        auto selectedRowIt = rowConstraint.begin();
        for (index_type group = 0; group < matrix.getRowGroupCount(); ++group) {
            index_type newRowCount = 0;
            while (selectedRowIt != rowConstraint.end() && *selectedRowIt < matrix.getRowGroupIndices()[group + 1]) {
                ++selectedRowIt;
                ++newRowCount;
            }
            if (newRowCount > 0) {
                newGroups.push_back(newGroups.back() + newRowCount);
            }
        }
        result.setRowGroupIndices(newGroups);
    }
    return result;
}

void expectEqual(storm::storage::SparseMatrix<double> const& expected, storm::storage::SparseMatrix<double> const& actual) {
    ASSERT_EQ(expected.getRowCount(), actual.getRowCount());
    ASSERT_EQ(expected.getColumnCount(), actual.getColumnCount());
    ASSERT_EQ(expected.getEntryCount(), actual.getEntryCount());
    ASSERT_EQ(expected.hasTrivialRowGrouping(), actual.hasTrivialRowGrouping());
    if (!expected.hasTrivialRowGrouping()) {
        ASSERT_EQ(expected.getRowGroupIndices(), actual.getRowGroupIndices());
    }
    for (uint64_t row = 0; row < expected.getRowCount(); ++row) {
        ASSERT_EQ(expected.getRow(row).getNumberOfEntries(), actual.getRow(row).getNumberOfEntries());
        auto it = actual.begin(row);
        for (auto const& entry : expected.getRow(row)) {
            EXPECT_EQ(entry.getColumn(), it->getColumn());
            EXPECT_EQ(entry.getValue(), it->getValue());
            ++it;
        }
    }
}

}  // namespace

TEST(SubmatrixBuilderTest, AgreesWithGetSubmatrix) {
    std::mt19937_64 rng(42);
    for (bool grouped : {false, true}) {
        for (int iteration = 0; iteration < 200; ++iteration) {
            uint64_t const n = 1 + rng() % 12;
            auto matrix = createRandomMatrix(rng, n, n, grouped, 0.4);
            storm::storage::SubmatrixBuilder<double> builder(matrix);
            // Reuse the builder to also check that the internal state is reset properly.
            for (int j = 0; j < 5; ++j) {
                auto const groupConstraint = randomBitVector(rng, matrix.getRowGroupCount(), 0.6);
                auto const rowConstraint = randomBitVector(rng, matrix.getRowCount(), 0.6);
                auto const columnConstraint = randomBitVector(rng, matrix.getColumnCount(), 0.6);
                auto const zeroColumns = randomBitVector(rng, matrix.getColumnCount(), 0.3);
                for (bool diagonal : {false, true}) {
                    expectEqual(referenceSubmatrix(matrix, true, groupConstraint, columnConstraint, diagonal, {}),
                                builder.getByRowGroupConstraint(groupConstraint, columnConstraint, diagonal));
                    expectEqual(referenceSubmatrix(matrix, true, groupConstraint, columnConstraint, diagonal, zeroColumns),
                                builder.getByRowGroupConstraint(groupConstraint, columnConstraint, diagonal, zeroColumns));
                    expectEqual(referenceSubmatrix(matrix, false, rowConstraint, columnConstraint, diagonal, {}),
                                builder.getByRowConstraint(rowConstraint, columnConstraint, diagonal));
                    expectEqual(referenceSubmatrix(matrix, false, rowConstraint, columnConstraint, diagonal, zeroColumns),
                                builder.getByRowConstraint(rowConstraint, columnConstraint, diagonal, zeroColumns));
                }
            }
        }
    }
}

TEST(SubmatrixBuilderTest, VectorConstraints) {
    std::mt19937_64 rng(7);
    auto matrix = createRandomMatrix(rng, 10, 10, true, 0.5);
    storm::storage::SubmatrixBuilder<double> builder(matrix);
    storm::storage::BitVector groups(10, std::vector<uint64_t>{1, 2, 5, 9});
    storm::storage::BitVector columns(10, std::vector<uint64_t>{0, 2, 3, 9});
    std::vector<uint64_t> groupVector(groups.begin(), groups.end());
    std::vector<uint64_t> columnVector(columns.begin(), columns.end());
    expectEqual(builder.getByRowGroupConstraint(groups, columns, true), builder.getByRowGroupConstraint(groupVector, columnVector, true));
}

TEST(SubmatrixBuilderTest, EmptyConstraintThrows) {
    std::mt19937_64 rng(1);
    auto matrix = createRandomMatrix(rng, 4, 4, false, 0.5);
    storm::storage::SubmatrixBuilder<double> builder(matrix);
    storm::storage::BitVector empty(4);
    storm::storage::BitVector some(4, true);
    STORM_SILENT_EXPECT_THROW(builder.getByRowConstraint(empty, some, false), storm::exceptions::InvalidArgumentException);
    STORM_SILENT_EXPECT_THROW(builder.getByRowConstraint(some, empty, false), storm::exceptions::InvalidArgumentException);
    // The builder is still usable afterwards
    expectEqual(referenceSubmatrix(matrix, false, some, some, false, {}), builder.getByRowConstraint(some, some, false));
}
