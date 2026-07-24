#pragma once

#include "storm/models/symbolic/Model.h"

#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"

#include "storm/solver/LinearEquationSolver.h"

namespace storm {

class Environment;

namespace modelchecker {
// Forward-declare result class.
class CheckResult;

namespace helper {

template<storm::dd::DdType DdType, typename ValueType>
class HybridDtmcPrctlHelper {
   public:
    typedef typename storm::models::symbolic::Model<DdType, ValueType>::RewardModelType RewardModelType;

    static std::unique_ptr<CheckResult> computeBoundedUntilProbabilities(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                         storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                         storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates,
                                                                         uint_fast64_t stepBound);

    static std::unique_ptr<CheckResult> computeNextProbabilities(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                 storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                 storm::dd::Bdd<DdType> const& nextStates);

    static std::unique_ptr<CheckResult> computeUntilProbabilities(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                  storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                  storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates,
                                                                  bool qualitative);

    static std::unique_ptr<CheckResult> computeGloballyProbabilities(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                     storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                     storm::dd::Bdd<DdType> const& psiStates, bool qualitative);

    static std::unique_ptr<CheckResult> computeCumulativeRewards(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                 storm::dd::Add<DdType, ValueType> const& transitionMatrix, RewardModelType const& rewardModel,
                                                                 uint_fast64_t stepBound);

    static std::unique_ptr<CheckResult> computeInstantaneousRewards(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                    storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                    RewardModelType const& rewardModel, uint_fast64_t stepBound);

    static std::unique_ptr<CheckResult> computeReachabilityRewards(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                   storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                   RewardModelType const& rewardModel, storm::dd::Bdd<DdType> const& targetStates,
                                                                   bool qualitative);

    static std::unique_ptr<CheckResult> computeReachabilityTimes(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                 storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                 storm::dd::Bdd<DdType> const& targetStates, bool qualitative);
};

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
