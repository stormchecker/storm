#pragma once

#include <cstdint>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"

namespace storm::transformer {

/*! A weak zero-weight component and its interface in the split matrix. */
struct ZeroWeightComponent {
    std::vector<uint64_t> states;
    std::vector<uint64_t> entryStates;
    std::vector<uint64_t> positivePredecessorRows;
    std::vector<uint64_t> boundaryStates;

    bool operator==(ZeroWeightComponent const&) const = default;
};

/*!
 * Classifies action weights and weak zero-weight components outside target states.
 */
template<typename ValueType>
class ZeroWeightAnalysis {
   public:
    struct Result {
        storm::storage::BitVector zeroWeightChoices;
        storm::storage::BitVector statesWithZeroWeightChoices;
        storm::storage::BitVector pureZeroWeightStates;
        storm::storage::BitVector mixedZeroWeightStates;
        storm::storage::BitVector positiveOnlyStates;
        /*! Interfaces are filled separately by analyzeComponentInterfaces. */
        std::vector<ZeroWeightComponent> weakComponents;
        std::vector<uint64_t> stateToWeakComponent;
    };

    /*!
     * Classifies non-target actions and states and finds weak zero-weight components.
     *
     * @param transitionMatrix The transition matrix.
     * @param actionWeights One nonnegative weight per matrix row.
     * @param targetStates States excluded from the classification.
     * @return The classification and component membership.
     */
    static Result analyze(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType> const& actionWeights,
                          storm::storage::BitVector const& targetStates);

    /*!
     * Fills sorted, unique component interfaces after mixed-state splitting.
     * Boundaries cover the whole component. Properness is not checked.
     *
     * @param transitionMatrix The split transition matrix.
     * @param analysis Matching analysis, updated in place.
     */
    static void analyzeComponentInterfaces(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, Result& analysis);
};

}  // namespace storm::transformer
