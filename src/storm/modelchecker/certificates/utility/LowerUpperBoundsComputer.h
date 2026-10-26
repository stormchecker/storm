#pragma once

#include <optional>
#include <vector>

#include "storm/solver/OptimizationDirection.h"

namespace storm::storage {
class BitVector;
template<typename ValueType>
class SparseMatrix;
}  // namespace storm::storage

namespace storm::modelchecker {

/*!
 * Computes for each state a lower (if Lower is true) or an upper (otherwise) bound on the optimal total reward that is collected in the given subsystem.
 * @param matrix the transitions of the subsystem
 * @param offsets the reward collected for each row (including the value obtained when leaving the subsystem)
 * @param exitRows those rows that might leave the subsystem
 * @param exitProbabilities for each row, the probability to leave the subsystem
 * @param dir the optimization direction. Must be given for nondeterministic matrices.
 */
template<typename ValueType, bool Lower>
std::vector<ValueType> computeLowerOrUpperBoundsTotalReward(storm::storage::SparseMatrix<ValueType> const& matrix, std::vector<ValueType> const& offsets,
                                                            storm::storage::BitVector const& exitRows, std::vector<ValueType> const& exitProbabilities,
                                                            std::optional<storm::OptimizationDirection> dir = {});

}  // namespace storm::modelchecker
