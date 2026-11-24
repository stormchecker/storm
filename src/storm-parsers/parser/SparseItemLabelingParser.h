#pragma once

#include <boost/optional.hpp>
#include <cstdint>
#include <string>

#include "storm-parsers/parser/MappedFile.h"
#include "storm-parsers/storm-parsers-api.h"
#include "storm/models/sparse/ChoiceLabeling.h"
#include "storm/models/sparse/StateLabeling.h"

namespace storm {
namespace parser {

/*!
 * This class can be used to parse a labeling file.
 */
class STORM_PARSERS_API SparseItemLabelingParser {
   public:
    /*!
     * Parses the given file and returns the resulting state labeling.
     *
     * @param stateCount The number of states of the model to be labeled.
     * @param filename The path and name of the labeling (.lab) file.
     * @return The parsed labeling as a StateLabeling object.
     */
    static storm::models::sparse::StateLabeling parseAtomicPropositionLabeling(uint_fast64_t stateCount, std::string const& filename);

    /*!
     * Parses the given file and returns the resulting choice labeling.
     *
     * @param choiceCount the number of choices of the model.
     * @param filename The name of the file to parse.
     * @param nondeterministicChoiceIndices if given, the choices are assumed to be nondeterministic, i.e.,
     *        a choice is specified by a tuple of state index and (local) choice index.
     * @return The resulting choice labeling.
     */
    static storm::models::sparse::ChoiceLabeling parseChoiceLabeling(
        uint_fast64_t choiceCount, std::string const& filename, boost::optional<std::vector<uint_fast64_t>> const& nondeterministicChoiceIndices = boost::none);

   private:
    /*!
     * Perform syntax checks of the file
     * @param file The mapped file
     */
    static void checkSyntax(std::string const& filename, storm::parser::MappedFile const& file);

    /*!
     * Parses the file header (i.e. the #DECLARATION ... #END construct) and adds the label names to the given labeling
     *
     * @param labeling the labeling to which label names are added
     * @param buf the reference to the file contents
     */
    static void parseLabelNames(std::string const& filename, storm::models::sparse::ItemLabeling& labeling, char const*& buf);

    /*!
     * Parses the label assignments assuming that each item is uniquely specified by a single index, e.g.,
     *  * 42 label1 label2 label3
     *
     * @param labeling the labeling to which file assignments are added
     * @param buf the reference to the file contents
     */
    static void parseDeterministicLabelAssignments(std::string const& filename, storm::models::sparse::ItemLabeling& labeling, char const*& buf);

    /*!
     * Parses the label assignments assuming that each item is specified by a tuple of indices, e.g.,
     *  * 42 0 label1 label2
     *  * 42 1 label2 label3
     *
     * @param labeling the labeling to which file assignments are added
     * @param nondeterministicChoiceIndices The indices at which the choices of the states begin.
     * @param buf the reference to the file contents
     */
    static void parseNonDeterministicLabelAssignments(std::string const& filename, storm::models::sparse::ChoiceLabeling& labeling,
                                                      std::vector<uint_fast64_t> const& nondeterministicChoiceIndices, char const*& buf);
};

}  // namespace parser
}  // namespace storm
