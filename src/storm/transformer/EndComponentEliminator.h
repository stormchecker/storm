#pragma once

#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

namespace storm {
namespace transformer {

template<typename ValueType>
class EndComponentEliminator {
   public:
    struct EndComponentEliminatorReturnType {
        // The resulting matrix
        storm::storage::SparseMatrix<ValueType> matrix;
        // Index mapping that gives for each row of the resulting matrix the corresponding row in the original matrix.
        // For the sink rows added to EC states, an arbitrary row of the original matrix that stays inside the EC is given.
        std::vector<uint_fast64_t> newToOldRowMapping;
        // Gives for each state (=rowGroup) of the original matrix the corresponding state in the resulting matrix.
        // States of a removed ECs are mapped to the state that substitutes the EC.
        // If the given state does not exist in the result (i.e., it is not in the provided subsystem), the returned value will be
        // std::numeric_limits<uint_fast64_t>::max(), i.e., an invalid index.
        std::vector<uint_fast64_t> oldToNewStateMapping;
        // Indicates the rows that represent "staying" in the eliminated EC for ever
        storm::storage::BitVector sinkRows;
    };

    /*
     * Substitutes the given end components by a single state.
     *
     *
     * Only states in the given subsystem are kept. Transitions leading to a state outside of the subsystem will be
     * removed (but the corresponding row is kept, possibly yielding empty rows).
     * The given ECs all have to lie within the provided subsystem.
     *
     * For each such EC (that is not contained in another EC), we add a new state and redirect all incoming and outgoing
     * transitions of the EC to (and from) this state.
     * If addSinkRowStates is true for at least one state of an eliminated EC, a row is added to the new state (representing the choice to stay at the EC
     * forever). If addSelfLoopAtSinkStates is true, such rows get a selfloop (with value 1). Otherwise, the row remains empty.
     */
    static EndComponentEliminatorReturnType transform(storm::storage::SparseMatrix<ValueType> const& originalMatrix,
                                                      storm::storage::MaximalEndComponentDecomposition<ValueType> ecs,
                                                      storm::storage::BitVector const& subsystemStates, storm::storage::BitVector const& addSinkRowStates,
                                                      bool addSelfLoopAtSinkStates = false) {
        // further shrink the set of kept states by removing all states that are part of an EC
        storm::storage::BitVector keptStates = subsystemStates;
        for (auto const& ec : ecs) {
            for (auto const& stateActionsPair : ec) {
                keptStates.set(stateActionsPair.first, false);
            }
        }
        STORM_LOG_DEBUG("Found " << ecs.size() << " end components to eliminate. Keeping " << keptStates.getNumberOfSetBits() << " of " << keptStates.size()
                                 << " original states plus " << ecs.size() << "new end component states.");

        EndComponentEliminatorReturnType result;
        std::vector<uint_fast64_t> newRowGroupIndices;
        result.oldToNewStateMapping = std::vector<uint_fast64_t>(originalMatrix.getRowGroupCount(), std::numeric_limits<uint_fast64_t>::max());
        result.sinkRows =
            storm::storage::BitVector(originalMatrix.getRowCount(), false);  // will be resized as soon as the rowCount of the resulting matrix is known

        for (auto keptState : keptStates) {
            result.oldToNewStateMapping[keptState] = newRowGroupIndices.size();  // i.e., the current number of processed states
            newRowGroupIndices.push_back(result.newToOldRowMapping.size());      // i.e., the current number of processed rows
            for (uint_fast64_t oldRow = originalMatrix.getRowGroupIndices()[keptState]; oldRow < originalMatrix.getRowGroupIndices()[keptState + 1]; ++oldRow) {
                result.newToOldRowMapping.push_back(oldRow);
            }
        }
        for (auto const& ec : ecs) {
            newRowGroupIndices.push_back(result.newToOldRowMapping.size());
            bool ecGetsSinkRow = false;
            for (auto const& stateActionsPair : ec) {
                result.oldToNewStateMapping[stateActionsPair.first] = newRowGroupIndices.size() - 1;
                for (uint_fast64_t row = originalMatrix.getRowGroupIndices()[stateActionsPair.first];
                     row < originalMatrix.getRowGroupIndices()[stateActionsPair.first + 1]; ++row) {
                    if (stateActionsPair.second.find(row) == stateActionsPair.second.end()) {
                        result.newToOldRowMapping.push_back(row);
                    }
                }
                ecGetsSinkRow |= addSinkRowStates.get(stateActionsPair.first);
            }
            if (ecGetsSinkRow) {
                STORM_LOG_ASSERT(result.newToOldRowMapping.size() < originalMatrix.getRowCount(),
                                 "Didn't expect to see more rows in the reduced matrix than in the original one.");
                result.sinkRows.set(result.newToOldRowMapping.size(), true);
                result.newToOldRowMapping.push_back(*ec.begin()->second.begin());
            }
        }
        newRowGroupIndices.push_back(result.newToOldRowMapping.size());
        result.sinkRows.resize(result.newToOldRowMapping.size());

        result.matrix = buildTransformedMatrix(originalMatrix, newRowGroupIndices, result.newToOldRowMapping, result.oldToNewStateMapping, result.sinkRows,
                                               addSelfLoopAtSinkStates);
        STORM_LOG_DEBUG("EndComponentEliminator is done. Resulting matrix has " << result.matrix.getRowGroupCount() << " row groups.");
        return result;
    }

    /*
     * Identifies end components and substitutes them by a single state.
     *
     * Only states in the given subsystem are kept. Transitions leading to a state outside of the subsystem will be
     * removed (but the corresponding row is kept, possibly yielding empty rows).
     * The ECs are then identified on the subsystem.
     *
     * Only ECs for which possibleECRows is true for all choices are considered.
     * Furthermore, the rows that contain a transition leading outside of the subsystem are not considered for an EC.
     *
     * For each such EC (that is not contained in another EC), we add a new state and redirect all incoming and outgoing
     * transitions of the EC to (and from) this state.
     * If addSinkRowStates is true for at least one state of an eliminated EC, a row is added to the new state (representing the choice to stay at the EC
     * forever). If addSelfLoopAtSinkStates is true, such rows get a selfloop (with value 1). Otherwise, the row remains empty.
     */
    static EndComponentEliminatorReturnType transform(storm::storage::SparseMatrix<ValueType> const& originalMatrix,
                                                      storm::storage::BitVector const& subsystemStates, storm::storage::BitVector const& possibleECRows,
                                                      storm::storage::BitVector const& addSinkRowStates, bool addSelfLoopAtSinkStates = false) {
        STORM_LOG_DEBUG("Invoked EndComponentEliminator on matrix with " << originalMatrix.getRowGroupCount() << " row groups and "
                                                                         << subsystemStates.getNumberOfSetBits() << " subsystem states.");

        // storm::storage::MaximalEndComponentDecomposition<ValueType> ecs = computeECs(originalMatrix, possibleECRows, subsystemStates);
        storm::storage::MaximalEndComponentDecomposition<ValueType> ecs(originalMatrix, originalMatrix.transpose(true), subsystemStates, possibleECRows);
        return transform(originalMatrix, ecs, subsystemStates, addSinkRowStates, addSelfLoopAtSinkStates);
    }

   private:
    static storm::storage::MaximalEndComponentDecomposition<ValueType> computeECs(storm::storage::SparseMatrix<ValueType> const& originalMatrix,
                                                                                  storm::storage::BitVector const& possibleECRows,
                                                                                  storm::storage::BitVector const& subsystemStates) {
        // Get an auxiliary matrix to identify the correct end components w.r.t. the possible EC rows and the subsystem.
        // This is done by redirecting choices that can never be part of an EC to a sink state.
        // Such choices are either non-possible EC rows or rows that lead to a state that is not in the subsystem.
        uint_fast64_t sinkState = originalMatrix.getRowGroupCount();
        storm::storage::SparseMatrixBuilder<ValueType> builder(originalMatrix.getRowCount() + 1, originalMatrix.getColumnCount() + 1,
                                                               originalMatrix.getEntryCount() + 1, false, true, originalMatrix.getRowGroupCount() + 1);
        uint_fast64_t row = 0;
        for (uint_fast64_t rowGroup = 0; rowGroup < originalMatrix.getRowGroupCount(); ++rowGroup) {
            builder.newRowGroup(row);
            for (; row < originalMatrix.getRowGroupIndices()[rowGroup + 1]; ++row) {
                bool keepRow = possibleECRows.get(row);
                if (keepRow) {  // Also check whether all successors are in the subsystem
                    for (auto const& entry : originalMatrix.getRow(row)) {
                        keepRow &= subsystemStates.get(entry.getColumn());
                    }
                }
                if (keepRow) {
                    for (auto const& entry : originalMatrix.getRow(row)) {
                        builder.addNextValue(row, entry.getColumn(), entry.getValue());
                    }
                } else {
                    builder.addNextValue(row, sinkState, storm::utility::one<ValueType>());
                }
            }
        }
        builder.newRowGroup(row);
        builder.addNextValue(row, sinkState, storm::utility::one<ValueType>());
        storm::storage::SparseMatrix<ValueType> auxiliaryMatrix =
            builder.build(originalMatrix.getRowCount() + 1, originalMatrix.getColumnCount() + 1, originalMatrix.getRowGroupCount() + 1);
        storm::storage::SparseMatrix<ValueType> backwardsTransitions = auxiliaryMatrix.transpose(true);
        storm::storage::BitVector sinkStateAsBitVector(auxiliaryMatrix.getRowGroupCount(), false);
        sinkStateAsBitVector.set(sinkState);
        storm::storage::BitVector auxSubsystemStates = subsystemStates;
        auxSubsystemStates.resize(subsystemStates.size() + 1, true);
        // The states for which sinkState is reachable under every scheduler can not be part of an EC
        auxSubsystemStates &= ~(storm::utility::graph::performProbGreater0A(auxiliaryMatrix, auxiliaryMatrix.getRowGroupIndices(), backwardsTransitions,
                                                                            auxSubsystemStates, sinkStateAsBitVector));
        return storm::storage::MaximalEndComponentDecomposition<ValueType>(auxiliaryMatrix, backwardsTransitions, auxSubsystemStates);
    }

    static storm::storage::SparseMatrix<ValueType> buildTransformedMatrix(storm::storage::SparseMatrix<ValueType> const& originalMatrix,
                                                                          std::vector<uint_fast64_t> const& newRowGroupIndices,
                                                                          std::vector<uint_fast64_t> const& newToOldRowMapping,
                                                                          std::vector<uint_fast64_t> const& oldToNewStateMapping,
                                                                          storm::storage::BitVector const& sinkRows, bool addSelfLoopAtSinkStates) {
        uint_fast64_t numRowGroups = newRowGroupIndices.size() - 1;
        uint_fast64_t newRow = 0;
        storm::storage::SparseMatrixBuilder<ValueType> builder(newToOldRowMapping.size(), numRowGroups, originalMatrix.getEntryCount(), false, true,
                                                               numRowGroups);
        for (uint_fast64_t newRowGroup = 0; newRowGroup < numRowGroups; ++newRowGroup) {
            builder.newRowGroup(newRow);
            for (; newRow < newRowGroupIndices[newRowGroup + 1]; ++newRow) {
                if (sinkRows.get(newRow)) {
                    if (addSelfLoopAtSinkStates) {
                        builder.addNextValue(newRow, newRowGroup, storm::utility::one<ValueType>());
                    }
                } else {
                    // Make sure that the entries for this row are inserted in the right order.
                    // Also, transitions to the same EC need to be merged and transitions to states that are erased need to be ignored
                    std::map<uint_fast64_t, ValueType> sortedEntries;
                    for (auto const& entry : originalMatrix.getRow(newToOldRowMapping[newRow])) {
                        uint_fast64_t newColumn = oldToNewStateMapping[entry.getColumn()];
                        if (newColumn < numRowGroups) {
                            auto insertResult = sortedEntries.insert(std::make_pair(newColumn, entry.getValue()));
                            if (!insertResult.second) {
                                // We have already seen an entry with this column. ==> merge transitions
                                insertResult.first->second += entry.getValue();
                            }
                        }
                    }
                    for (auto const& sortedEntry : sortedEntries) {
                        builder.addNextValue(newRow, sortedEntry.first, sortedEntry.second);
                    }
                }
            }
        }
        return builder.build(newToOldRowMapping.size(), numRowGroups, numRowGroups);
    }
};
}  // namespace transformer
}  // namespace storm
