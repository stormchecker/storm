#include "storm-parsers/parser/NondeterministicSparseTransitionParser.h"

#include <string>

#include "storm-parsers/parser/MappedFile.h"
#include "storm-parsers/util/cstring.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/BuildSettings.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {

using namespace storm::utility::cstring;

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> NondeterministicSparseTransitionParser<ValueType>::parseNondeterministicTransitions(std::string const& filename) {
    storm::storage::SparseMatrix<ValueType> emptyMatrix;
    return NondeterministicSparseTransitionParser::parse(filename, false, emptyMatrix);
}

template<typename ValueType>
template<typename MatrixValueType>
storm::storage::SparseMatrix<ValueType> NondeterministicSparseTransitionParser<ValueType>::parseNondeterministicTransitionRewards(
    std::string const& filename, storm::storage::SparseMatrix<MatrixValueType> const& modelInformation) {
    return NondeterministicSparseTransitionParser::parse(filename, true, modelInformation);
}

template<typename ValueType>
template<typename MatrixValueType>
storm::storage::SparseMatrix<ValueType> NondeterministicSparseTransitionParser<ValueType>::parse(
    std::string const& filename, bool isRewardFile, storm::storage::SparseMatrix<MatrixValueType> const& modelInformation) {
    // Enforce locale where decimal point is '.'.
    setlocale(LC_NUMERIC, "C");

    // Open file.
    MappedFile file(filename.c_str());
    char const* buf = file.getData();

    // Perform first pass, i.e. obtain number of columns, rows and non-zero elements.
    NondeterministicSparseTransitionParser::FirstPassResult firstPass =
        NondeterministicSparseTransitionParser::firstPass(file.getData(), isRewardFile, modelInformation);

    // If first pass returned zero, the file format was wrong.
    if (firstPass.numberOfNonzeroEntries == 0) {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Error while parsing " << filename << ": erroneous file format.");
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
        if (firstPass.choices > modelInformation.getRowCount() || (uint_fast64_t)(firstPass.highestStateIndex + 1) > modelInformation.getColumnCount()) {
            STORM_LOG_THROW(false, storm::exceptions::OutOfRangeException, "Reward matrix size exceeds transition matrix size.");
        } else if (firstPass.choices != modelInformation.getRowCount()) {
            STORM_LOG_THROW(false, storm::exceptions::OutOfRangeException, "Reward matrix row count does not match transition matrix row count.");
        } else if (firstPass.numberOfNonzeroEntries > modelInformation.getEntryCount()) {
            STORM_LOG_ERROR("The reward matrix has more entries than the transition matrix. There must be a reward for a non existent transition");
            STORM_LOG_THROW(false, storm::exceptions::OutOfRangeException, "The reward matrix has more entries than the transition matrix.");
        } else {
            firstPass.highestStateIndex = modelInformation.getColumnCount() - 1;
        }
    }

    // Create the matrix builder.
    // The matrix to be build should have as many columns as we have nodes and as many rows as we have choices.
    // Those two values, as well as the number of nonzero elements, was been calculated in the first run.
    STORM_LOG_INFO("Attempting to create matrix of size " << firstPass.choices << " x " << (firstPass.highestStateIndex + 1) << " with "
                                                          << firstPass.numberOfNonzeroEntries << " entries.");
    storm::storage::SparseMatrixBuilder<ValueType> matrixBuilder;
    if (!isRewardFile) {
        matrixBuilder = storm::storage::SparseMatrixBuilder<ValueType>(firstPass.choices, firstPass.highestStateIndex + 1, firstPass.numberOfNonzeroEntries,
                                                                       true, true, firstPass.highestStateIndex + 1);
    } else {
        matrixBuilder = storm::storage::SparseMatrixBuilder<ValueType>(firstPass.choices, firstPass.highestStateIndex + 1, firstPass.numberOfNonzeroEntries,
                                                                       true, true, modelInformation.getRowGroupCount());
    }

    // Initialize variables for the parsing run.
    uint_fast64_t source = 0, target = 0, lastSource = 0, choice = 0, lastChoice = 0, curRow = 0;
    double val = 0.0;
    bool dontFixDeadlocks = storm::settings::getModule<storm::settings::modules::BuildSettings>().isDontFixDeadlocksSet();
    bool hadDeadlocks = false;

    // The first state already starts a new row group of the matrix.
    matrixBuilder.newRowGroup(0);

    // Read all transitions from file.
    while (buf[0] != '\0') {
        // Read source state and choice.
        source = checked_strtol(buf, &buf);
        choice = checked_strtol(buf, &buf);

        if (isRewardFile) {
            // If we have switched the source state, we possibly need to insert the rows of the last
            // source state.
            if (source != lastSource) {
                curRow += ((modelInformation.getRowGroupIndices())[lastSource + 1] - (modelInformation.getRowGroupIndices())[lastSource]) - (lastChoice + 1);
            }

            // If we skipped some states, we need to reserve empty rows for all their nondeterministic
            // choices and create the row groups.
            for (uint_fast64_t i = lastSource + 1; i < source; ++i) {
                matrixBuilder.newRowGroup(modelInformation.getRowGroupIndices()[i]);
                curRow += ((modelInformation.getRowGroupIndices())[i + 1] - (modelInformation.getRowGroupIndices())[i]);
            }

            // If we moved to the next source, we need to open the next row group.
            if (source != lastSource) {
                matrixBuilder.newRowGroup(modelInformation.getRowGroupIndices()[source]);
            }

            // If we advanced to the next state, but skipped some choices, we have to reserve rows
            // for them
            if (source != lastSource) {
                curRow += choice + 1;
            } else if (choice != lastChoice) {
                curRow += choice - lastChoice;
            }
        } else {
            // Increase line count if we have either finished reading the transitions of a certain state
            // or we have finished reading one nondeterministic choice of a state.
            if ((source != lastSource || choice != lastChoice)) {
                ++curRow;
            }

            // Check if we have skipped any source node, i.e. if any node has no
            // outgoing transitions. If so, insert a self-loop.
            // Also begin a new rowGroup for the skipped state.
            for (uint_fast64_t node = lastSource + 1; node < source; node++) {
                hadDeadlocks = true;
                if (!dontFixDeadlocks) {
                    matrixBuilder.newRowGroup(curRow);
                    matrixBuilder.addNextValue(curRow, node, 1);
                    ++curRow;
                    STORM_LOG_INFO("Warning while parsing " << filename << ": node " << node << " has no outgoing transitions. A self-loop was inserted.");
                } else {
                    STORM_LOG_ERROR("Error while parsing " << filename << ": node " << node << " has no outgoing transitions.");
                }
            }
            if (source != lastSource) {
                // Create a new rowGroup for the source, if this is the first choice we encounter for this state.
                matrixBuilder.newRowGroup(curRow);
            }
        }

        // Read target and value and write it to the matrix.
        target = checked_strtol(buf, &buf);
        val = checked_strtod(buf, &buf);
        matrixBuilder.addNextValue(curRow, target, val);

        lastSource = source;
        lastChoice = choice;

        // Proceed to beginning of next line in file and next row in matrix.
        buf = forwardToLineEnd(buf);

        buf = trimWhitespaces(buf);
    }

    if (dontFixDeadlocks && hadDeadlocks && !isRewardFile)
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Some of the states do not have outgoing transitions.");

    // Since we assume the transition rewards are for the transitions of the model, we copy the rowGroupIndices.
    if (isRewardFile) {
        for (uint_fast64_t node = lastSource + 1; node < modelInformation.getRowGroupCount(); node++) {
            matrixBuilder.newRowGroup(modelInformation.getRowGroupIndices()[node]);
        }
    }

    // Finally, build the actual matrix, test and return it.
    storm::storage::SparseMatrix<ValueType> resultMatrix = matrixBuilder.build();

    // Since we cannot check if each transition for which there is a reward in the reward file also exists in the transition matrix during parsing, we have to
    // do it afterwards.
    if (isRewardFile && !resultMatrix.isSubmatrixOf(modelInformation)) {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "There are rewards for non existent transitions given in the reward file.");
    }

    return resultMatrix;
}

template<typename ValueType>
template<typename MatrixValueType>
typename NondeterministicSparseTransitionParser<ValueType>::FirstPassResult NondeterministicSparseTransitionParser<ValueType>::firstPass(
    char const* buf, bool isRewardFile, storm::storage::SparseMatrix<MatrixValueType> const& modelInformation) {
    // Check file header and extract number of transitions.

    // Skip the format hint if it is there.
    buf = trimWhitespaces(buf);
    if (buf[0] < '0' || buf[0] > '9') {
        buf = forwardToLineEnd(buf);
        buf = trimWhitespaces(buf);
    }

    // Read all transitions.
    uint_fast64_t source = 0, target = 0, choice = 0, lastChoice = 0, lastSource = 0, lastTarget = -1;
    double val = 0.0;
    typename NondeterministicSparseTransitionParser<ValueType>::FirstPassResult result;

    // Since the first line is already a new choice but is not covered below, that has to be covered here.
    result.choices = 1;

    while (buf[0] != '\0') {
        // Read source state and choice.
        source = checked_strtol(buf, &buf);

        // Read the name of the nondeterministic choice.
        choice = checked_strtol(buf, &buf);

        if (source < lastSource) {
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException,
                            "The current source state " << source << " is smaller than the last one " << lastSource << ".");
        }

        // Check if we encountered a state index that is bigger than all previously seen.
        if (source > result.highestStateIndex) {
            result.highestStateIndex = source;
        }

        if (isRewardFile) {
            // Make sure that the highest state index of the reward file is not higher than the highest state index of the corresponding model.
            if (result.highestStateIndex > modelInformation.getColumnCount() - 1) {
                STORM_LOG_THROW(false, storm::exceptions::OutOfRangeException,
                                "State index " << result.highestStateIndex << " found. This exceeds the highest state index of the model, which is "
                                               << modelInformation.getColumnCount() - 1 << " .");
            }

            // If we have switched the source state, we possibly need to insert rows for skipped choices of the last
            // source state.
            if (source != lastSource) {
                // number of choices skipped = number of choices of last state - number of choices read
                result.choices +=
                    ((modelInformation.getRowGroupIndices())[lastSource + 1] - (modelInformation.getRowGroupIndices())[lastSource]) - (lastChoice + 1);
            }

            // If we skipped some states, we need to reserve empty rows for all their nondeterministic
            // choices.
            for (uint_fast64_t i = lastSource + 1; i < source; ++i) {
                result.choices += ((modelInformation.getRowGroupIndices())[i + 1] - (modelInformation.getRowGroupIndices())[i]);
            }

            // If we advanced to the next state, but skipped some choices, we have to reserve rows
            // for them.
            if (source != lastSource) {
                result.choices += choice + 1;
            } else if (choice != lastChoice) {
                result.choices += choice - lastChoice;
            }
        } else {
            // If we have skipped some states, we need to reserve the space for the self-loop insertion
            // in the second pass.
            if (source > lastSource + 1) {
                result.numberOfNonzeroEntries += source - lastSource - 1;
                result.choices += source - lastSource - 1;
            }

            if (source != lastSource || choice != lastChoice) {
                // If we have switched the source state or the nondeterministic choice, we need to
                // reserve one row more.
                ++result.choices;
            }
        }

        // Read target and check if we encountered a state index that is bigger than all previously seen.
        target = checked_strtol(buf, &buf);

        if (target > result.highestStateIndex) {
            result.highestStateIndex = target;
        }

        // Also, have we already seen this transition?
        if (target == lastTarget && choice == lastChoice && source == lastSource) {
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException,
                            "The same transition (" << source << ", " << choice << ", " << target << ") is given twice.");
        }

        // Read value and check whether it's positive.
        val = checked_strtod(buf, &buf);
        if (!isRewardFile && (val < 0.0 || val > 1.0)) {
            STORM_LOG_ERROR("Expected a positive probability but got \"" << std::string(buf, 0, 16) << "\".");
            NondeterministicSparseTransitionParser::FirstPassResult nullResult;
            return nullResult;
        } else if (val < 0.0) {
            STORM_LOG_ERROR("Expected a positive reward value but got \"" << std::string(buf, 0, 16) << "\".");
            NondeterministicSparseTransitionParser::FirstPassResult nullResult;
            return nullResult;
        }

        lastChoice = choice;
        lastSource = source;
        lastTarget = target;

        // Increase number of non-zero values.
        result.numberOfNonzeroEntries++;

        // The PRISM output format lists the name of the transition in the fourth column,
        // but omits the fourth column if it is an internal action. In either case we can skip to the end of the line.
        buf = forwardToLineEnd(buf);

        buf = trimWhitespaces(buf);
    }

    if (isRewardFile) {
        // If not all rows were filled for the last state, we need to insert them.
        result.choices += ((modelInformation.getRowGroupIndices())[lastSource + 1] - (modelInformation.getRowGroupIndices())[lastSource]) - (lastChoice + 1);

        // If we skipped some states, we need to reserve empty rows for all their nondeterministic
        // choices.
        for (uint_fast64_t i = lastSource + 1; i < modelInformation.getRowGroupIndices().size() - 1; ++i) {
            result.choices += ((modelInformation.getRowGroupIndices())[i + 1] - (modelInformation.getRowGroupIndices())[i]);
        }
    }

    return result;
}

template class NondeterministicSparseTransitionParser<double>;
template storm::storage::SparseMatrix<double> NondeterministicSparseTransitionParser<double>::parseNondeterministicTransitionRewards(
    std::string const& filename, storm::storage::SparseMatrix<double> const& modelInformation);
template storm::storage::SparseMatrix<double> NondeterministicSparseTransitionParser<double>::parse(
    std::string const& filename, bool isRewardFile, storm::storage::SparseMatrix<double> const& modelInformation);

template class NondeterministicSparseTransitionParser<storm::Interval>;

template storm::storage::SparseMatrix<storm::Interval> NondeterministicSparseTransitionParser<storm::Interval>::parseNondeterministicTransitionRewards<double>(
    std::string const& filename, storm::storage::SparseMatrix<double> const& modelInformation);
template storm::storage::SparseMatrix<storm::Interval> NondeterministicSparseTransitionParser<storm::Interval>::parse<double>(
    std::string const& filename, bool isRewardFile, storm::storage::SparseMatrix<double> const& modelInformation);

}  // namespace parser
}  // namespace storm
