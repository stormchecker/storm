#include "storm-parsers/parser/DeterministicSparseTransitionParser.h"

#include <clocale>
#include <string>

#include "storm-parsers/parser/MappedFile.h"
#include "storm-parsers/util/cstring.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/BuildSettings.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {

using namespace storm::utility::cstring;

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> DeterministicSparseTransitionParser<ValueType>::parseDeterministicTransitions(std::string const& filename) {
    storm::storage::SparseMatrix<ValueType> emptyMatrix;
    return DeterministicSparseTransitionParser<ValueType>::parse(filename, false, emptyMatrix);
}

template<typename ValueType>
template<typename MatrixValueType>
storm::storage::SparseMatrix<ValueType> DeterministicSparseTransitionParser<ValueType>::parseDeterministicTransitionRewards(
    std::string const& filename, storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix) {
    return DeterministicSparseTransitionParser<ValueType>::parse(filename, true, transitionMatrix);
}

template<typename ValueType>
template<typename MatrixValueType>
storm::storage::SparseMatrix<ValueType> DeterministicSparseTransitionParser<ValueType>::parse(
    std::string const& filename, bool isRewardFile, storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix) {
    // Enforce locale where decimal point is '.'.
    setlocale(LC_NUMERIC, "C");

    // Open file.
    MappedFile file(filename.c_str());
    char const* buf = file.getData();

    // Perform first pass, i.e. count entries that are not zero.
    DeterministicSparseTransitionParser<ValueType>::FirstPassResult firstPass =
        DeterministicSparseTransitionParser<ValueType>::firstPass(file.getData(), !isRewardFile);

    STORM_LOG_TRACE("First pass on " << filename << " shows " << firstPass.numberOfNonzeroEntries << " non-zeros.");

    // If first pass returned zero, the file format was wrong.
    if (firstPass.numberOfNonzeroEntries == 0) {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Error while parsing " << filename << ": empty or erroneous file format.");
    }

    // Perform second pass.

    // Skip the format hint if it is there.
    buf = trimWhitespaces(buf);
    if (buf[0] < '0' || buf[0] > '9') {
        buf = forwardToLineEnd(buf);
        buf = trimWhitespaces(buf);
    }

    if (isRewardFile) {
        // The reward matrix should match the size of the transition matrix.
        if (firstPass.highestStateIndex + 1 > transitionMatrix.getRowCount() || firstPass.highestStateIndex + 1 > transitionMatrix.getColumnCount()) {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Reward matrix has more rows or columns than transition matrix.");
        } else {
            // If we found the right number of states or less, we set it to the number of states represented by the transition matrix.
            firstPass.highestStateIndex = transitionMatrix.getRowCount() - 1;
        }
    }

    // Creating matrix builder here.
    // The actual matrix will be build once all contents are inserted.
    storm::storage::SparseMatrixBuilder<ValueType> resultMatrix(firstPass.highestStateIndex + 1, firstPass.highestStateIndex + 1,
                                                                firstPass.numberOfNonzeroEntries);

    uint_fast64_t row, col, lastRow = 0;
    double val;
    bool dontFixDeadlocks = storm::settings::getModule<storm::settings::modules::BuildSettings>().isDontFixDeadlocksSet();
    bool hadDeadlocks = false;

    // Read all transitions from file. Note that we assume that the
    // transitions are listed in canonical order, otherwise this will not
    // work, i.e. the values in the matrix will be at wrong places.

    // Different parsing routines for transition systems and transition rewards.
    if (isRewardFile) {
        while (buf[0] != '\0') {
            // Read next transition.
            row = checked_strtol(buf, &buf);
            col = checked_strtol(buf, &buf);
            val = checked_strtod(buf, &buf);

            resultMatrix.addNextValue(row, col, val);
            buf = trimWhitespaces(buf);
        }
    } else {
        // Read first row and add self-loops if necessary.
        char const* tmp;
        row = checked_strtol(buf, &tmp);

        if (row > 0) {
            for (uint_fast64_t skippedRow = 0; skippedRow < row; ++skippedRow) {
                hadDeadlocks = true;
                if (!dontFixDeadlocks) {
                    resultMatrix.addNextValue(skippedRow, skippedRow, storm::utility::one<ValueType>());
                    STORM_LOG_WARN("Warning while parsing " << filename << ": state " << skippedRow
                                                            << " has no outgoing transitions. A self-loop was inserted.");
                } else {
                    STORM_LOG_ERROR("Error while parsing " << filename << ": state " << skippedRow << " has no outgoing transitions.");
                    // Before throwing the appropriate exception we will give notice of all deadlock states.
                }
            }
        }

        while (buf[0] != '\0') {
            // Read next transition.
            row = checked_strtol(buf, &buf);
            col = checked_strtol(buf, &buf);
            val = checked_strtod(buf, &buf);

            // Test if we moved to a new row.
            // Handle all incomplete or skipped rows.
            if (lastRow != row) {
                for (uint_fast64_t skippedRow = lastRow + 1; skippedRow < row; ++skippedRow) {
                    hadDeadlocks = true;
                    if (!dontFixDeadlocks) {
                        resultMatrix.addNextValue(skippedRow, skippedRow, storm::utility::one<ValueType>());
                        STORM_LOG_INFO("Warning while parsing " << filename << ": state " << skippedRow
                                                                << " has no outgoing transitions. A self-loop was inserted.");
                    } else {
                        STORM_LOG_ERROR("Error while parsing " << filename << ": state " << skippedRow << " has no outgoing transitions.");
                        // Before throwing the appropriate exception we will give notice of all deadlock states.
                    }
                }
                lastRow = row;
            }

            resultMatrix.addNextValue(row, col, val);
            buf = trimWhitespaces(buf);
        }

        // If we encountered deadlock and did not fix them, now is the time to throw the exception.
        if (dontFixDeadlocks && hadDeadlocks)
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Some of the states do not have outgoing transitions.");
    }

    // Finally, build the actual matrix, test and return it.
    storm::storage::SparseMatrix<ValueType> result = resultMatrix.build();

    // Since we cannot check if each transition for which there is a reward in the reward file also exists in the transition matrix during parsing, we have to
    // do it afterwards.
    if (isRewardFile && !result.isSubmatrixOf(transitionMatrix)) {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "There are rewards for non existent transitions given in the reward file.");
    }

    return result;
}

template<typename ValueType>
typename DeterministicSparseTransitionParser<ValueType>::FirstPassResult DeterministicSparseTransitionParser<ValueType>::firstPass(
    char const* buf, bool reserveDiagonalElements) {
    DeterministicSparseTransitionParser<ValueType>::FirstPassResult result;

    // Skip the format hint if it is there.
    buf = trimWhitespaces(buf);
    if (buf[0] < '0' || buf[0] > '9') {
        buf = forwardToLineEnd(buf);
        buf = trimWhitespaces(buf);
    }

    // Check all transitions for non-zero diagonal entries and deadlock states.
    uint_fast64_t row, col, lastRow = 0, lastCol = -1;

    // Read first row and reserve space for self-loops if necessary.
    char const* tmp;
    row = checked_strtol(buf, &tmp);
    if (row > 0 && reserveDiagonalElements) {
        for (uint_fast64_t skippedRow = 0; skippedRow < row; ++skippedRow) {
            ++result.numberOfNonzeroEntries;
        }
    }

    while (buf[0] != '\0') {
        // Read the transition.
        row = checked_strtol(buf, &buf);
        col = checked_strtol(buf, &buf);
        // The actual read value is not needed here.
        checked_strtod(buf, &buf);

        if (lastRow != row && reserveDiagonalElements) {
            // Compensate for missing rows.
            for (uint_fast64_t skippedRow = lastRow + 1; skippedRow < row; ++skippedRow) {
                ++result.numberOfNonzeroEntries;
            }
        }

        // Check if a higher state id was found.
        if (row > result.highestStateIndex)
            result.highestStateIndex = row;
        if (col > result.highestStateIndex)
            result.highestStateIndex = col;

        ++result.numberOfNonzeroEntries;

        // Have we already seen this transition?
        if (row == lastRow && col == lastCol) {
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "The same transition (" << row << ", " << col << ") is given twice.");
        }

        lastRow = row;
        lastCol = col;

        buf = trimWhitespaces(buf);
    }

    if (reserveDiagonalElements) {
        for (uint_fast64_t skippedRow = (uint_fast64_t)(lastRow + 1); skippedRow <= result.highestStateIndex; ++skippedRow) {
            ++result.numberOfNonzeroEntries;
        }
    }

    return result;
}

template class DeterministicSparseTransitionParser<double>;
template storm::storage::SparseMatrix<double> DeterministicSparseTransitionParser<double>::parseDeterministicTransitionRewards(
    std::string const& filename, storm::storage::SparseMatrix<double> const& transitionMatrix);
template storm::storage::SparseMatrix<double> DeterministicSparseTransitionParser<double>::parse(std::string const& filename, bool isRewardFile,
                                                                                                 storm::storage::SparseMatrix<double> const& transitionMatrix);

template class DeterministicSparseTransitionParser<storm::Interval>;

template storm::storage::SparseMatrix<storm::Interval> DeterministicSparseTransitionParser<storm::Interval>::parseDeterministicTransitionRewards(
    std::string const& filename, storm::storage::SparseMatrix<double> const& transitionMatrix);
template storm::storage::SparseMatrix<storm::Interval> DeterministicSparseTransitionParser<storm::Interval>::parse(
    std::string const& filename, bool isRewardFile, storm::storage::SparseMatrix<double> const& transitionMatrix);
}  // namespace parser
}  // namespace storm
