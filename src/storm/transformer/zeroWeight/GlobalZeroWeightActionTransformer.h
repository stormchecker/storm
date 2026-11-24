#pragma once

#include <cstdint>
#include <vector>

#include "storm/environment/Environment.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"

namespace storm::transformer {

/*!
 * Replaces zero-weight components with entry-specific exit laws.
 */
template<typename ValueType>
class GlobalZeroWeightActionTransformer {
   public:
    /*!
     * Identifies a law in the result's entryExitLaws by entry state and vertex index.
     */
    struct EntryLawSelection {
        uint64_t entryState;
        uint64_t vertex;

        bool operator==(EntryLawSelection const&) const = default;
    };

    /*!
     * Exit laws for one removed entry. The entry uses input numbering and the boundary uses output numbering.
     */
    struct EntryExitLaws {
        uint64_t entryState;
        std::vector<uint64_t> boundaryStates;
        std::vector<std::vector<ValueType>> laws;
    };

    struct Result {
        storm::storage::SparseMatrix<ValueType> transitionMatrix;
        std::vector<ValueType> actionWeights;
        storm::storage::BitVector targetStates;
        storm::storage::BitVector initialStates;
        std::vector<uint64_t> oldToNewStateMapping;
        std::vector<uint64_t> newToOldStateMapping;
        std::vector<std::vector<uint64_t>> oldToNewRowMapping;
        std::vector<uint64_t> newToOldRowMapping;
        std::vector<std::vector<EntryLawSelection>> entryLawSelections;
        std::vector<EntryExitLaws> entryExitLaws;
    };

    /*!
     * Collapses reachable zero-weight components in a normalized transition matrix.
     *
     * @param environment The LP solver settings.
     * @param transitionMatrix One column and row group per state.
     * @param actionWeights One nonnegative weight per matrix row.
     * @param targetStates States excluded from collapse.
     * @param initialStates States excluded from collapse.
     * @return The transformed matrix and mappings to the input.
     */
    static Result collapse(storm::Environment const& environment, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                           std::vector<ValueType> const& actionWeights, storm::storage::BitVector const& targetStates,
                           storm::storage::BitVector const& initialStates);
};

}  // namespace storm::transformer
