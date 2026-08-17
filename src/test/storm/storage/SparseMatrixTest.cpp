#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/permutation.h"

TEST(SparseMatrixBuilder, CreationEmpty) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder;
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    ASSERT_EQ(0ul, matrix.getRowCount());
    ASSERT_EQ(0ul, matrix.getColumnCount());
    ASSERT_EQ(0ul, matrix.getEntryCount());
}

TEST(SparseMatrixBuilder, CreationWithDimensions) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(3, 4, 5);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 3, 0.2));

    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    ASSERT_EQ(3ul, matrix.getRowCount());
    ASSERT_EQ(4ul, matrix.getColumnCount());
    ASSERT_EQ(5ul, matrix.getEntryCount());
}

TEST(SparseMatrixBuilder, CreationWithoutNumberOfEntries) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(3, 4);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 3, 0.2));

    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    ASSERT_EQ(3ul, matrix.getRowCount());
    ASSERT_EQ(4ul, matrix.getColumnCount());
    ASSERT_EQ(5ul, matrix.getEntryCount());
}

TEST(SparseMatrixBuilder, CreationWithNumberOfRows) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(3);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 3, 0.2));

    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    ASSERT_EQ(3ul, matrix.getRowCount());
    ASSERT_EQ(4ul, matrix.getColumnCount());
    ASSERT_EQ(5ul, matrix.getEntryCount());
}

TEST(SparseMatrixBuilder, CreationWithoutDimensions) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder;
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 3, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 2, 0.2));

    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    ASSERT_EQ(2ul, matrix.getRowCount());
    ASSERT_EQ(4ul, matrix.getColumnCount());
    ASSERT_EQ(5ul, matrix.getEntryCount());
}

TEST(SparseMatrixBuilder, AddNextValue) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder1(3, 4, 5);
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(0, 2, 1.2));
    STORM_SILENT_ASSERT_THROW(matrixBuilder1.addNextValue(0, 4, 0.5), storm::exceptions::OutOfRangeException);
    STORM_SILENT_ASSERT_THROW(matrixBuilder1.addNextValue(3, 1, 0.5), storm::exceptions::OutOfRangeException);

    storm::storage::SparseMatrixBuilder<double> matrixBuilder2(3, 4);
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 2, 1.2));
    STORM_SILENT_ASSERT_THROW(matrixBuilder2.addNextValue(0, 4, 0.5), storm::exceptions::OutOfRangeException);
    STORM_SILENT_ASSERT_THROW(matrixBuilder2.addNextValue(3, 1, 0.5), storm::exceptions::OutOfRangeException);

    storm::storage::SparseMatrixBuilder<double> matrixBuilder3(3);
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(1, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(2, 4, 0.5));
    STORM_SILENT_ASSERT_THROW(matrixBuilder3.addNextValue(3, 1, 0.2), storm::exceptions::OutOfRangeException);

    storm::storage::SparseMatrixBuilder<double> matrixBuilder4;
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(1, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(2, 4, 0.5));
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(3, 1, 0.2));
}

TEST(SparseMatrix, Build) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder1(3, 4, 5);
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(1, 3, 0.2));
    ASSERT_NO_THROW(matrixBuilder1.build());

    storm::storage::SparseMatrixBuilder<double> matrixBuilder2(3, 4, 5);
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 1, 0.7));
    STORM_SILENT_ASSERT_THROW(matrixBuilder2.build(), storm::exceptions::InvalidStateException);

    storm::storage::SparseMatrixBuilder<double> matrixBuilder3;
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(1, 3, 0.2));
    storm::storage::SparseMatrix<double> matrix3;
    ASSERT_NO_THROW(matrix3 = matrixBuilder3.build());
    ASSERT_EQ(2ul, matrix3.getRowCount());
    ASSERT_EQ(4ul, matrix3.getColumnCount());
    ASSERT_EQ(5ul, matrix3.getEntryCount());

    storm::storage::SparseMatrixBuilder<double> matrixBuilder4;
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(1, 3, 0.2));
    storm::storage::SparseMatrix<double> matrix4;
    ASSERT_NO_THROW(matrix4 = matrixBuilder4.build(4));
    ASSERT_EQ(4ul, matrix4.getRowCount());
    ASSERT_EQ(4ul, matrix4.getColumnCount());
    ASSERT_EQ(5ul, matrix4.getEntryCount());

    storm::storage::SparseMatrixBuilder<double> matrixBuilder5;
    ASSERT_NO_THROW(matrixBuilder5.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder5.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder5.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder5.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder5.addNextValue(1, 3, 0.2));
    storm::storage::SparseMatrix<double> matrix5;
    ASSERT_NO_THROW(matrix5 = matrixBuilder5.build(0, 6));
    ASSERT_EQ(2ul, matrix5.getRowCount());
    ASSERT_EQ(6ul, matrix5.getColumnCount());
    ASSERT_EQ(5ul, matrix5.getEntryCount());
}

TEST(SparseMatrix, DiagonalEntries) {
    {
        // No row groupings
        storm::storage::SparseMatrixBuilder<double> builder(4, 4, 7);
        storm::storage::SparseMatrixBuilder<double> builderCmp(4, 4, 7);
        for (uint64_t i = 0; i < 4; ++i) {
            ASSERT_NO_THROW(builder.addDiagonalEntry(i, i));
            ASSERT_NO_THROW(builder.addNextValue(i, 2, 100.0 + i));
            if (i < 2) {
                ASSERT_NO_THROW(builderCmp.addNextValue(i, i, i));
                ASSERT_NO_THROW(builderCmp.addNextValue(i, 2, 100.0 + i));
            } else {
                ASSERT_NO_THROW(builderCmp.addNextValue(i, 2, 100.0 + i));
                ASSERT_NO_THROW(builderCmp.addNextValue(i, i, i));
            }
        }
        auto matrix = builder.build();
        auto matrixCmp = builderCmp.build();
        EXPECT_EQ(matrix, matrixCmp);
    }
    {
        // With row groupings (each row group has 3 rows)
        storm::storage::SparseMatrixBuilder<double> builder(12, 4, 21, true, true, 4);
        storm::storage::SparseMatrixBuilder<double> builderCmp(12, 4, 21, true, true, 4);
        for (uint64_t i = 0; i < 4; ++i) {
            uint64_t row = 3 * i;
            builder.newRowGroup(row);
            builderCmp.newRowGroup(row);
            for (; row < 3 * (i + 1); ++row) {
                ASSERT_NO_THROW(builder.addDiagonalEntry(row, row));
                ASSERT_NO_THROW(builder.addNextValue(row, 2, 100 + row));
                if (i < 2) {
                    ASSERT_NO_THROW(builderCmp.addNextValue(row, i, row));
                    ASSERT_NO_THROW(builderCmp.addNextValue(row, 2, 100.0 + row));
                } else {
                    ASSERT_NO_THROW(builderCmp.addNextValue(row, 2, 100.0 + row));
                    ASSERT_NO_THROW(builderCmp.addNextValue(row, i, row));
                }
            }
        }
        auto matrix = builder.build();
        auto matrixCmp = builderCmp.build();
        EXPECT_EQ(matrix, matrixCmp);
    }
    {
        // With row groupings (every second row is empty)
        storm::storage::SparseMatrixBuilder<double> builder(12, 4, 10, true, true, 4);
        storm::storage::SparseMatrixBuilder<double> builderCmp(12, 4, 10, true, true, 4);
        for (uint64_t i = 0; i < 4; ++i) {
            uint64_t row = 3 * i;
            builder.newRowGroup(row);
            builderCmp.newRowGroup(row);
            for (; row < 3 * (i + 1); ++row) {
                if (row % 2 == 1) {
                    continue;
                }
                ASSERT_NO_THROW(builder.addDiagonalEntry(row, row));
                ASSERT_NO_THROW(builder.addNextValue(row, 2, 100 + row));
                if (i < 2) {
                    ASSERT_NO_THROW(builderCmp.addNextValue(row, i, row));
                    ASSERT_NO_THROW(builderCmp.addNextValue(row, 2, 100.0 + row));
                } else {
                    ASSERT_NO_THROW(builderCmp.addNextValue(row, i, row));
                    ASSERT_NO_THROW(builderCmp.addNextValue(row, 2, 100.0 + row));
                }
            }
        }
        auto matrix = builder.build();
        auto matrixCmp = builderCmp.build();
        EXPECT_EQ(matrix, matrixCmp);
    }
}

TEST(SparseMatrix, CreationWithMovingContents) {
    std::vector<storm::storage::MatrixEntry<uint_fast64_t, double>> columnsAndValues;
    columnsAndValues.emplace_back(1, 1.0);
    columnsAndValues.emplace_back(2, 1.2);
    columnsAndValues.emplace_back(0, 0.5);
    columnsAndValues.emplace_back(1, 0.7);
    columnsAndValues.emplace_back(3, 0.2);

    ASSERT_NO_THROW(storm::storage::SparseMatrix<double> matrix(4, {0, 2, 5, 5}, columnsAndValues, boost::optional<std::vector<uint_fast64_t>>({0, 1, 2, 3})));
    storm::storage::SparseMatrix<double> matrix(4, {0, 2, 5, 5}, columnsAndValues, boost::optional<std::vector<uint_fast64_t>>({0, 1, 2, 3}));
    ASSERT_EQ(3ul, matrix.getRowCount());
    ASSERT_EQ(4ul, matrix.getColumnCount());
    ASSERT_EQ(5ul, matrix.getEntryCount());
    ASSERT_EQ(3ul, matrix.getRowGroupCount());
}

TEST(SparseMatrix, CopyConstruct) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder;
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 3, 0.2));

    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    ASSERT_NO_THROW(storm::storage::SparseMatrix<double> copy(matrix));
    storm::storage::SparseMatrix<double> copy(matrix);
    ASSERT_TRUE(matrix == copy);
}

TEST(SparseMatrix, CopyAssign) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder;
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 3, 0.2));

    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    ASSERT_NO_THROW(storm::storage::SparseMatrix<double> copy = matrix);
    storm::storage::SparseMatrix<double> copy = matrix;
    ASSERT_TRUE(matrix == copy);
}

TEST(SparseMatrix, MakeAbsorbing) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(3, 4, 5);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 3, 0.2));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    storm::storage::BitVector absorbingRows(3);
    absorbingRows.set(1);

    ASSERT_NO_THROW(matrix.makeRowsAbsorbing(absorbingRows));

    storm::storage::SparseMatrixBuilder<double> matrixBuilder2(3, 4, 3);
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 1, 1));

    storm::storage::SparseMatrix<double> matrix2;
    ASSERT_NO_THROW(matrix2 = matrixBuilder2.build());

    ASSERT_TRUE(matrix == matrix2);
}

TEST(SparseMatrix, MakeRowGroupAbsorbing) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(5, 4, 9, true, true);
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(4));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    storm::storage::BitVector absorbingRowGroups(3);
    absorbingRowGroups.set(1);

    ASSERT_NO_THROW(matrix.makeRowGroupsAbsorbing(absorbingRowGroups));

    storm::storage::SparseMatrixBuilder<double> matrixBuilder2(0, 0, 0, false, true);
    ASSERT_NO_THROW(matrixBuilder2.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder2.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(2, 1, 1));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(3, 1, 1));
    ASSERT_NO_THROW(matrixBuilder2.newRowGroup(4));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix2;
    ASSERT_NO_THROW(matrix2 = matrixBuilder2.build());

    ASSERT_TRUE(matrix == matrix2);
}

TEST(SparseMatrix, rowGroupIndices) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(5, 4, 9, true, true, 4);
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(4));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    EXPECT_EQ(4ul, matrix.getRowGroupCount());
    std::vector<storm::storage::SparseMatrixIndexType> expected, actual;
    expected.assign({0, 1});
    auto indices = matrix.getRowGroupIndices(0);
    actual.assign(indices.begin(), indices.end());
    EXPECT_EQ(expected, actual);
    expected.assign({2, 3});
    indices = matrix.getRowGroupIndices(1);
    actual.assign(indices.begin(), indices.end());
    EXPECT_EQ(expected, actual);
    expected.assign({4});
    indices = matrix.getRowGroupIndices(2);
    actual.assign(indices.begin(), indices.end());
    EXPECT_EQ(expected, actual);
    expected.assign({});
    indices = matrix.getRowGroupIndices(3);
    actual.assign(indices.begin(), indices.end());
    EXPECT_EQ(expected, actual);
}

TEST(SparseMatrix, ConstrainedRowSumVector) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(5, 4, 9);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    storm::storage::BitVector columnConstraint(4);
    columnConstraint.set(1);
    columnConstraint.set(3);

    ASSERT_NO_THROW(std::vector<double> constrainedRowSum = matrix.getConstrainedRowSumVector(storm::storage::BitVector(5, true), columnConstraint));
    std::vector<double> constrainedRowSum = matrix.getConstrainedRowSumVector(storm::storage::BitVector(5, true), columnConstraint);
    ASSERT_TRUE(constrainedRowSum == std::vector<double>({1.0, 0.7, 0, 0, 0.5}));

    storm::storage::SparseMatrixBuilder<double> matrixBuilder2(5, 4, 9, true, true);
    ASSERT_NO_THROW(matrixBuilder2.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder2.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(3, 3, 1.2));
    ASSERT_NO_THROW(matrixBuilder2.newRowGroup(4));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix2;
    ASSERT_NO_THROW(matrix2 = matrixBuilder2.build());

    storm::storage::BitVector rowGroupConstraint(3);
    rowGroupConstraint.set(1);

    storm::storage::BitVector columnConstraint2(4);
    columnConstraint2.set(2);
    columnConstraint2.set(3);

    ASSERT_NO_THROW(std::vector<double> constrainedRowSum2 = matrix2.getConstrainedRowGroupSumVector(rowGroupConstraint, columnConstraint2));
    std::vector<double> constrainedRowSum2 = matrix2.getConstrainedRowGroupSumVector(rowGroupConstraint, columnConstraint2);
    ASSERT_TRUE(constrainedRowSum2 == std::vector<double>({0, 2.3}));
}

TEST(SparseMatrix, Submatrix) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(5, 4, 9, true, true);
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(4));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    storm::storage::BitVector rowGroupConstraint(4);
    storm::storage::BitVector columnConstraint(4);

    STORM_SILENT_ASSERT_THROW(matrix.getSubmatrix(true, rowGroupConstraint, columnConstraint), storm::exceptions::InvalidArgumentException);

    std::vector<uint_fast64_t> rowGroupIndices = {0, 1, 2, 4, 5};

    rowGroupConstraint.set(2);
    rowGroupConstraint.set(3);
    columnConstraint.set(0);
    columnConstraint.set(3);

    ASSERT_NO_THROW(storm::storage::SparseMatrix<double> matrix2 = matrix.getSubmatrix(true, rowGroupConstraint, columnConstraint, false));
    storm::storage::SparseMatrix<double> matrix2 = matrix.getSubmatrix(true, rowGroupConstraint, columnConstraint, false);

    storm::storage::SparseMatrixBuilder<double> matrixBuilder3(3, 2, 3, true, true);
    ASSERT_NO_THROW(matrixBuilder3.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(0, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder3.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(2, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(2, 1, 0.3));
    storm::storage::SparseMatrix<double> matrix3;
    ASSERT_NO_THROW(matrix3 = matrixBuilder3.build());

    ASSERT_TRUE(matrix2 == matrix3);

    std::vector<uint_fast64_t> rowGroupToIndexMapping = {0, 0, 1, 0};

    ASSERT_NO_THROW(storm::storage::SparseMatrix<double> matrix4 = matrix.selectRowsFromRowGroups(rowGroupToIndexMapping));
    storm::storage::SparseMatrix<double> matrix4 = matrix.selectRowsFromRowGroups(rowGroupToIndexMapping);

    storm::storage::SparseMatrixBuilder<double> matrixBuilder5(4, 4, 8);
    ASSERT_NO_THROW(matrixBuilder5.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder5.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder5.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder5.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder5.addNextValue(2, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder5.addNextValue(3, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder5.addNextValue(3, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder5.addNextValue(3, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix5;
    ASSERT_NO_THROW(matrix5 = matrixBuilder5.build());

    ASSERT_TRUE(matrix4 == matrix5);
}

TEST(SparseMatrix, RestrictRows) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder1(7, 4, 9, true, true, 3);
    ASSERT_NO_THROW(matrixBuilder1.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder1.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder1.newRowGroup(4));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder1.addNextValue(6, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix1;
    ASSERT_NO_THROW(matrix1 = matrixBuilder1.build());

    storm::storage::BitVector constraint1(7);
    constraint1.set(0);
    constraint1.set(1);
    constraint1.set(2);
    constraint1.set(5);

    storm::storage::SparseMatrix<double> matrix1Prime;
    ASSERT_NO_THROW(matrix1Prime = matrix1.restrictRows(constraint1));

    storm::storage::SparseMatrixBuilder<double> matrixBuilder2(4, 4, 5, true, true, 3);
    ASSERT_NO_THROW(matrixBuilder2.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder2.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder2.newRowGroup(3));
    storm::storage::SparseMatrix<double> matrix2;
    ASSERT_NO_THROW(matrix2 = matrixBuilder2.build());

    ASSERT_EQ(matrix2, matrix1Prime);

    storm::storage::BitVector constraint2(4);
    constraint2.set(1);
    constraint2.set(2);

    storm::storage::SparseMatrix<double> matrix2Prime;
    STORM_SILENT_ASSERT_THROW(matrix2Prime = matrix2.restrictRows(constraint2), storm::exceptions::InvalidArgumentException);
    ASSERT_NO_THROW(matrix2Prime = matrix2.restrictRows(constraint2, true));

    storm::storage::SparseMatrixBuilder<double> matrixBuilder3(2, 4, 3, true, true, 3);
    ASSERT_NO_THROW(matrixBuilder3.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(0, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(0, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder3.newRowGroup(1));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(1, 0, 0.5));
    storm::storage::SparseMatrix<double> matrix3;
    ASSERT_NO_THROW(matrix3 = matrixBuilder3.build());

    ASSERT_EQ(matrix3, matrix2Prime);

    matrix3.makeRowGroupingTrivial();
    storm::storage::BitVector constraint3(2);
    constraint3.set(1);

    storm::storage::SparseMatrix<double> matrix3Prime;
    STORM_SILENT_ASSERT_THROW(matrix3Prime = matrix3.restrictRows(constraint3), storm::exceptions::InvalidArgumentException);
    ASSERT_NO_THROW(matrix3Prime = matrix3.restrictRows(constraint3, true));

    storm::storage::SparseMatrixBuilder<double> matrixBuilder4(1, 4, 1, true, true, 2);
    ASSERT_NO_THROW(matrixBuilder4.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder4.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(0, 0, 0.5));
    storm::storage::SparseMatrix<double> matrix4;
    ASSERT_NO_THROW(matrix4 = matrixBuilder4.build());

    ASSERT_EQ(matrix4, matrix3Prime);
}

TEST(SparseMatrix, Transpose) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(5, 4, 9);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    ASSERT_NO_THROW(storm::storage::SparseMatrix<double> transposeResult = matrix.transpose());
    storm::storage::SparseMatrix<double> transposeResult = matrix.transpose();

    storm::storage::SparseMatrixBuilder<double> matrixBuilder2(4, 5, 9);
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 1, 0.5));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 2, 0.5));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 4, 0.1));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 0, 1.0));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 4, 0.2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(2, 0, 1.2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(2, 3, 1.1));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(3, 4, 0.3));
    storm::storage::SparseMatrix<double> matrix2;
    ASSERT_NO_THROW(matrix2 = matrixBuilder2.build());

    ASSERT_TRUE(transposeResult == matrix2);
}

TEST(SparseMatrix, EquationSystem) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(4, 4, 7);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 0, 1.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 3, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 2, 0.99));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 3, 0.11));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    ASSERT_NO_THROW(matrix.convertToEquationSystem());

    storm::storage::SparseMatrixBuilder<double> matrixBuilder2(4, 4, 7);
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 0, 1 - 1.1));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 1, -1.2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 1, 1 - 0.5));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 3, -0.7));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(2, 0, -0.5));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(2, 2, 1 - 0.99));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(3, 3, 1 - 0.11));
    storm::storage::SparseMatrix<double> matrix2;
    ASSERT_NO_THROW(matrix2 = matrixBuilder2.build());

    ASSERT_TRUE(matrix == matrix2);
}

TEST(SparseMatrix, JacobiDecomposition) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(4, 4, 7);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 0, 1.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 3, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 2, 0.99));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 3, 0.11));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    ASSERT_NO_THROW(matrix.getJacobiDecomposition());
    std::pair<storm::storage::SparseMatrix<double>, std::vector<double>> jacobiDecomposition = matrix.getJacobiDecomposition();

    storm::storage::SparseMatrixBuilder<double> luBuilder(4, 4, 3);
    ASSERT_NO_THROW(luBuilder.addNextValue(0, 1, 1.2));
    ASSERT_NO_THROW(luBuilder.addNextValue(1, 3, 0.7));
    ASSERT_NO_THROW(luBuilder.addNextValue(2, 0, 0.5));
    storm::storage::SparseMatrix<double> lu;
    ASSERT_NO_THROW(lu = luBuilder.build());

    std::vector<double> dinv = {1 / 1.1, 1 / 0.5, 1 / 0.99, 1 / 0.11};
    ASSERT_TRUE(lu == jacobiDecomposition.first);
    ASSERT_TRUE(dinv == jacobiDecomposition.second);
}

TEST(SparseMatrix, PointwiseMultiplicationVector) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(5, 4, 9);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    storm::storage::SparseMatrixBuilder<double> matrixBuilder2(5, 4, 9);
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix2;
    ASSERT_NO_THROW(matrix2 = matrixBuilder2.build());

    ASSERT_NO_THROW(std::vector<double> pointwiseProductRowSums = matrix.getPointwiseProductRowSumVector(matrix2));
    std::vector<double> pointwiseProductRowSums = matrix.getPointwiseProductRowSumVector(matrix2);

    std::vector<double> correctResult = {1.0 * 1.0 + 1.2 * 1.2, 0.5 * 0.5 + 0.7 * 0.7, 0.5 * 0.5, 1.1 * 1.1, 0.1 * 0.1 + 0.2 * 0.2 + 0.3 * 0.3};
    ASSERT_TRUE(pointwiseProductRowSums == correctResult);
}

TEST(SparseMatrix, MatrixVectorMultiply) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(5, 4, 9);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    std::vector<double> x = {1, 0.3, 1.4, 7.1};
    std::vector<double> result(matrix.getRowCount());

    ASSERT_NO_THROW(matrix.multiplyWithVector(x, result));

    std::vector<double> correctResult = {1.0 * 0.3 + 1.2 * 1.4, 0.5 * 1 + 0.7 * 0.3, 0.5 * 1, 1.1 * 1.4, 0.1 * 1 + 0.2 * 0.3 + 0.3 * 7.1};

    for (std::size_t index = 0; index < correctResult.size(); ++index) {
        ASSERT_NEAR(result[index], correctResult[index], 1e-12);
    }
}

TEST(SparseMatrix, Iteration) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(5, 4, 9);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    for (auto const& entry : matrix.getRow(4)) {
        if (entry.getColumn() == 0) {
            ASSERT_EQ(0.1, entry.getValue());
        } else if (entry.getColumn() == 1) {
            ASSERT_EQ(0.2, entry.getValue());
        } else if (entry.getColumn() == 3) {
            ASSERT_EQ(0.3, entry.getValue());
        } else {
            ASSERT_TRUE(false);
        }
    }

    for (storm::storage::SparseMatrix<double>::iterator it = matrix.begin(4), ite = matrix.end(4); it != ite; ++it) {
        if (it->getColumn() == 0) {
            ASSERT_EQ(0.1, it->getValue());
        } else if (it->getColumn() == 1) {
            ASSERT_EQ(0.2, it->getValue());
        } else if (it->getColumn() == 3) {
            ASSERT_EQ(0.3, it->getValue());
        } else {
            ASSERT_TRUE(false);
        }
    }
}

TEST(SparseMatrix, RowSum) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(5, 4, 8);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    ASSERT_EQ(0, matrix.getRowSum(2));
    ASSERT_EQ(0.1 + 0.2 + 0.3, matrix.getRowSum(4));
}

TEST(SparseMatrix, IsSubmatrix) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(5, 4, 8);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    storm::storage::SparseMatrixBuilder<double> matrixBuilder2(5, 4, 5);
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(4, 1, 0.2));
    storm::storage::SparseMatrix<double> matrix2;
    ASSERT_NO_THROW(matrix2 = matrixBuilder2.build());

    ASSERT_TRUE(matrix2.isSubmatrixOf(matrix));

    storm::storage::SparseMatrixBuilder<double> matrixBuilder3(5, 4, 5);
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(0, 3, 1.0));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(4, 1, 0.2));
    storm::storage::SparseMatrix<double> matrix3;
    ASSERT_NO_THROW(matrix3 = matrixBuilder3.build());

    ASSERT_FALSE(matrix3.isSubmatrixOf(matrix));
    ASSERT_FALSE(matrix3.isSubmatrixOf(matrix2));
}

TEST(SparseMatrix, PermuteRows) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(5, 4, 8);
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 3, 0.3));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    std::vector<uint64_t> inversePermutation = {1, 4, 0, 3, 2};
    storm::storage::SparseMatrix<double> matrixperm = matrix.permuteRows(inversePermutation);
    EXPECT_EQ(5ul, matrixperm.getRowCount());
    EXPECT_EQ(4ul, matrixperm.getColumnCount());
    EXPECT_EQ(8ul, matrixperm.getEntryCount());
    EXPECT_EQ(matrix.getRowSum(1), matrixperm.getRowSum(0));
    EXPECT_EQ(matrix.getRowSum(4), matrixperm.getRowSum(1));
    EXPECT_EQ(matrix.getRowSum(0), matrixperm.getRowSum(2));
    EXPECT_EQ(matrix.getRowSum(3), matrixperm.getRowSum(3));
    EXPECT_EQ(matrix.getRowSum(2), matrixperm.getRowSum(4));
}

TEST(SparseMatrix, PermuteRowGroupsAndColumns) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(5, 4, 9, true, true, 4);
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));  // Group 0
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 0.7));
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 0, 0.5));  // Group 1
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 2, 1.1));
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(4));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 0, 0.1));  // Group 2
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(4, 3, 0.3));
    // Group 3 is empty
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    std::vector<uint64_t> permutation = {3, 0, 2, 1};
    std::vector<uint64_t> invertedPermutation = {1, 3, 2, 0};
    EXPECT_EQ(invertedPermutation, storm::utility::permutation::invertPermutation(permutation));
    auto matrixPerm = matrix.permuteRowGroupsAndColumns(invertedPermutation, permutation);

    storm::storage::SparseMatrixBuilder<double> expectedBuilder(5, 4, 9, true, true, 4);
    ASSERT_NO_THROW(expectedBuilder.newRowGroup(0));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(0, 3, 0.5));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(1, 2, 1.1));
    ASSERT_NO_THROW(expectedBuilder.newRowGroup(2));
    ASSERT_NO_THROW(expectedBuilder.newRowGroup(2));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(2, 3, 0.1));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(2, 0, 0.2));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(2, 1, 0.3));
    ASSERT_NO_THROW(expectedBuilder.newRowGroup(3));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(3, 0, 1.0));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(3, 2, 1.2));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(4, 3, 0.5));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(4, 0, 0.7));
    storm::storage::SparseMatrix<double> expectedMatrix;
    ASSERT_NO_THROW(expectedMatrix = expectedBuilder.build());
    EXPECT_EQ(expectedMatrix, matrixPerm);

    auto matrixPerm2 = matrixPerm.permuteRowGroupsAndColumns(permutation, invertedPermutation);
    EXPECT_EQ(matrix, matrixPerm2);
}

TEST(SparseMatrix, DropZeroEntries) {
    storm::storage::SparseMatrixBuilder<double> matrixBuilder(4, 3, 8, true, true);
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 1, 1.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(1, 2, 0.5));
    ASSERT_NO_THROW(matrixBuilder.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(2, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder.addNextValue(3, 2, 0.3));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = matrixBuilder.build());

    storm::storage::BitVector absorbingRows(3);
    absorbingRows.set(0);

    ASSERT_NO_THROW(matrix.makeRowsAbsorbing(absorbingRows, true));

    storm::storage::SparseMatrixBuilder<double> matrixBuilderX(4, 3, 8, true, true);
    ASSERT_NO_THROW(matrixBuilderX.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilderX.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilderX.addNextValue(0, 2, 1.2));
    ASSERT_NO_THROW(matrixBuilderX.newRowGroup(1));
    ASSERT_NO_THROW(matrixBuilderX.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilderX.addNextValue(1, 1, 1.1));
    ASSERT_NO_THROW(matrixBuilderX.addNextValue(1, 2, 0.5));
    ASSERT_NO_THROW(matrixBuilderX.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilderX.addNextValue(2, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilderX.addNextValue(3, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilderX.addNextValue(3, 2, 0.3));
    storm::storage::SparseMatrix<double> matrixX;
    ASSERT_NO_THROW(matrixX = matrixBuilderX.build());

    ASSERT_NO_THROW(matrixX.makeRowsAbsorbing(absorbingRows, false));

    storm::storage::SparseMatrixBuilder<double> matrixBuilder2(4, 3, 7, true, true);
    ASSERT_NO_THROW(matrixBuilder2.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(0, 0, 1.0));
    ASSERT_NO_THROW(matrixBuilder2.newRowGroup(1));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 0, 0.5));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 1, 1.1));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(1, 2, 0.5));
    ASSERT_NO_THROW(matrixBuilder2.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(2, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(3, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder2.addNextValue(3, 2, 0.3));
    storm::storage::SparseMatrix<double> matrix2;
    ASSERT_NO_THROW(matrix2 = matrixBuilder2.build());

    ASSERT_TRUE(matrix == matrix2);
    ASSERT_TRUE(matrix.getEntryCount() == matrix2.getEntryCount());

    ASSERT_TRUE(matrixX == matrix2);
    ASSERT_FALSE(matrixX.getEntryCount() == matrix2.getEntryCount());

    ASSERT_NO_THROW(matrix.makeRowDirac(1, 1, true));
    ASSERT_NO_THROW(matrixX.dropZeroEntries());
    ASSERT_NO_THROW(matrixX.makeRowDirac(1, 1, false));

    storm::storage::SparseMatrixBuilder<double> matrixBuilder3(4, 3, 5, true, true);
    ASSERT_NO_THROW(matrixBuilder3.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(0, 0, 1.0));
    ASSERT_NO_THROW(matrixBuilder3.newRowGroup(1));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(1, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder3.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(2, 0, 0.1));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(3, 1, 0.2));
    ASSERT_NO_THROW(matrixBuilder3.addNextValue(3, 2, 0.3));
    storm::storage::SparseMatrix<double> matrix3;
    ASSERT_NO_THROW(matrix3 = matrixBuilder3.build());

    ASSERT_TRUE(matrix == matrix3);
    ASSERT_TRUE(matrix.getEntryCount() == matrix3.getEntryCount());

    ASSERT_TRUE(matrixX == matrix3);
    ASSERT_FALSE(matrixX.getEntryCount() == matrix3.getEntryCount());

    storm::storage::BitVector absorbingRowGroups(3);
    absorbingRowGroups.set(2);

    ASSERT_NO_THROW(matrix.makeRowGroupsAbsorbing(absorbingRowGroups, true));
    ASSERT_NO_THROW(matrixX.dropZeroEntries());
    ASSERT_NO_THROW(matrixX.makeRowGroupsAbsorbing(absorbingRowGroups, false));

    storm::storage::SparseMatrixBuilder<double> matrixBuilder4(4, 3, 4, true, true);
    ASSERT_NO_THROW(matrixBuilder4.newRowGroup(0));
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(0, 0, 1.0));
    ASSERT_NO_THROW(matrixBuilder4.newRowGroup(1));
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(1, 1, 1.0));
    ASSERT_NO_THROW(matrixBuilder4.newRowGroup(2));
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(2, 2, 1.0));
    ASSERT_NO_THROW(matrixBuilder4.addNextValue(3, 2, 1.0));
    storm::storage::SparseMatrix<double> matrix4;
    ASSERT_NO_THROW(matrix4 = matrixBuilder4.build());

    ASSERT_TRUE(matrix == matrix4);
    ASSERT_TRUE(matrix.getEntryCount() == matrix4.getEntryCount());

    ASSERT_TRUE(matrixX == matrix4);
    ASSERT_FALSE(matrixX.getEntryCount() == matrix4.getEntryCount());
}

TEST(SparseMatrix, isProbabilistic) {
    storm::storage::SparseMatrixBuilder<double> builder(4, 4, 7, true);
    ASSERT_NO_THROW(builder.addNextValue(0, 1, storm::utility::sqrt(2.0)));
    ASSERT_NO_THROW(builder.addNextValue(0, 2, 1.0 - storm::utility::sqrt(2.0)));
    ASSERT_NO_THROW(builder.addNextValue(1, 2, storm::utility::sqrt(2.0)));
    ASSERT_NO_THROW(builder.addNextValue(1, 3, 1.0 - storm::utility::sqrt(2.0)));
    ASSERT_NO_THROW(builder.addNextValue(2, 0, storm::utility::sqrt(2.0)));
    ASSERT_NO_THROW(builder.addNextValue(2, 3, 1.0 - storm::utility::sqrt(2.0)));
    ASSERT_NO_THROW(builder.addNextValue(3, 3, 1.0));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = builder.build());
    std::string reason;
    ASSERT_FALSE(matrix.isProbabilistic(0.0, reason)) << reason;
    ASSERT_TRUE(matrix.isProbabilistic(0.5, reason)) << reason;
    for (auto& entry : matrix) {
        entry.setValue(storm::utility::abs(entry.getValue()) / 2.0);
    }
    ASSERT_FALSE(matrix.isProbabilistic(0, reason)) << reason;
}
