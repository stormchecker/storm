#include "storm/modelchecker/certificates/utility/LowerUpperBoundsComputer.h"

#include <algorithm>
#include <functional>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/modelchecker/prctl/helper/BaierUpperRewardBoundsComputer.h"
#include "storm/modelchecker/prctl/helper/DsMpiUpperRewardBoundsComputer.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm::modelchecker {

template<typename ValueType, bool Lower>
std::vector<ValueType> computeLowerOrUpperBoundsTotalReward(storm::storage::SparseMatrix<ValueType> const& matrix, std::vector<ValueType> const& offsets,
                                                            storm::storage::BitVector const& exitRows, std::vector<ValueType> const& exitProbabilities,
                                                            std::optional<storm::OptimizationDirection> dir) {
    STORM_LOG_ASSERT(matrix.hasTrivialRowGrouping() || dir.has_value(), "Optimization direction needs to be given if matrix is nondeterministic.");
    storm::utility::Extremum<Lower ? storm::OptimizationDirection::Minimize : storm::OptimizationDirection::Maximize, ValueType> globalBound;
    // Try to compute a lower and an upper bound the "easy" way
    auto const insideRows = ~exitRows;
    std::conditional_t<Lower, std::greater_equal<ValueType>, std::less_equal<ValueType>> comp;
    bool const canEasy =
        std::all_of(insideRows.begin(), insideRows.end(), [&offsets, &comp](uint64_t row) { return comp(offsets[row], storm::utility::zero<ValueType>()); });

    if (canEasy) {
        for (auto const row : exitRows) {
            STORM_LOG_ASSERT(!storm::utility::isZero(exitProbabilities.at(row)), "Exit probability must not be zero.");
            ValueType const rowValue = offsets[row] / exitProbabilities[row];
            globalBound &= rowValue;
        }
    }

    // We might have to invoke the respective reward bound computers.
    if (!canEasy) {
        if (dir.has_value() && (Lower == minimize(*dir))) {
            // Baier's algorithm provides a lower and an upper bound that hold for all strategies. It supports positive and negative rewards.
            auto const bounds =
                storm::modelchecker::helper::BaierUpperRewardBoundsComputer<ValueType>(matrix, exitProbabilities).computeTotalRewardBounds(offsets);
            globalBound &= Lower ? bounds.lower : bounds.upper;
        } else {
            // For lower bounds, we actually compute upper bounds for the negated rewards.
            // For upper bounds, we potentially cut away negative offsets.
            std::optional<std::vector<ValueType>> tmpOffsets;
            if (Lower) {
                tmpOffsets.emplace(offsets.size());
                storm::utility::vector::applyPointwise(offsets, *tmpOffsets,
                                                       [](ValueType const& v) { return std::max<ValueType>(storm::utility::zero<ValueType>(), -v); });
            } else if (std::any_of(offsets.begin(), offsets.end(), [](ValueType const& v) { return v < storm::utility::zero<ValueType>(); })) {
                tmpOffsets.emplace(offsets.size());
                storm::utility::vector::applyPointwise(offsets, *tmpOffsets, [](ValueType const& v) { return std::max(storm::utility::zero<ValueType>(), v); });
            }
            auto bounds = storm::modelchecker::helper::DsMpiMdpUpperRewardBoundsComputer<ValueType>(
                              matrix, tmpOffsets.has_value() ? tmpOffsets.value() : offsets, exitProbabilities)
                              .computeUpperBounds();
            if (Lower) {
                storm::utility::vector::applyPointwise(bounds, bounds, [](ValueType const& v) { return -v; });
            }
            return bounds;
        }
    }
    STORM_LOG_ASSERT(!globalBound.empty(), "Could not compute lower or upper bound for total reward.");
    return std::vector<ValueType>(matrix.getRowGroupCount(), *globalBound);
}

template std::vector<double> computeLowerOrUpperBoundsTotalReward<double, true>(storm::storage::SparseMatrix<double> const&, std::vector<double> const&,
                                                                                storm::storage::BitVector const&, std::vector<double> const&,
                                                                                std::optional<storm::OptimizationDirection>);
template std::vector<double> computeLowerOrUpperBoundsTotalReward<double, false>(storm::storage::SparseMatrix<double> const&, std::vector<double> const&,
                                                                                 storm::storage::BitVector const&, std::vector<double> const&,
                                                                                 std::optional<storm::OptimizationDirection>);
template std::vector<storm::RationalNumber> computeLowerOrUpperBoundsTotalReward<storm::RationalNumber, true>(
    storm::storage::SparseMatrix<storm::RationalNumber> const&, std::vector<storm::RationalNumber> const&, storm::storage::BitVector const&,
    std::vector<storm::RationalNumber> const&, std::optional<storm::OptimizationDirection>);
template std::vector<storm::RationalNumber> computeLowerOrUpperBoundsTotalReward<storm::RationalNumber, false>(
    storm::storage::SparseMatrix<storm::RationalNumber> const&, std::vector<storm::RationalNumber> const&, storm::storage::BitVector const&,
    std::vector<storm::RationalNumber> const&, std::optional<storm::OptimizationDirection>);

}  // namespace storm::modelchecker
