#pragma once

#include "storm-parsers/parser/ExplicitModelParserOptions.h"
#include "storm/storage/SparseMatrix.h"

#include <vector>

namespace storm {
namespace parser {

/*!
 * A class providing the functionality to parse the transitions of a nondeterministic model.
 *
 * The file is parsed in two passes.
 * The first pass tests the file format and collects statistical data needed for the second pass.
 * The second pass then collects the actual file data and compiles it into a Result.
 */
template<typename ValueType = double>
class NondeterministicSparseTransitionParser {
   public:
    /*!
     * A structure representing the result of the first pass of this parser.
     * It contains the number of non-zero entries in the model, the highest state index and the total number if nondeterministic choices.
     */
    struct FirstPassResult {
        /*!
         * The default constructor.
         * Constructs an empty FirstPassResult.
         */
        FirstPassResult() : numberOfNonzeroEntries(0), highestStateIndex(0), choices(0) {
            // Intentionally left empty.
        }

        //! The total number of non-zero entries of the model.
        uint_fast64_t numberOfNonzeroEntries;

        //! The highest state index that appears in the model.
        uint_fast64_t highestStateIndex;

        //! The total number of nondeterministic choices within the transition system.
        uint_fast64_t choices;
    };

    /*!
     * Load a nondeterministic transition system from file and create a sparse adjacency matrix whose entries represent the weights of the edges
     *
     * @param filename The path and name of file to be parsed.
     */
    static storm::storage::SparseMatrix<ValueType> parseNondeterministicTransitions(std::string const& filename,
                                                                                    ExplicitModelParserOptions const& options = ExplicitModelParserOptions());

    /*!
     * Load a nondeterministic transition system from file and create a sparse adjacency matrix whose entries represent the weights of the edges
     *
     * @param filename The path and name of file to be parsed.
     * @param modelInformation The information about the transition structure of nondeterministic model in which the transition rewards shall be used.
     * @return A struct containing the parsed file contents, i.e. the transition reward matrix and the mapping between its rows and the states of the model.
     */
    template<typename MatrixValueType>
    static storm::storage::SparseMatrix<ValueType> parseNondeterministicTransitionRewards(
        std::string const& filename, storm::storage::SparseMatrix<MatrixValueType> const& modelInformation);

   private:
    /*!
     * This method does the first pass through the buffer containing the content of some transition file.
     *
     * It computes the overall number of nondeterministic choices, i.e. the
     * number of rows in the matrix that should be created.
     * It also calculates the overall number of non-zero cells, i.e. the number
     * of elements the matrix has to hold, and the maximum node id, i.e. the
     * number of columns of the matrix.
     *
     * @param buffer Buffer containing the data to scan. This is expected to be some char array.
     * @param insertDiagonalEntriesIfMissing A flag set iff entries on the primary diagonal of the matrix should be added in case they are missing in the parsed
     * file.
     * @return A structure representing the result of the first pass.
     */
    template<typename MatrixValueType>
    static FirstPassResult firstPass(char const* buffer, bool isRewardFile, storm::storage::SparseMatrix<MatrixValueType> const& modelInformation);

    /*!
     * The main parsing routine.
     * Opens the given file, calls the first pass and performs the second pass, parsing the content of the file into a SparseMatrix.
     *
     * @param filename The path and name of file to be parsed.
     * @param rewardFile A flag set iff the file to be parsed contains transition rewards.
     * @param insertDiagonalEntriesIfMissing A flag set iff entries on the primary diagonal of the matrix should be added in case they are missing in the parsed
     * file.
     * @param modelInformation A struct containing information that is used to check if the transition reward matrix fits to the rest of the model.
     * @return A SparseMatrix containing the parsed file contents.
     */
    template<typename MatrixValueType>
    static storm::storage::SparseMatrix<ValueType> parse(std::string const& filename, bool isRewardFile,
                                                         storm::storage::SparseMatrix<MatrixValueType> const& modelInformation,
                                                         ExplicitModelParserOptions const& options = ExplicitModelParserOptions());
};

}  // namespace parser
}  // namespace storm
