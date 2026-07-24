#pragma once

#include "storm/models/symbolic/NondeterministicModel.h"

#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"

#include "storm/solver/SolveGoal.h"
#include "storm/solver/SymbolicMinMaxLinearEquationSolver.h"

namespace storm {

class Environment;

namespace modelchecker {
// Forward-declare result class.
class CheckResult;

namespace helper {

template<storm::dd::DdType DdType, typename ValueType>
class SymbolicMdpPrctlHelper {
   public:
    typedef typename storm::models::symbolic::NondeterministicModel<DdType, ValueType>::RewardModelType RewardModelType;

    static std::unique_ptr<CheckResult> computeBoundedUntilProbabilities(Environment const& env, OptimizationDirection dir,
                                                                         storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
                                                                         storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                         storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates,
                                                                         uint_fast64_t stepBound);

    static std::unique_ptr<CheckResult> computeNextProbabilities(Environment const& env, OptimizationDirection dir,
                                                                 storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
                                                                 storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                 storm::dd::Bdd<DdType> const& nextStates);

    static std::unique_ptr<CheckResult> computeUntilProbabilities(Environment const& env, OptimizationDirection dir,
                                                                  storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
                                                                  storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                  storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates,
                                                                  bool qualitative,
                                                                  boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues = boost::none);

    static std::unique_ptr<CheckResult> computeUntilProbabilities(Environment const& env, OptimizationDirection dir,
                                                                  storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
                                                                  storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                  storm::dd::Bdd<DdType> const& maybeStates,
                                                                  storm::dd::Bdd<DdType> const& statesWithProbability1,
                                                                  boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues = boost::none);

    static std::unique_ptr<CheckResult> computeGloballyProbabilities(Environment const& env, OptimizationDirection dir,
                                                                     storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
                                                                     storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                     storm::dd::Bdd<DdType> const& psiStates, bool qualitative);

    static std::unique_ptr<CheckResult> computeCumulativeRewards(Environment const& env, OptimizationDirection dir,
                                                                 storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
                                                                 storm::dd::Add<DdType, ValueType> const& transitionMatrix, RewardModelType const& rewardModel,
                                                                 uint_fast64_t stepBound);

    static std::unique_ptr<CheckResult> computeInstantaneousRewards(Environment const& env, OptimizationDirection dir,
                                                                    storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
                                                                    storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                    RewardModelType const& rewardModel, uint_fast64_t stepBound);

    static std::unique_ptr<CheckResult> computeReachabilityRewards(Environment const& env, OptimizationDirection dir,
                                                                   storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
                                                                   storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                   RewardModelType const& rewardModel, storm::dd::Bdd<DdType> const& targetStates,
                                                                   boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues = boost::none);

    static std::unique_ptr<CheckResult> computeReachabilityRewards(Environment const& env, OptimizationDirection dir,
                                                                   storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
                                                                   storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                   storm::dd::Bdd<DdType> const& transitionMatrixBdd, RewardModelType const& rewardModel,
                                                                   storm::dd::Bdd<DdType> const& maybeStates, storm::dd::Bdd<DdType> const& targetStates,
                                                                   storm::dd::Bdd<DdType> const& infinityStates,
                                                                   boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues = boost::none);

    static std::unique_ptr<CheckResult> computeReachabilityTimes(Environment const& env, OptimizationDirection dir,
                                                                 storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
                                                                 storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                 storm::dd::Bdd<DdType> const& targetStates,
                                                                 boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues = boost::none);
};

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
