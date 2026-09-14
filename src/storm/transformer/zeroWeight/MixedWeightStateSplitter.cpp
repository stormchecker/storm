#include "storm/transformer/zeroWeight/MixedWeightStateSplitter.h"

#include <limits>
#include <numeric>
#include <utility>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::transformer {

template<typename ValueType>
typename MixedWeightStateSplitter<ValueType>::Result MixedWeightStateSplitter<ValueType>::split(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                                std::vector<ValueType> const& actionWeights,
                                                                                                storm::storage::BitVector const& targetStates,
                                                                                                storm::storage::BitVector const& initialStates) {
    uint64_t const numberOfStates = transitionMatrix.getRowGroupCount();
    uint64_t const numberOfRows = transitionMatrix.getRowCount();
    uint64_t const invalidIndex = std::numeric_limits<uint64_t>::max();
    STORM_LOG_THROW(transitionMatrix.getColumnCount() == numberOfStates, storm::exceptions::InvalidArgumentException,
                    "Expected one transition matrix column per state.");
    STORM_LOG_THROW(initialStates.size() == numberOfStates, storm::exceptions::InvalidArgumentException, "Expected one initial-state bit per state.");

    Result result;
    result.analysis = ZeroWeightAnalysis<ValueType>::analyze(transitionMatrix, actionWeights, targetStates);
    STORM_LOG_THROW((initialStates & result.analysis.statesWithZeroWeightChoices).empty(), storm::exceptions::InvalidArgumentException,
                    "Zero-weight transformations require non-target initial states to have only positive-weight actions.");

    auto const mixedStates = result.analysis.mixedZeroWeightStates;
    uint64_t const numberOfMixedStates = mixedStates.getNumberOfSetBits();
    uint64_t const newNumberOfStates = numberOfStates + numberOfMixedStates;
    uint64_t const newNumberOfRows = numberOfRows + numberOfMixedStates;
    result.targetStates = targetStates;
    result.targetStates.resize(newNumberOfStates, false);
    result.initialStates = initialStates;
    result.initialStates.resize(newNumberOfStates, false);
    result.positiveShells.assign(numberOfStates, invalidIndex);
    result.newToOldStateMapping.resize(newNumberOfStates);
    std::iota(result.newToOldStateMapping.begin(), result.newToOldStateMapping.begin() + numberOfStates, uint64_t{0});
    result.oldToNewRowMapping.resize(numberOfRows);
    result.newToOldRowMapping.reserve(newNumberOfRows);

    if (numberOfMixedStates == 0) {
        result.transitionMatrix = transitionMatrix;
        result.actionWeights = actionWeights;
        std::iota(result.oldToNewRowMapping.begin(), result.oldToNewRowMapping.end(), uint64_t{0});
        result.newToOldRowMapping = result.oldToNewRowMapping;
        return result;
    }

    uint64_t shell = numberOfStates;
    for (uint64_t state : mixedStates) {
        result.positiveShells[state] = shell;
        result.newToOldStateMapping[shell] = state;
        ++shell;
    }

    result.analysis.statesWithZeroWeightChoices.resize(newNumberOfStates, false);
    result.analysis.pureZeroWeightStates.resize(newNumberOfStates, false);
    result.analysis.mixedZeroWeightStates = storm::storage::BitVector(newNumberOfStates, false);
    result.analysis.positiveOnlyStates.resize(newNumberOfStates, false);
    result.analysis.stateToWeakComponent.resize(newNumberOfStates, invalidIndex);
    storm::storage::BitVector zeroWeightChoices(newNumberOfRows, false);
    storm::storage::SparseMatrixBuilder<ValueType> builder(newNumberOfRows, newNumberOfStates, transitionMatrix.getEntryCount() + numberOfMixedStates, true,
                                                           true, newNumberOfStates);
    result.actionWeights.reserve(newNumberOfRows);
    auto const& rowGroupIndices = transitionMatrix.getRowGroupIndices();
    uint64_t newRow = 0;
    auto copyRow = [&](uint64_t oldRow) {
        for (auto const& entry : transitionMatrix.getRow(oldRow)) {
            builder.addNextValue(newRow, entry.getColumn(), entry.getValue());
        }
        result.actionWeights.push_back(actionWeights[oldRow]);
        result.oldToNewRowMapping[oldRow] = newRow;
        result.newToOldRowMapping.push_back(oldRow);
        if (result.analysis.zeroWeightChoices.get(oldRow)) {
            zeroWeightChoices.set(newRow);
        }
        ++newRow;
    };

    for (uint64_t state = 0; state < numberOfStates; ++state) {
        builder.newRowGroup(newRow);
        for (uint64_t row = rowGroupIndices[state]; row < rowGroupIndices[state + 1]; ++row) {
            if (!mixedStates.get(state) || result.analysis.zeroWeightChoices.get(row)) {
                copyRow(row);
            }
        }
        if (mixedStates.get(state)) {
            builder.addNextValue(newRow, result.positiveShells[state], storm::utility::one<ValueType>());
            result.actionWeights.push_back(storm::utility::zero<ValueType>());
            result.newToOldRowMapping.push_back(invalidIndex);
            zeroWeightChoices.set(newRow);
            result.analysis.pureZeroWeightStates.set(state);
            ++newRow;
        }
    }

    for (uint64_t state : mixedStates) {
        builder.newRowGroup(newRow);
        for (uint64_t row = rowGroupIndices[state]; row < rowGroupIndices[state + 1]; ++row) {
            if (!result.analysis.zeroWeightChoices.get(row)) {
                copyRow(row);
            }
        }
        result.analysis.positiveOnlyStates.set(result.positiveShells[state]);
    }

    STORM_LOG_ASSERT(newRow == newNumberOfRows, "Unexpected row count after mixed-state splitting.");
    result.transitionMatrix = builder.build();
    result.analysis.zeroWeightChoices = std::move(zeroWeightChoices);
    return result;
}

template class MixedWeightStateSplitter<double>;
template class MixedWeightStateSplitter<storm::RationalNumber>;

}  // namespace storm::transformer
