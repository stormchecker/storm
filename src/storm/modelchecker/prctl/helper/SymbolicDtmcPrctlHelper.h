#pragma once

#include "storm/models/symbolic/Model.h"

#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"

#include "storm/solver/SymbolicLinearEquationSolver.h"

namespace storm {

class Environment;

namespace modelchecker {
namespace helper {

template<storm::dd::DdType DdType, typename ValueType>
class SymbolicDtmcPrctlHelper {
   public:
    typedef typename storm::models::symbolic::Model<DdType, ValueType>::RewardModelType RewardModelType;

    static storm::dd::Add<DdType, ValueType> computeBoundedUntilProbabilities(Environment const& env,
                                                                              storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                              storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                              storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates,
                                                                              uint_fast64_t stepBound);

    static storm::dd::Add<DdType, ValueType> computeNextProbabilities(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                      storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                      storm::dd::Bdd<DdType> const& nextStates);

    static storm::dd::Add<DdType, ValueType> computeUntilProbabilities(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                       storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                       storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates,
                                                                       bool qualitative,
                                                                       boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues = boost::none);

    static storm::dd::Add<DdType, ValueType> computeUntilProbabilities(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                       storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                       storm::dd::Bdd<DdType> const& maybeStates,
                                                                       storm::dd::Bdd<DdType> const& statesWithProbability1,
                                                                       boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues = boost::none);

    static storm::dd::Add<DdType, ValueType> computeGloballyProbabilities(Environment const& env,
                                                                          storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                          storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                          storm::dd::Bdd<DdType> const& psiStates, bool qualitative);

    static storm::dd::Add<DdType, ValueType> computeCumulativeRewards(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                      storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                      RewardModelType const& rewardModel, uint_fast64_t stepBound);

    static storm::dd::Add<DdType, ValueType> computeInstantaneousRewards(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                         storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                         RewardModelType const& rewardModel, uint_fast64_t stepBound);

    static storm::dd::Add<DdType, ValueType> computeReachabilityRewards(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                        storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                        RewardModelType const& rewardModel, storm::dd::Bdd<DdType> const& targetStates,
                                                                        bool qualitative,
                                                                        boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues = boost::none);

    static storm::dd::Add<DdType, ValueType> computeReachabilityRewards(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                        storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                        RewardModelType const& rewardModel, storm::dd::Bdd<DdType> const& maybeStates,
                                                                        storm::dd::Bdd<DdType> const& targetStates,
                                                                        storm::dd::Bdd<DdType> const& infinityStates,
                                                                        boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues = boost::none);

    static storm::dd::Add<DdType, ValueType> computeReachabilityTimes(Environment const& env, storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                      storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                      storm::dd::Bdd<DdType> const& targetStates, bool qualitative,
                                                                      boost::optional<storm::dd::Add<DdType, ValueType>> const& startValues = boost::none);
};

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
