#pragma once

#include "storm/modelchecker/hints/ModelCheckerHint.h"
#include "storm/modelchecker/prctl/helper/SemanticSolutionType.h"
#include "storm/solver/SolveGoal.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/solver.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType, typename SolutionType = ValueType>
class SparseNondeterministicStepBoundedHorizonHelper {
   public:
    SparseNondeterministicStepBoundedHorizonHelper();
    std::vector<SolutionType> compute(Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal,
                                      storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                      storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& phiStates,
                                      storm::storage::BitVector const& psiStates, uint64_t lowerBound, uint64_t upperBound,
                                      ModelCheckerHint const& hint = ModelCheckerHint());

   private:
};

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
