#pragma once

#include "storm/modelchecker/prctl/helper/MDPModelCheckingHelperReturnType.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/SolveGoal.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/MaximalEndComponent.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/numerical.h"

namespace storm {

class Environment;

namespace modelchecker {
namespace helper {

class SparseMarkovAutomatonCslHelper {
   public:
    template<typename ValueType, typename std::enable_if<storm::NumberTraits<ValueType>::SupportsExponential, int>::type = 0>
    static std::vector<ValueType> computeBoundedUntilProbabilities(Environment const& env, storm::solver::SolveGoal<ValueType>&& goal,
                                                                   storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                   std::vector<ValueType> const& exitRateVector,
                                                                   storm::storage::BitVector const& markovianStates, storm::storage::BitVector const& phiStates,
                                                                   storm::storage::BitVector const& psiStates, std::pair<double, double> const& boundsPair);

    template<typename ValueType, typename std::enable_if<!storm::NumberTraits<ValueType>::SupportsExponential, int>::type = 0>
    static std::vector<ValueType> computeBoundedUntilProbabilities(Environment const& env, storm::solver::SolveGoal<ValueType>&& goal,
                                                                   storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                   std::vector<ValueType> const& exitRateVector,
                                                                   storm::storage::BitVector const& markovianStates, storm::storage::BitVector const& phiStates,
                                                                   storm::storage::BitVector const& psiStates, std::pair<double, double> const& boundsPair);

    template<typename ValueType>
    static MDPSparseModelCheckingHelperReturnType<ValueType> computeUntilProbabilities(Environment const& env, OptimizationDirection dir,
                                                                                       storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                       storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                       storm::storage::BitVector const& phiStates,
                                                                                       storm::storage::BitVector const& psiStates, bool qualitative,
                                                                                       bool produceScheduler);

    template<typename ValueType, typename RewardModelType>
    static MDPSparseModelCheckingHelperReturnType<ValueType> computeTotalRewards(Environment const& env, OptimizationDirection dir,
                                                                                 storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                 storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                 std::vector<ValueType> const& exitRateVector,
                                                                                 storm::storage::BitVector const& markovianStates,
                                                                                 RewardModelType const& rewardModel, bool produceScheduler);

    template<typename ValueType, typename RewardModelType>
    static MDPSparseModelCheckingHelperReturnType<ValueType> computeReachabilityRewards(Environment const& env, OptimizationDirection dir,
                                                                                        storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                        storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                        std::vector<ValueType> const& exitRateVector,
                                                                                        storm::storage::BitVector const& markovianStates,
                                                                                        RewardModelType const& rewardModel,
                                                                                        storm::storage::BitVector const& psiStates, bool produceScheduler);

    template<typename ValueType>
    static MDPSparseModelCheckingHelperReturnType<ValueType> computeReachabilityTimes(Environment const& env, OptimizationDirection dir,
                                                                                      storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                      storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                      std::vector<ValueType> const& exitRateVector,
                                                                                      storm::storage::BitVector const& markovianStates,
                                                                                      storm::storage::BitVector const& psiStates, bool produceScheduler);
};

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
