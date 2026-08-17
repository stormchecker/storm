#include "storm-pars/transformer/IntervalEndComponentPreserver.h"

#include "storm-pars/utility/parametric.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/RobustMaximalEndComponentDecomposition.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/utility/constants.h"
#include "storm/utility/logging.h"

namespace storm {
namespace transformer {

IntervalEndComponentPreserver::IntervalEndComponentPreserver() {
    // Intentionally left empty
}

std::optional<storage::SparseMatrix<Interval>> IntervalEndComponentPreserver::eliminateMECs(storm::storage::SparseMatrix<Interval> const& originalMatrix,
                                                                                            std::vector<Interval> const& originalVector) {
    storage::RobustMaximalEndComponentDecomposition<Interval> decomposition(originalMatrix, originalMatrix.transpose(), originalVector);

    bool hasNonTrivialMEC = false;
    for (auto const& group : decomposition) {
        if (!group.isTrivial()) {
            hasNonTrivialMEC = true;
            // std::cout << "Non-trivial MEC: ";
            // for (auto const& state : group) {
            //     std::cout << state << " ";
            // }
            // std::cout << std::endl;
        }
    }

    if (!hasNonTrivialMEC) {
        return std::nullopt;
    }

    auto const& indexMap = decomposition.computeStateToSccIndexMap(originalMatrix.getRowCount());

    storm::storage::SparseMatrixBuilder<Interval> builder(originalMatrix.getRowCount() + 1, originalMatrix.getColumnCount() + 1, 0, true, false);

    uint64_t sinkState = originalMatrix.getRowCount();

    for (uint64_t row = 0; row < originalMatrix.getRowCount(); row++) {
        if (indexMap.at(row) >= decomposition.size() || decomposition.getBlock(indexMap.at(row)).isTrivial()) {
            // Group is trivial: Copy the row
            for (auto const& entry : originalMatrix.getRow(row)) {
                // We want to route this transition to a state in the group
                builder.addNextValue(row, entry.getColumn(), entry.getValue());
            }
        } else {
            auto const& group = decomposition.getBlock(indexMap.at(row));
            // Group is non-trivial: Check whether state is the smallest in the group
            uint64_t smallestInGroup = *group.begin();
            if (row != smallestInGroup) {
                // Add a one transition that points to the smallest state in the group
                builder.addNextValue(row, smallestInGroup, utility::one<Interval>());
                continue;
            }
            // Collect all states outside of the group that states inside of the group go to
            boost::container::flat_set<uint64_t> groupSet;
            for (auto const& state : group) {
                for (auto const& entry : originalMatrix.getRow(state)) {
                    if (group.getStates().contains(entry.getColumn()) || utility::isZero(entry.getValue())) {
                        continue;
                    }
                    // We want to route this transition to the state representing the group
                    uint64_t groupIndex = indexMap.at(entry.getColumn());
                    uint64_t stateRepresentingGroup = groupIndex >= decomposition.size() ? entry.getColumn() : *decomposition.getBlock(groupIndex).begin();
                    groupSet.insert(stateRepresentingGroup);
                }
            }
            STORM_LOG_DEBUG("Transformed group of size " << groupSet.size() << " for state " << row);
            // Insert interval [0, 1] to all of these states
            for (auto const& state : groupSet) {
                builder.addNextValue(row, state, Interval(0, 1));
            }
            builder.addNextValue(row, sinkState, Interval(0, 1));
        }
    }

    return builder.build();
}

}  // namespace transformer
}  // namespace storm