#pragma once

#include <optional>

#include <memory>

#include "storm/models/symbolic/MarkovAutomaton.h"

#include "storm/modelchecker/results/CheckResult.h"

#include "storm/solver/OptimizationDirection.h"
#include "storm/utility/NumberTraits.h"

namespace storm {

class Environment;

namespace modelchecker {
namespace helper {

class HybridMarkovAutomatonCslHelper {
   public:
    template<storm::dd::DdType DdType, typename ValueType>
    static std::unique_ptr<CheckResult> computeReachabilityRewards(
        Environment const& env, OptimizationDirection dir, storm::models::symbolic::MarkovAutomaton<DdType, ValueType> const& model,
        storm::dd::Add<DdType, ValueType> const& transitionMatrix, storm::dd::Bdd<DdType> const& markovianStates,
        storm::dd::Add<DdType, ValueType> const& exitRateVector, typename storm::models::symbolic::Model<DdType, ValueType>::RewardModelType const& rewardModel,
        storm::dd::Bdd<DdType> const& targetStates, bool qualitative);

    template<storm::dd::DdType DdType, typename ValueType, typename std::enable_if<storm::NumberTraits<ValueType>::SupportsExponential, int>::type = 0>
    static std::unique_ptr<CheckResult> computeBoundedUntilProbabilities(Environment const& env, OptimizationDirection dir,
                                                                         storm::models::symbolic::MarkovAutomaton<DdType, ValueType> const& model,
                                                                         storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                         storm::dd::Bdd<DdType> const& markovianStates,
                                                                         storm::dd::Add<DdType, ValueType> const& exitRateVector,
                                                                         storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates,
                                                                         bool qualitative, double lowerBound, std::optional<double> const& upperBound);

    template<storm::dd::DdType DdType, typename ValueType, typename std::enable_if<!storm::NumberTraits<ValueType>::SupportsExponential, int>::type = 0>
    static std::unique_ptr<CheckResult> computeBoundedUntilProbabilities(Environment const& env, OptimizationDirection dir,
                                                                         storm::models::symbolic::MarkovAutomaton<DdType, ValueType> const& model,
                                                                         storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                         storm::dd::Bdd<DdType> const& markovianStates,
                                                                         storm::dd::Add<DdType, ValueType> const& exitRateVector,
                                                                         storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates,
                                                                         bool qualitative, double lowerBound, std::optional<double> const& upperBound);
};

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
