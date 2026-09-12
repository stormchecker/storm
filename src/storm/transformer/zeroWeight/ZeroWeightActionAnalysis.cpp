#include "storm/transformer/zeroWeight/ZeroWeightActionAnalysis.h"

#include <cstdint>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::transformer {

template<typename ValueType>
typename ZeroWeightActionAnalysis<ValueType>::Result ZeroWeightActionAnalysis<ValueType>::analyze(
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType> const& actionWeights,
    storm::storage::BitVector const& targetStates) {
    STORM_LOG_THROW(actionWeights.size() == transitionMatrix.getRowCount(), storm::exceptions::InvalidArgumentException,
                    "Expected the action weight count to match the transition matrix row count (received " << actionWeights.size() << ", expected "
                                                                                                           << transitionMatrix.getRowCount() << ").");
    STORM_LOG_THROW(targetStates.size() == transitionMatrix.getRowGroupCount(), storm::exceptions::InvalidArgumentException,
                    "Expected the target-state bit count to match the transition matrix row-group count (received "
                        << targetStates.size() << ", expected " << transitionMatrix.getRowGroupCount() << ").");

    for (auto const& actionWeight : actionWeights) {
        STORM_LOG_THROW(storm::utility::isNonNegative(actionWeight), storm::exceptions::InvalidArgumentException,
                        "Zero-weight action analysis requires nonnegative action weights.");
    }

    Result result{storm::storage::BitVector(transitionMatrix.getRowCount(), false), storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false),
                  storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false), storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false),
                  storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false)};

    auto const& rowGroupIndices = transitionMatrix.getRowGroupIndices();
    for (uint64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
        if (targetStates.get(state)) {
            continue;
        }

        bool hasZeroWeightChoice = false;
        bool hasPositiveWeightChoice = false;
        for (uint64_t row = rowGroupIndices[state]; row < rowGroupIndices[state + 1]; ++row) {
            if (storm::utility::isZero(actionWeights[row])) {
                result.zeroWeightChoices.set(row);
                hasZeroWeightChoice = true;
            } else {
                hasPositiveWeightChoice = true;
            }
        }

        if (hasZeroWeightChoice) {
            result.statesWithZeroWeightChoices.set(state);
            if (hasPositiveWeightChoice) {
                result.mixedZeroWeightStates.set(state);
            } else {
                result.pureZeroWeightStates.set(state);
            }
        } else if (hasPositiveWeightChoice) {
            result.positiveOnlyStates.set(state);
        }
    }

    return result;
}

template class ZeroWeightActionAnalysis<double>;
template class ZeroWeightActionAnalysis<storm::RationalNumber>;

}  // namespace storm::transformer
