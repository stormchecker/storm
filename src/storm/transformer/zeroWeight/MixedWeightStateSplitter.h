#pragma once

#include <cstdint>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/zeroWeight/ZeroWeightActionAnalysis.h"

namespace storm::transformer {

/*!
 * Moves positive actions at mixed states to positive-only shells.
 */
template<typename ValueType>
class MixedWeightStateSplitter {
   public:
    struct Result {
        storm::storage::SparseMatrix<ValueType> transitionMatrix;
        std::vector<ValueType> actionWeights;
        storm::storage::BitVector targetStates;
        storm::storage::BitVector initialStates;
        typename ZeroWeightActionAnalysis<ValueType>::Result analysis;
        /*! Shell for each original state, or the maximum uint64_t if not split. */
        std::vector<uint64_t> positiveShells;
        /*! Original state for each output state, including shells. */
        std::vector<uint64_t> newToOldStateMapping;
        /*! Output row for each original action. */
        std::vector<uint64_t> oldToNewRowMapping;
        /*! Original action for each output row, or the maximum uint64_t for stop actions. */
        std::vector<uint64_t> newToOldRowMapping;
    };

    /*!
     * Splits mixed non-target states without changing the input.
     * Original state IDs are retained. Shells are appended in state order.
     * Non-target initial states must have only positive actions.
     * The returned analysis describes the output. Properness is not checked.
     *
     * @param transitionMatrix One column and row group per state.
     * @param actionWeights One nonnegative weight per row.
     * @param targetStates States left unchanged.
     * @param initialStates Initial states in the original matrix.
     * @return The split matrix, weights, classification, and origin mappings.
     */
    static Result split(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType> const& actionWeights,
                        storm::storage::BitVector const& targetStates, storm::storage::BitVector const& initialStates);
};

}  // namespace storm::transformer
