#pragma once

#include "storm/exceptions/UnmetRequirementException.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"
#include "storm/utility/vector.h"

namespace storm {
namespace solver {

namespace helper {

/*!
 * Returns a reordering of the matrix row(groups) and columns such that we can solve the (minmax or linear) equation system in one go.
 * More precisely, let x be the result and i an arbitrary rowgroup index. Solving for rowgroup x[i] only requires knowledge of the result at rowgroups x[i+1],
 * x[i+2], ...
 */
template<typename ValueType>
boost::optional<std::vector<uint64_t>> computeTopologicalGroupOrdering(storm::storage::SparseMatrix<ValueType> const& matrix) {
    uint64_t numGroups = matrix.getRowGroupCount();
    bool orderedMatrixRequired = false;
    std::vector<uint64_t> result;
    result.reserve(numGroups);
    storm::storage::BitVector processed(numGroups, false);
    storm::storage::BitVector visited(numGroups, false);
    std::vector<uint64_t> stack;

    // It's more likely that states without a successor are located at the end (due to the way we build the model).
    // We therefore process the matrix in reverse order.
    uint64_t startState = numGroups;
    while (startState > 0) {
        --startState;
        // skip already processed states
        if (processed.get(startState))
            continue;

        // Now do a dfs from start state.
        stack.push_back(startState);
        while (!stack.empty()) {
            uint64_t current = stack.back();
            if (visited.get(current)) {
                // We are backtracking, so add this state now
                // It might be that we already processed this state before. This can happen, e.g., if this state has two direct predecessors, A and B, and
                // A is also a predecessor of B. Then, this node gets inserted into the stack two times: when analyzing A and B.
                if (!processed.get(current)) {
                    result.push_back(current);
                    processed.set(current);
                }
                stack.pop_back();
            } else {
                visited.set(current);
                for (auto const& entry : matrix.getRowGroup(current)) {
                    if (!processed.get(entry.getColumn()) && !storm::utility::isZero(entry.getValue())) {
                        orderedMatrixRequired = true;
                        STORM_LOG_THROW(!visited.get(entry.getColumn()), storm::exceptions::UnmetRequirementException, "The model is not acyclic.");
                        stack.push_back(entry.getColumn());
                    }
                }
                // If there are no successors to process, we will add the current state to the result in the next iteration.
            }
        }
    }
    // we will do backwards iterations, so the order has to be reversed
    if (orderedMatrixRequired) {
        std::reverse(result.begin(), result.end());
        STORM_LOG_ASSERT(result.size() == matrix.getRowGroupCount(), "Result vector has an unexpected amount of entries.");
        return result;
    } else {
        return boost::none;
    }
}

/// reorders the row group such that the i'th row of the new matrix corresponds to the order[i]'th row of the source matrix.
/// Also eliminates selfloops p>0 and inserts 1/p into the bFactors
template<typename ValueType>
storm::storage::SparseMatrix<ValueType> createReorderedMatrix(storm::storage::SparseMatrix<ValueType> const& matrix,
                                                              std::vector<uint64_t> const& newToOrigIndexMap,
                                                              std::vector<std::pair<uint64_t, ValueType>>& bFactors) {
    std::vector<uint64_t> origToNewMap(newToOrigIndexMap.size(), std::numeric_limits<uint64_t>::max());
    for (uint64_t i = 0; i < newToOrigIndexMap.size(); ++i) {
        origToNewMap[newToOrigIndexMap[i]] = i;
    }

    bool hasRowGrouping = !matrix.hasTrivialRowGrouping();
    storm::storage::SparseMatrixBuilder<ValueType> builder(matrix.getRowCount(), matrix.getColumnCount(), matrix.getNonzeroEntryCount(), false, hasRowGrouping,
                                                           hasRowGrouping ? matrix.getRowGroupCount() : static_cast<uint64_t>(0));
    uint64_t newRow = 0;
    for (uint64_t newRowGroup = 0; newRowGroup < newToOrigIndexMap.size(); ++newRowGroup) {
        auto const& origRowGroup = newToOrigIndexMap[newRowGroup];
        if (hasRowGrouping) {
            builder.newRowGroup(newRow);
        }
        for (uint64_t origRow = matrix.getRowGroupIndices()[origRowGroup]; origRow < matrix.getRowGroupIndices()[origRowGroup + 1]; ++origRow) {
            for (auto const& entry : matrix.getRow(origRow)) {
                if (storm::utility::isZero(entry.getValue())) {
                    continue;
                }
                if (entry.getColumn() == origRowGroup) {
                    if (storm::utility::isOne(entry.getValue())) {
                        // a one selfloop can only mean that there is never a non-zero value at the b vector for the current row.
                        // Let's "assert" this by considering infinity. (This is necessary to avoid division by zero)
                        bFactors.emplace_back(newRow, storm::utility::infinity<ValueType>());
                    }
                    ValueType factor = storm::utility::one<ValueType>() / (storm::utility::one<ValueType>() - entry.getValue());
                    bFactors.emplace_back(newRow, factor);
                }
                builder.addNextValue(newRow, origToNewMap[entry.getColumn()], entry.getValue());
            }
            ++newRow;
        }
    }
    auto result = builder.build(matrix.getRowCount(), matrix.getColumnCount(), matrix.getRowGroupCount());
    // apply the bFactors to the relevant rows
    for (auto const& bFactor : bFactors) {
        STORM_LOG_ASSERT(!storm::utility::isInfinity(bFactor.second) || storm::utility::isZero(result.getRowSum(bFactor.first)),
                         "The input matrix does not seem to be probabilistic.");
        for (auto& entry : result.getRow(bFactor.first)) {
            entry.setValue(entry.getValue() * bFactor.second);
        }
    }
    STORM_LOG_DEBUG("Reordered " << matrix.getDimensionsAsString() << " with " << bFactors.size() << " selfloop entries for acyclic solving.");
    return result;
}
}  // namespace helper
}  // namespace solver
}  // namespace storm
