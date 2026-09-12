#pragma once

#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"

namespace storm::transformer {

/*!
 * Classifies action weights outside target states.
 */
template<typename ValueType>
class ZeroWeightActionAnalysis {
   public:
    struct Result {
        storm::storage::BitVector zeroWeightChoices;
        storm::storage::BitVector statesWithZeroWeightChoices;
        storm::storage::BitVector pureZeroWeightStates;
        storm::storage::BitVector mixedZeroWeightStates;
        storm::storage::BitVector positiveOnlyStates;
    };

    /*!
     * Classifies non-target actions and states by weight.
     *
     * @param transitionMatrix The normalized transition matrix.
     * @param actionWeights One nonnegative weight per matrix row.
     * @param targetStates States excluded from the classification.
     * @return The action and state classification.
     */
    static Result analyze(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType> const& actionWeights,
                          storm::storage::BitVector const& targetStates);
};

}  // namespace storm::transformer
