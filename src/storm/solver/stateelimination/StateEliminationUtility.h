#pragma once

#include <boost/optional.hpp>
#include <memory>
#include <vector>

#include "storm/adapters/RationalFunctionForward.h"
#include "storm/solver/stateelimination/EliminationOrder.h"
#include "storm/storage/sparse/StateType.h"

namespace storm {
namespace storage {
class BitVector;

template<typename ValueType>
class FlexibleSparseMatrix;

template<typename ValueType>
class SparseMatrix;
}  // namespace storage

namespace solver {
namespace stateelimination {

class StatePriorityQueue;

bool eliminationOrderNeedsDistances(EliminationOrder const& order);
bool eliminationOrderNeedsForwardDistances(EliminationOrder const& order);
bool eliminationOrderNeedsReversedDistances(EliminationOrder const& order);
bool eliminationOrderIsPenaltyBased(EliminationOrder const& order);
bool eliminationOrderIsStatic(EliminationOrder const& order);

template<typename ValueType>
uint_fast64_t estimateComplexity(ValueType const& value);

template<>
uint_fast64_t estimateComplexity(storm::RationalFunction const& value);

template<typename ValueType>
uint_fast64_t computeStatePenalty(storm::storage::sparse::state_type const& state, storm::storage::FlexibleSparseMatrix<ValueType> const& transitionMatrix,
                                  storm::storage::FlexibleSparseMatrix<ValueType> const& backwardTransitions,
                                  std::vector<ValueType> const& oneStepProbabilities);

template<typename ValueType>
uint_fast64_t computeStatePenaltyRegularExpression(storm::storage::sparse::state_type const& state,
                                                   storm::storage::FlexibleSparseMatrix<ValueType> const& transitionMatrix,
                                                   storm::storage::FlexibleSparseMatrix<ValueType> const& backwardTransitions,
                                                   std::vector<ValueType> const& oneStepProbabilities);

template<typename ValueType>
std::shared_ptr<StatePriorityQueue> createStatePriorityQueue(EliminationOrder const& order, boost::optional<std::vector<uint_fast64_t>> const& stateDistances,
                                                             storm::storage::FlexibleSparseMatrix<ValueType> const& transitionMatrix,
                                                             storm::storage::FlexibleSparseMatrix<ValueType> const& backwardTransitions,
                                                             std::vector<ValueType> const& oneStepProbabilities, storm::storage::BitVector const& states);

std::shared_ptr<StatePriorityQueue> createStatePriorityQueue(storm::storage::BitVector const& states);
std::shared_ptr<StatePriorityQueue> createStatePriorityQueue(std::vector<storm::storage::sparse::state_type> const& states);

template<typename ValueType>
std::vector<uint_fast64_t> getDistanceBasedPriorities(EliminationOrder const& order, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                      storm::storage::SparseMatrix<ValueType> const& transitionMatrixTransposed,
                                                      storm::storage::BitVector const& initialStates, std::vector<ValueType> const& oneStepProbabilities,
                                                      bool forward, bool reverse);

template<typename ValueType>
std::vector<uint_fast64_t> getStateDistances(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                             storm::storage::SparseMatrix<ValueType> const& transitionMatrixTransposed,
                                             storm::storage::BitVector const& initialStates, std::vector<ValueType> const& oneStepProbabilities, bool forward);

}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
