#pragma once

#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"

namespace storm {
namespace parser {

/*!
 * A class providing the functionality to parse the transitions of a Markov automaton.
 *
 * The file is parsed in two passes.
 * The first pass tests the file format and collects statistical data needed for the second pass.
 * The second pass then collects the actual file data and compiles it into a Result.
 */
template<typename ValueType = double>
class MarkovAutomatonSparseTransitionParser {
   public:
    /*!
     * A structure representing the result of the first pass of this parser. It contains the number of non-zero entries in the model, the highest state index
     * and the total number of choices.
     */
    struct FirstPassResult {
        /*!
         * The default constructor.
         * Constructs an empty FirstPassResult.
         */
        FirstPassResult() : numberOfNonzeroEntries(0), highestStateIndex(0), numberOfChoices(0) {
            // Intentionally left empty.
        }

        //! The total number of non-zero entries of the model.
        uint_fast64_t numberOfNonzeroEntries;

        //! The highest state index that appears in the model.
        uint_fast64_t highestStateIndex;

        //! The total number of nondeterministic choices in the model.
        uint_fast64_t numberOfChoices;
    };

    /*!
     * A structure representing the result of the parser. It contains the sparse matrix that represents the transitions (along with a vector indicating
     * at which index the choices of a given state begin) as well as the exit rates for all Markovian choices.
     */
    struct Result {
        /*!
         * Creates a new instance of the struct using the result of the first pass to correctly initialize the container.
         *
         * @param firstPassResult A reference to the result of the first pass.
         */
        Result(FirstPassResult const& firstPassResult)
            : transitionMatrixBuilder(firstPassResult.numberOfChoices, firstPassResult.highestStateIndex + 1, firstPassResult.numberOfNonzeroEntries, true,
                                      firstPassResult.highestStateIndex + 1),
              markovianChoices(firstPassResult.numberOfChoices),
              markovianStates(firstPassResult.highestStateIndex + 1),
              exitRates(firstPassResult.highestStateIndex + 1) {
            // Intentionally left empty.
        }

        //! A matrix representing the transitions of the model.
        storm::storage::SparseMatrixBuilder<ValueType> transitionMatrixBuilder;

        //! A bit vector indicating which choices are Markovian. By duality, all other choices are probabilitic.
        storm::storage::BitVector markovianChoices;

        //! A bit vector indicating which states possess a Markovian choice.
        storm::storage::BitVector markovianStates;

        //! A vector that stores the exit rates for each state. For all states that do not possess Markovian choices this is equal to 0.
        std::vector<ValueType> exitRates;
    };

    /*!
     * Parses the given file under the assumption that it contains a Markov automaton specified in the appropriate format.
     *
     * @param filename The name of the file to parse.
     * @return A structure representing the result of the parser.
     */
    static Result parseMarkovAutomatonTransitions(std::string const& filename);

   private:
    /*!
     * Performs the first pass on the input pointed to by the given buffer.
     *
     * @param buffer The buffer that cointains the input.
     * @return A structure representing the result of the first pass.
     */
    static FirstPassResult firstPass(char const* buffer);

    /*!
     * Performs the second pass on the input pointed to by the given buffer with the information of the first pass.
     *
     * @param buffer The buffer that cointains the input.
     * @param firstPassResult The result of the first pass performed on the same input.
     * @return A structure representing the result of the second pass.
     */
    static Result secondPass(char const* buffer, FirstPassResult const& firstPassResult);
};

}  // namespace parser
}  // namespace storm
