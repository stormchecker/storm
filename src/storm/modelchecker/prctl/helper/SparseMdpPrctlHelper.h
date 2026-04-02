#pragma once

#include <vector>

#include "storm/adapters/IntervalForward.h"
#include "storm/modelchecker/hints/ModelCheckerHint.h"
#include "storm/modelchecker/prctl/helper/MDPModelCheckingHelperReturnType.h"
#include "storm/modelchecker/prctl/helper/rewardbounded/MultiDimensionalRewardUnfolding.h"
#include "storm/solver/SolveGoal.h"
#include "storm/solver/UncertaintyResolutionMode.h"
#include "storm/storage/MaximalEndComponent.h"
#include "storm/storage/SparseMatrix.h"

namespace storm {

class Environment;

namespace storage {
class BitVector;
}

namespace models {
namespace sparse {
template<typename ValueType>
class StandardRewardModel;
}
}  // namespace models

namespace modelchecker {
class CheckResult;

namespace helper {

template<typename ValueType, typename SolutionType = ValueType>
class SparseMdpPrctlHelper {
   public:
    static std::map<storm::storage::sparse::state_type, SolutionType> computeRewardBoundedValues(
        Environment const& env, OptimizationDirection dir, rewardbounded::MultiDimensionalRewardUnfolding<ValueType, true>& rewardUnfolding,
        storm::storage::BitVector const& initialStates);

    static std::vector<SolutionType> computeNextProbabilities(Environment const& env, OptimizationDirection dir,
                                                              UncertaintyResolutionMode uncertaintyResolutionMode,
                                                              storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                              storm::storage::BitVector const& nextStates);

    static MDPSparseModelCheckingHelperReturnType<SolutionType> computeUntilProbabilities(
        Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
        storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& phiStates,
        storm::storage::BitVector const& psiStates, bool qualitative, bool produceScheduler, ModelCheckerHint const& hint = ModelCheckerHint());

    static MDPSparseModelCheckingHelperReturnType<SolutionType> computeGloballyProbabilities(Environment const& env,
                                                                                             storm::solver::SolveGoal<ValueType, SolutionType>&& goal,
                                                                                             storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                             storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                             storm::storage::BitVector const& psiStates, bool qualitative,
                                                                                             bool produceScheduler, bool useMecBasedTechnique = false);

    template<typename RewardModelType>
    static std::vector<SolutionType> computeInstantaneousRewards(Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal,
                                                                 storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                 RewardModelType const& rewardModel, uint_fast64_t stepCount);

    template<typename RewardModelType>
    static std::vector<SolutionType> computeCumulativeRewards(Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal,
                                                              storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                              RewardModelType const& rewardModel, uint_fast64_t stepBound);

    template<typename RewardModelType>
    static MDPSparseModelCheckingHelperReturnType<SolutionType> computeTotalRewards(Environment const& env,
                                                                                    storm::solver::SolveGoal<ValueType, SolutionType>&& goal,
                                                                                    storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                    storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                    RewardModelType const& rewardModel, bool qualitative, bool produceScheduler,
                                                                                    ModelCheckerHint const& hint = ModelCheckerHint());
    template<typename RewardModelType>
    static std::vector<SolutionType> computeDiscountedCumulativeRewards(Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal,
                                                                        storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                        RewardModelType const& rewardModel, uint_fast64_t stepBound, ValueType discountFactor);

    template<typename RewardModelType>
    static MDPSparseModelCheckingHelperReturnType<SolutionType> computeDiscountedTotalRewards(
        Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
        storm::storage::SparseMatrix<ValueType> const& backwardTransitions, RewardModelType const& rewardModel, bool qualitative, bool produceScheduler,
        ValueType discountFactor, ModelCheckerHint const& hint = ModelCheckerHint());

    template<typename RewardModelType>
    static MDPSparseModelCheckingHelperReturnType<SolutionType> computeReachabilityRewards(
        Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
        storm::storage::SparseMatrix<ValueType> const& backwardTransitions, RewardModelType const& rewardModel, storm::storage::BitVector const& targetStates,
        bool qualitative, bool produceScheduler, ModelCheckerHint const& hint = ModelCheckerHint());

    static MDPSparseModelCheckingHelperReturnType<SolutionType> computeReachabilityTimes(
        Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
        storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& targetStates, bool qualitative,
        bool produceScheduler, ModelCheckerHint const& hint = ModelCheckerHint());

    static std::vector<SolutionType> computeReachabilityRewards(Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal,
                                                                storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                storm::models::sparse::StandardRewardModel<storm::Interval> const& intervalRewardModel,
                                                                bool lowerBoundOfIntervals, storm::storage::BitVector const& targetStates, bool qualitative);

    static std::vector<SolutionType> computeReachabilityRewards(Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal,
                                                                storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                storm::models::sparse::StandardRewardModel<storm::RationalInterval> const& intervalRewardModel,
                                                                bool lowerBoundOfIntervals, storm::storage::BitVector const& targetStates, bool qualitative);

   private:
    static MDPSparseModelCheckingHelperReturnType<SolutionType> computeReachabilityRewardsHelper(
        Environment const& env, storm::solver::SolveGoal<ValueType, SolutionType>&& goal, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
        storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
        std::function<std::vector<ValueType>(uint_fast64_t, storm::storage::SparseMatrix<ValueType> const&, storm::storage::BitVector const&)> const&
            totalStateRewardVectorGetter,
        storm::storage::BitVector const& targetStates, bool qualitative, bool produceScheduler,
        std::function<storm::storage::BitVector()> const& zeroRewardStatesGetter, std::function<storm::storage::BitVector()> const& zeroRewardChoicesGetter,
        ModelCheckerHint const& hint = ModelCheckerHint());
};

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
