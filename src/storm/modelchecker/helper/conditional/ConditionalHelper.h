#pragma once

#include <memory>
#include "storm/logic/ConditionalFormula.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/solver/SolveGoal.h"

namespace storm {
class Environment;

namespace storage {
class BitVector;
template<typename ValueType>
class SparseMatrix;
}  // namespace storage

namespace modelchecker {
class CheckResult;

namespace utility {
template<typename ValueType>
class BackwardTransitionCache;
}

template<typename ValueType, typename SolutionType = ValueType>
std::unique_ptr<CheckResult> computeConditionalProbabilities(Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal,
                                                             bool produceSchedulers, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                             storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                             storm::storage::BitVector const& targetStates, storm::storage::BitVector const& conditionStates);

}  // namespace modelchecker
}  // namespace storm
