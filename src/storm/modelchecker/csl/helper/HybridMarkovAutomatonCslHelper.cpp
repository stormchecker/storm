#include "storm/modelchecker/csl/helper/HybridMarkovAutomatonCslHelper.h"

#include "storm/modelchecker/csl/helper/SparseMarkovAutomatonCslHelper.h"
#include "storm/modelchecker/prctl/helper/HybridMdpPrctlHelper.h"

#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"

#include "storm/solver/SolveGoal.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

#include "storm/models/symbolic/StandardRewardModel.h"

#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/modelchecker/results/HybridQuantitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQuantitativeCheckResult.h"

#include "storm/utility/Stopwatch.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<storm::dd::DdType DdType, class ValueType>
void discretizeRewardModel(typename storm::models::symbolic::Model<DdType, ValueType>::RewardModelType& rewardModel,
                           storm::dd::Add<DdType, ValueType> const& exitRateVector, storm::dd::Bdd<DdType> const& markovianStates) {
    if (rewardModel.hasStateRewards()) {
        rewardModel.getStateRewardVector() *= markovianStates.ite(exitRateVector.getDdManager().template getAddOne<ValueType>() / exitRateVector,
                                                                  exitRateVector.getDdManager().template getAddZero<ValueType>());
    }
}

template<storm::dd::DdType DdType, class ValueType>
std::unique_ptr<CheckResult> HybridMarkovAutomatonCslHelper::computeReachabilityRewards(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::MarkovAutomaton<DdType, ValueType> const& model,
    storm::dd::Add<DdType, ValueType> const& transitionMatrix, storm::dd::Bdd<DdType> const& markovianStates,
    storm::dd::Add<DdType, ValueType> const& exitRateVector, typename storm::models::symbolic::Model<DdType, ValueType>::RewardModelType const& rewardModel,
    storm::dd::Bdd<DdType> const& targetStates, bool qualitative) {
    auto discretizedRewardModel = rewardModel;
    discretizeRewardModel(discretizedRewardModel, exitRateVector, markovianStates);
    return HybridMdpPrctlHelper<DdType, ValueType>::computeReachabilityRewards(env, dir, model, transitionMatrix, discretizedRewardModel, targetStates,
                                                                               qualitative);
}

template<storm::dd::DdType DdType, typename ValueType, typename std::enable_if<storm::NumberTraits<ValueType>::SupportsExponential, int>::type>
std::unique_ptr<CheckResult> HybridMarkovAutomatonCslHelper::computeBoundedUntilProbabilities(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::MarkovAutomaton<DdType, ValueType> const& model,
    storm::dd::Add<DdType, ValueType> const& transitionMatrix, storm::dd::Bdd<DdType> const& markovianStates,
    storm::dd::Add<DdType, ValueType> const& exitRateVector, storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates, bool qualitative,
    double lowerBound, std::optional<double> const& upperBound) {
    // If the time bounds are [0, inf], we rather call untimed reachability.
    if (storm::utility::isZero(lowerBound) && !upperBound) {
        return storm::modelchecker::helper::HybridMdpPrctlHelper<DdType, ValueType>::computeUntilProbabilities(env, dir, model, transitionMatrix, phiStates,
                                                                                                               psiStates, qualitative);
    }
    // If the interval is of the form [0,0], we can return the result directly
    if (upperBound && storm::utility::isZero(*upperBound)) {
        // In this case, the interval is of the form [0, 0].
        return std::unique_ptr<CheckResult>(
            new SymbolicQuantitativeCheckResult<DdType, ValueType>(model.getReachableStates(), psiStates.template toAdd<ValueType>()));
    }

    // If we reach this point, we convert this query to an instance for the sparse engine.
    storm::utility::Stopwatch conversionWatch(true);
    // Create ODD for the translation.
    storm::dd::Odd odd = model.getReachableStates().createOdd();
    storm::storage::SparseMatrix<ValueType> explicitTransitionMatrix = transitionMatrix.toMatrix(model.getNondeterminismVariables(), odd, odd);
    std::vector<ValueType> explicitExitRateVector = exitRateVector.toVector(odd);
    conversionWatch.stop();
    STORM_LOG_INFO("Converting symbolic matrix to explicit representation done in " << conversionWatch.getTimeInMilliseconds() << "ms.");

    auto explicitResult = storm::modelchecker::helper::SparseMarkovAutomatonCslHelper::computeBoundedUntilProbabilities(
        env, storm::solver::SolveGoal<ValueType>(dir), explicitTransitionMatrix, explicitExitRateVector, markovianStates.toVector(odd), phiStates.toVector(odd),
        psiStates.toVector(odd), {lowerBound, upperBound});
    return std::unique_ptr<CheckResult>(new HybridQuantitativeCheckResult<DdType, ValueType>(
        model.getReachableStates(), model.getManager().getBddZero(), model.getManager().template getAddZero<ValueType>(), model.getReachableStates(),
        std::move(odd), std::move(explicitResult)));
}

template<storm::dd::DdType DdType, typename ValueType, typename std::enable_if<!storm::NumberTraits<ValueType>::SupportsExponential, int>::type>
std::unique_ptr<CheckResult> HybridMarkovAutomatonCslHelper::computeBoundedUntilProbabilities(
    Environment const&, OptimizationDirection, storm::models::symbolic::MarkovAutomaton<DdType, ValueType> const&, storm::dd::Add<DdType, ValueType> const&,
    storm::dd::Bdd<DdType> const&, storm::dd::Add<DdType, ValueType> const&, storm::dd::Bdd<DdType> const&, storm::dd::Bdd<DdType> const&, bool, double,
    std::optional<double> const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Computing bounded until probabilities is unsupported for this value type.");
}

// Explicit instantiations.

// Cudd, double.
template std::unique_ptr<CheckResult> HybridMarkovAutomatonCslHelper::computeReachabilityRewards(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::CUDD, double> const& model,
    storm::dd::Add<storm::dd::DdType::CUDD, double> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::CUDD> const& markovianStates,
    storm::dd::Add<storm::dd::DdType::CUDD, double> const& exitRateVector,
    typename storm::models::symbolic::Model<storm::dd::DdType::CUDD, double>::RewardModelType const& rewardModel,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& targetStates, bool qualitative);
template std::unique_ptr<CheckResult> HybridMarkovAutomatonCslHelper::computeBoundedUntilProbabilities(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::CUDD, double> const& model,
    storm::dd::Add<storm::dd::DdType::CUDD, double> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::CUDD> const& markovianStates,
    storm::dd::Add<storm::dd::DdType::CUDD, double> const& exitRateVector, storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates, bool qualitative, double lowerBound, std::optional<double> const& upperBound);

// Sylvan, double.
template std::unique_ptr<CheckResult> HybridMarkovAutomatonCslHelper::computeReachabilityRewards(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::Sylvan, double> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, double> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& markovianStates,
    storm::dd::Add<storm::dd::DdType::Sylvan, double> const& exitRateVector,
    typename storm::models::symbolic::Model<storm::dd::DdType::Sylvan, double>::RewardModelType const& rewardModel,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& targetStates, bool qualitative);
template std::unique_ptr<CheckResult> HybridMarkovAutomatonCslHelper::computeBoundedUntilProbabilities(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::Sylvan, double> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, double> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& markovianStates,
    storm::dd::Add<storm::dd::DdType::Sylvan, double> const& exitRateVector, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, bool qualitative, double lowerBound, std::optional<double> const& upperBound);

// Sylvan, rational number.
template std::unique_ptr<CheckResult> HybridMarkovAutomatonCslHelper::computeReachabilityRewards(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& markovianStates,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& exitRateVector,
    typename storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalNumber>::RewardModelType const& rewardModel,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& targetStates, bool qualitative);
template std::unique_ptr<CheckResult> HybridMarkovAutomatonCslHelper::computeBoundedUntilProbabilities(
    Environment const& env, OptimizationDirection dir, storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& transitionMatrix, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& markovianStates,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& exitRateVector, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, bool qualitative, double lowerBound, std::optional<double> const& upperBound);

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
