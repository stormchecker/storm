#pragma once

#include <memory>

#include "storm/models/symbolic/Ctmc.h"

#include "storm/modelchecker/results/CheckResult.h"

#include "storm/solver/LinearEquationSolver.h"

#include "storm/utility/NumberTraits.h"

namespace storm {

class Environment;

namespace modelchecker::helper {

class HybridCtmcCslHelper {
   public:
    template<storm::dd::DdType DdType, typename ValueType>
        requires storm::NumberTraits<ValueType>::SupportsExponential
    static std::unique_ptr<CheckResult> computeBoundedUntilProbabilities(Environment const& env, storm::models::symbolic::Ctmc<DdType, ValueType> const& model,
                                                                         bool onlyInitialStatesRelevant, storm::dd::Add<DdType, ValueType> const& rateMatrix,
                                                                         storm::dd::Add<DdType, ValueType> const& exitRateVector,
                                                                         storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates,
                                                                         bool qualitative, ValueType lowerBound, ValueType upperBound);

    template<storm::dd::DdType DdType, typename ValueType>
        requires storm::NumberTraits<ValueType>::SupportsExponential
    static std::unique_ptr<CheckResult> computeInstantaneousRewards(
        Environment const& env, storm::models::symbolic::Ctmc<DdType, ValueType> const& model, bool onlyInitialStatesRelevant,
        storm::dd::Add<DdType, ValueType> const& rateMatrix, storm::dd::Add<DdType, ValueType> const& exitRateVector,
        typename storm::models::symbolic::Model<DdType, ValueType>::RewardModelType const& rewardModel, ValueType timeBound);

    template<storm::dd::DdType DdType, typename ValueType>
        requires storm::NumberTraits<ValueType>::SupportsExponential
    static std::unique_ptr<CheckResult> computeCumulativeRewards(Environment const& env, storm::models::symbolic::Ctmc<DdType, ValueType> const& model,
                                                                 bool onlyInitialStatesRelevant, storm::dd::Add<DdType, ValueType> const& rateMatrix,
                                                                 storm::dd::Add<DdType, ValueType> const& exitRateVector,
                                                                 typename storm::models::symbolic::Model<DdType, ValueType>::RewardModelType const& rewardModel,
                                                                 ValueType timeBound);

    template<storm::dd::DdType DdType, typename ValueType>
    static std::unique_ptr<CheckResult> computeUntilProbabilities(Environment const& env, storm::models::symbolic::Ctmc<DdType, ValueType> const& model,
                                                                  storm::dd::Add<DdType, ValueType> const& rateMatrix,
                                                                  storm::dd::Add<DdType, ValueType> const& exitRateVector,
                                                                  storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates,
                                                                  bool qualitative);

    template<storm::dd::DdType DdType, typename ValueType>
    static std::unique_ptr<CheckResult> computeReachabilityRewards(
        Environment const& env, storm::models::symbolic::Ctmc<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& rateMatrix,
        storm::dd::Add<DdType, ValueType> const& exitRateVector, typename storm::models::symbolic::Model<DdType, ValueType>::RewardModelType const& rewardModel,
        storm::dd::Bdd<DdType> const& targetStates, bool qualitative);

    template<storm::dd::DdType DdType, typename ValueType>
    static std::unique_ptr<CheckResult> computeNextProbabilities(Environment const& env, storm::models::symbolic::Ctmc<DdType, ValueType> const& model,
                                                                 storm::dd::Add<DdType, ValueType> const& rateMatrix,
                                                                 storm::dd::Add<DdType, ValueType> const& exitRateVector,
                                                                 storm::dd::Bdd<DdType> const& nextStates);

    /*!
     * Converts the given rate-matrix into a time-abstract probability matrix.
     *
     * @param model The symbolic model.
     * @param rateMatrix The rate matrix.
     * @param exitRateVector The exit rate vector of the model.
     * @return The probability matrix.
     */
    template<storm::dd::DdType DdType, typename ValueType>
    static storm::dd::Add<DdType, ValueType> computeProbabilityMatrix(storm::dd::Add<DdType, ValueType> const& rateMatrix,
                                                                      storm::dd::Add<DdType, ValueType> const& exitRateVector);

    /*!
     * Computes the matrix representing the transitions of the uniformized CTMC.
     *
     * @param transitionMatrix The matrix to uniformize.
     * @param exitRateVector The exit rate vector.
     * @param maybeStates The states that need to be considered.
     * @param uniformizationRate The rate to be used for uniformization.
     * @return The uniformized matrix.
     */
    template<storm::dd::DdType DdType, typename ValueType>
    static storm::dd::Add<DdType, ValueType> computeUniformizedMatrix(storm::models::symbolic::Ctmc<DdType, ValueType> const& model,
                                                                      storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                      storm::dd::Add<DdType, ValueType> const& exitRateVector,
                                                                      storm::dd::Bdd<DdType> const& maybeStates, ValueType uniformizationRate);
};

}  // namespace modelchecker::helper
}  // namespace storm
