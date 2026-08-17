#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/transformer/EndComponentEliminator.h"

TEST(NeutralECRemover, SimpleModelTest) {
    storm::storage::SparseMatrixBuilder<double> builder(12, 5, 19, true, true, 5);
    ASSERT_NO_THROW(builder.newRowGroup(0));
    ASSERT_NO_THROW(builder.addNextValue(0, 0, 1.0));
    ASSERT_NO_THROW(builder.addNextValue(1, 1, 0.3));
    ASSERT_NO_THROW(builder.addNextValue(1, 2, 0.1));
    ASSERT_NO_THROW(builder.addNextValue(1, 3, 0.4));
    ASSERT_NO_THROW(builder.addNextValue(1, 4, 0.2));
    ASSERT_NO_THROW(builder.newRowGroup(2));
    ASSERT_NO_THROW(builder.addNextValue(2, 1, 0.7));
    ASSERT_NO_THROW(builder.addNextValue(2, 3, 0.3));
    ASSERT_NO_THROW(builder.addNextValue(3, 1, 0.1));
    ASSERT_NO_THROW(builder.addNextValue(3, 4, 0.9));
    ASSERT_NO_THROW(builder.addNextValue(4, 1, 0.2));
    ASSERT_NO_THROW(builder.addNextValue(4, 4, 0.8));
    ASSERT_NO_THROW(builder.newRowGroup(5));
    ASSERT_NO_THROW(builder.addNextValue(5, 2, 1.0));
    ASSERT_NO_THROW(builder.newRowGroup(6));
    ASSERT_NO_THROW(builder.addNextValue(6, 1, 1.0));
    ASSERT_NO_THROW(builder.addNextValue(7, 2, 1.0));
    ASSERT_NO_THROW(builder.addNextValue(8, 3, 1.0));
    ASSERT_NO_THROW(builder.newRowGroup(9));
    ASSERT_NO_THROW(builder.addNextValue(9, 4, 1.0));
    ASSERT_NO_THROW(builder.addNextValue(10, 1, 0.4));
    ASSERT_NO_THROW(builder.addNextValue(10, 4, 0.6));
    ASSERT_NO_THROW(builder.addNextValue(11, 3, 1));
    storm::storage::SparseMatrix<double> matrix;
    ASSERT_NO_THROW(matrix = builder.build());

    storm::storage::BitVector subsystem(5, true);
    subsystem.set(2, false);

    storm::storage::BitVector possibleEcRows(12, true);
    possibleEcRows.set(3, false);
    possibleEcRows.set(6, false);
    possibleEcRows.set(7, false);
    possibleEcRows.set(8, false);
    possibleEcRows.set(11, false);

    storm::storage::BitVector allowEmptyRows(5, true);
    allowEmptyRows.set(1, false);
    allowEmptyRows.set(4, false);

    auto res = storm::transformer::EndComponentEliminator<double>::transform(matrix, subsystem, possibleEcRows, allowEmptyRows);

    // Expected data
    // State 0 is a singleton EC that is replaced by state 2
    // States 1,4 build an EC that will be eliminated and replaced by state 1.
    // State 2 is not part of the subsystem and thus disregarded
    // State 3 is the only state that is kept as it is (except of the transition to 2) and will now be represented by state 0
    storm::storage::SparseMatrixBuilder<double> expectedBuilder(8, 3, 8, true, true, 3);
    ASSERT_NO_THROW(expectedBuilder.newRowGroup(0));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(0, 1, 1.0));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(2, 0, 1.0));
    ASSERT_NO_THROW(expectedBuilder.newRowGroup(3));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(3, 0, 0.3));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(3, 1, 0.7));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(4, 1, 1.0));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(5, 0, 1.0));
    ASSERT_NO_THROW(expectedBuilder.newRowGroup(6));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(6, 0, 0.4));
    ASSERT_NO_THROW(expectedBuilder.addNextValue(6, 1, 0.5));
    storm::storage::SparseMatrix<double> expectedMatrix;
    ASSERT_NO_THROW(expectedMatrix = expectedBuilder.build());
    std::vector<uint_fast64_t> expectedNewToOldRowMapping = {6, 7, 8, 2, 3, 11, 1, 0};
    std::vector<uint_fast64_t> expectedOldToNewStateMapping = {2, 1, std::numeric_limits<uint_fast64_t>::max(), 0, 1};

    // std::cout << "Original matrix:\n" << matrix << "\n\nComputation Result: \n" << res.matrix
    // << "\n\nexpected Matrix\n"<< expectedMatrix << '\n';

    // Note that there are other possible solutions that yield equivalent matrices / vectors.
    // In particular, the ordering within the row groups depends on the MEC decomposition implementation.
    // Hence, we can not do this:
    // EXPECT_EQ(expectedMatrix, res.matrix);
    // EXPECT_EQ(expectedNewToOldRowMapping, res.newToOldRowMapping);
    // EXPECT_EQ(expectedOldToNewStateMapping, res.oldToNewStateMapping);

    // Instead, we try to find a mapping from the actual solution to the expected solution
    EXPECT_EQ(expectedMatrix.getRowCount(), res.matrix.getRowCount());
    EXPECT_EQ(expectedMatrix.getRowGroupCount(), res.matrix.getRowGroupCount());
    EXPECT_EQ(expectedMatrix.getColumnCount(), res.matrix.getColumnCount());
    EXPECT_EQ(expectedNewToOldRowMapping.size(), res.newToOldRowMapping.size());
    EXPECT_EQ(expectedOldToNewStateMapping.size(), res.oldToNewStateMapping.size());

    std::vector<uint64_t> actualToExpectedStateMapping(res.matrix.getRowGroupCount());
    for (uint64_t oldState = 0; oldState < expectedOldToNewStateMapping.size(); ++oldState) {
        uint64_t expectedNewState = expectedOldToNewStateMapping[oldState];
        uint64_t actualNewState = res.oldToNewStateMapping[oldState];
        if (actualNewState < std::numeric_limits<uint_fast64_t>::max()) {
            ASSERT_LT(expectedNewState, std::numeric_limits<uint_fast64_t>::max()) << " Mapping does not match for oldState " << oldState;
            actualToExpectedStateMapping[actualNewState] = expectedNewState;
        } else {
            ASSERT_EQ(expectedNewState, actualNewState) << " Mapping does not match for oldState " << oldState;
        }
    }
    std::vector<uint64_t> actualToExpectedRowMapping;
    for (uint64_t actualRow = 0; actualRow < res.matrix.getRowCount(); ++actualRow) {
        bool found = false;
        for (uint64_t expectedRow = 0; expectedRow < expectedMatrix.getRowCount(); ++expectedRow) {
            if (res.newToOldRowMapping[actualRow] == expectedNewToOldRowMapping[expectedRow]) {
                actualToExpectedRowMapping.push_back(expectedRow);
                EXPECT_FALSE(found) << "Found multiple matching rows";
                found = true;
            }
        }
        EXPECT_TRUE(found) << "Could not find matching expected row for result row " << actualRow;
    }

    for (uint64_t oldState = 0; oldState < expectedOldToNewStateMapping.size(); ++oldState) {
        uint64_t expectedNewState = expectedOldToNewStateMapping[oldState];
        uint64_t actualNewState = res.oldToNewStateMapping[oldState];
        if (actualNewState < std::numeric_limits<uint_fast64_t>::max()) {
            for (uint64_t actualRow = res.matrix.getRowGroupIndices()[actualNewState]; actualRow < res.matrix.getRowGroupIndices()[actualNewState + 1];
                 ++actualRow) {
                uint64_t expectedRow = actualToExpectedRowMapping[actualRow];
                EXPECT_EQ(res.newToOldRowMapping[actualRow], expectedNewToOldRowMapping[expectedRow]);
                // Check whether the expectedRow belongs to the row group of the expectedState
                EXPECT_GE(expectedRow, expectedMatrix.getRowGroupIndices()[expectedNewState]);
                EXPECT_LT(expectedRow, expectedMatrix.getRowGroupIndices()[expectedNewState + 1]);
                // Check whether the two rows are equal
                EXPECT_EQ(expectedMatrix.getRow(expectedRow).getNumberOfEntries(), res.matrix.getRow(actualRow).getNumberOfEntries());
                for (auto const& expectedEntry : expectedMatrix.getRow(expectedRow)) {
                    bool foundEqualEntry = false;
                    for (auto const& actualEntry : res.matrix.getRow(actualRow)) {
                        if (expectedEntry.getValue() == actualEntry.getValue() &&
                            expectedEntry.getColumn() == actualToExpectedStateMapping[actualEntry.getColumn()]) {
                            EXPECT_FALSE(foundEqualEntry) << "Found multiple equal entries.";
                            foundEqualEntry = true;
                        }
                    }
                    EXPECT_TRUE(foundEqualEntry) << "Could not matching entry for expected entry'" << expectedEntry.getValue() << " (row " << expectedRow
                                                 << ", column " << expectedEntry.getColumn() << "). Was searching at row " << actualRow
                                                 << " of actual matrix \n"
                                                 << res.matrix << ".";
                }
            }
        }
    }
}
