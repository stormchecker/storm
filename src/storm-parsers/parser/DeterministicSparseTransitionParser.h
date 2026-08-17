#pragma once

#include "storm-parsers/parser/ExplicitModelParserOptions.h"
#include "storm/storage/SparseMatrix.h"

namespace storm {
namespace parser {

/*!
 *	This class can be used to parse a file containing either transitions or transition rewards of a deterministic model.
 *
 *	The file is parsed in two passes.
 *	The first pass tests the file format and collects statistical data needed for the second pass.
 *	The second pass then parses the file data and constructs a SparseMatrix representing it.
 */
template<typename ValueType = double>
class DeterministicSparseTransitionParser {
   public:
    /*!
     * A structure representing the result of the first pass of this parser. It contains the number of non-zero entries in the model and the highest state
     * index.
     */
    struct FirstPassResult {
        /*!
         * The default constructor.
         * Constructs an empty FirstPassResult.
         */
        FirstPassResult() : numberOfNonzeroEntries(0), highestStateIndex(0) {
            // Intentionally left empty.
        }

        //! The total number of non-zero entries of the model.
        uint_fast64_t numberOfNonzeroEntries;

        //! The highest state index that appears in the model.
        uint_fast64_t highestStateIndex;
    };

    /*!
     * Load a deterministic transition system from file and create a
     * sparse adjacency matrix whose entries represent the weights of the edges.
     *
     * @param filename The path and name of the file to be parsed.
     * @param insertDiagonalEntriesIfMissing A flag set iff entries on the primary diagonal of the matrix should be added in case they are missing in the parsed
     * file.
     * @return A SparseMatrix containing the parsed transition system.
     */
    static storm::storage::SparseMatrix<ValueType> parseDeterministicTransitions(std::string const& filename,
                                                                                 ExplicitModelParserOptions const& options = ExplicitModelParserOptions());

    /*!
     * Load the transition rewards for a deterministic transition system from file and create a
     * sparse adjacency matrix whose entries represent the rewards of the respective transitions.
     *
     * @param filename The path and name of the file to be parsed.
     * @param transitionMatrix The transition matrix of the system.
     * @return A SparseMatrix containing the parsed transition rewards.
     */
    template<typename MatrixValueType>
    static storm::storage::SparseMatrix<ValueType> parseDeterministicTransitionRewards(std::string const& filename,
                                                                                       storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix);

   private:
    /*
     * Performs the first pass on the input pointed to by the given buffer to obtain the number of
     * transitions and the maximum node id.
     *
     * @param buffer The buffer that contains the input.
     * @param reserveDiagonalElements A flag indicating whether the diagonal elements should be counted as if they
     * were present to enable fixes later.
     * @return A structure representing the result of the first pass.
     */
    static FirstPassResult firstPass(char const* buffer, bool reserveDiagonalElements);

    /*
     * The main parsing routine.
     * Opens the given file, calls the first pass and performs the second pass, parsing the content of the file into a SparseMatrix.
     *
     * @param filename The path and name of the file to be parsed.
     * @param rewardFile A flag set iff the file to be parsed contains transition rewards.
     * @param insertDiagonalEntriesIfMissing A flag set iff entries on the primary diagonal of the matrix should be added in case they are missing in the parsed
     * file.
     * @param transitionMatrix The transition matrix of the system (this is only meaningful if isRewardFile is set to true).
     * @return A SparseMatrix containing the parsed file contents.
     */
    template<typename MatrixValueType>
    static storm::storage::SparseMatrix<ValueType> parse(std::string const& filename, bool isRewardFile,
                                                         storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix,
                                                         ExplicitModelParserOptions const& options = ExplicitModelParserOptions());
};

}  // namespace parser
}  // namespace storm
