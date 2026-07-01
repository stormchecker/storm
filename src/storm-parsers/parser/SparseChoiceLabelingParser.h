#ifndef STORM_PARSER_SPARSECHOICELABELINGPARSER_H_
#define STORM_PARSER_SPARSECHOICELABELINGPARSER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "storm/storage/BoostTypes.h"

namespace storm {
namespace parser {
/*!
 * A class providing the functionality to parse a choice labeling.
 */
class SparseChoiceLabelingParser {
   public:
    /*!
     * Parses the given file and returns the resulting choice labeling.
     *
     * @param nondeterministicChoiceIndices The indices at which the choices
     * of the states begin.
     * @param filename The name of the file to parse.
     * @return The resulting choice labeling.
     */
    static std::vector<storm::storage::FlatSet<uint_fast64_t>> parseChoiceLabeling(std::vector<uint_fast64_t> const& nondeterministicChoiceIndices,
                                                                                   std::string const& filename);
};
}  // namespace parser
}  // namespace storm

#endif /* STORM_PARSER_SPARSECHOICELABELINGPARSER_H_ */
