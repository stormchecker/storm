#pragma once

#include <cstdint>
#include "storm/models/sparse/StateLabeling.h"

namespace storm {
namespace parser {

/*!
 * This class can be used to parse a labeling file.
 *
 * Since the labeling is state based, the same label parser can be used for all models.
 */
class AtomicPropositionLabelingParser {
   public:
    /*!
     * Reads a label file and puts the result in an AtomicPropositionsLabeling object.
     *
     * @param stateCount The number of states of the model to be labeled.
     * @param filename The path and name of the labeling (.lab) file.
     * @return The parsed labeling as an AtomicPropositionsLabeling object.
     */
    static storm::models::sparse::StateLabeling parseAtomicPropositionLabeling(uint_fast64_t stateCount, std::string const &filename);
};

}  // namespace parser
}  // namespace storm
