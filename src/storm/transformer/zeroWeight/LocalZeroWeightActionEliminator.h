#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"

namespace storm::transformer {

/*!
 * Eliminates zero-weight states by composing their choices into incoming actions.
 */
template<typename ValueType>
class LocalZeroWeightActionEliminator {
   public:
    struct ChoiceSelection {
        uint64_t state;
        uint64_t row;

        bool operator==(ChoiceSelection const&) const = default;
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
        std::vector<std::vector<ChoiceSelection>> choiceSelections;
    };

    /*!
     * Creates an elimination engine for a normalized transition matrix.
     *
     * @param transitionMatrix One column and row group per state.
     * @param actionWeights One nonnegative weight per row.
     * @param targetStates States that must not be eliminated.
     * @param initialStates States that must not be eliminated.
     */
    LocalZeroWeightActionEliminator(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType> const& actionWeights,
                                    storm::storage::BitVector const& targetStates, storm::storage::BitVector const& initialStates);

    /*!
     * Eliminates one state whose active actions all have weight zero.
     *
     * @param state State in the input matrix.
     */
    void eliminateState(uint64_t state);

    /*!
     * Builds the matrix containing the states that have not been eliminated.
     *
     * @return The compacted matrix and its mappings.
     */
    Result build() const;

   private:
    using Transition = std::pair<uint64_t, ValueType>;

    struct WorkingAction {
        bool active;
        uint64_t sourceState;
        std::vector<Transition> transitions;
        ValueType weight;
        uint64_t inputRow;
        std::vector<ChoiceSelection> choiceSelections;
    };

    struct NormalizedChoice {
        std::vector<Transition> transitions;
        std::vector<ChoiceSelection> choiceSelections;
    };

    uint64_t addAction(WorkingAction&& action);
    static bool mergeChoiceSelections(std::vector<ChoiceSelection> const& first, std::vector<ChoiceSelection> const& second,
                                      std::vector<ChoiceSelection>& result);
    static std::vector<Transition> composeTransitions(std::vector<Transition> const& predecessorTransitions, uint64_t eliminatedState,
                                                      ValueType const& probabilityToState, std::vector<Transition> const& normalizedTransitions);

    uint64_t numberOfStates;
    uint64_t numberOfInputRows;
    storm::storage::BitVector targetStates;
    storm::storage::BitVector initialStates;
    storm::storage::BitVector eliminatedStates;
    std::vector<WorkingAction> actions;
    std::vector<std::vector<uint64_t>> stateActions;
    std::vector<std::vector<uint64_t>> incomingActions;
};

}  // namespace storm::transformer
