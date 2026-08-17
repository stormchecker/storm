#include "storm/modelchecker/csl/helper/HybridCtmcCslHelper.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/environment/solver/TimeBoundedSolverEnvironment.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/modelchecker/csl/helper/SparseCtmcCslHelper.h"
#include "storm/modelchecker/prctl/helper/HybridDtmcPrctlHelper.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/modelchecker/results/HybridQuantitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQuantitativeCheckResult.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<storm::dd::DdType DdType, class ValueType>
std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeReachabilityRewards(
    Environment const& env, storm::models::symbolic::Ctmc<DdType, ValueType> const& model, storm::dd::Add<DdType, ValueType> const& rateMatrix,
    storm::dd::Add<DdType, ValueType> const& exitRateVector, typename storm::models::symbolic::Model<DdType, ValueType>::RewardModelType const& rewardModel,
    storm::dd::Bdd<DdType> const& targetStates, bool qualitative) {
    return HybridDtmcPrctlHelper<DdType, ValueType>::computeReachabilityRewards(env, model, computeProbabilityMatrix(rateMatrix, exitRateVector),
                                                                                rewardModel.divideStateRewardVector(exitRateVector), targetStates, qualitative);
}

template<storm::dd::DdType DdType, class ValueType>
std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeNextProbabilities(Environment const& env,
                                                                           storm::models::symbolic::Ctmc<DdType, ValueType> const& model,
                                                                           storm::dd::Add<DdType, ValueType> const& rateMatrix,
                                                                           storm::dd::Add<DdType, ValueType> const& exitRateVector,
                                                                           storm::dd::Bdd<DdType> const& nextStates) {
    return HybridDtmcPrctlHelper<DdType, ValueType>::computeNextProbabilities(env, model, computeProbabilityMatrix(rateMatrix, exitRateVector), nextStates);
}

template<storm::dd::DdType DdType, class ValueType>
std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeUntilProbabilities(Environment const& env,
                                                                            storm::models::symbolic::Ctmc<DdType, ValueType> const& model,
                                                                            storm::dd::Add<DdType, ValueType> const& rateMatrix,
                                                                            storm::dd::Add<DdType, ValueType> const& exitRateVector,
                                                                            storm::dd::Bdd<DdType> const& phiStates, storm::dd::Bdd<DdType> const& psiStates,
                                                                            bool qualitative) {
    return HybridDtmcPrctlHelper<DdType, ValueType>::computeUntilProbabilities(env, model, computeProbabilityMatrix(rateMatrix, exitRateVector), phiStates,
                                                                               psiStates, qualitative);
}

template<storm::dd::DdType DdType, typename ValueType>
    requires storm::NumberTraits<ValueType>::SupportsExponential
std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeBoundedUntilProbabilities(
    Environment const& env, storm::models::symbolic::Ctmc<DdType, ValueType> const& model, bool onlyInitialStatesRelevant,
    storm::dd::Add<DdType, ValueType> const& rateMatrix, storm::dd::Add<DdType, ValueType> const& exitRateVector, storm::dd::Bdd<DdType> const& phiStates,
    storm::dd::Bdd<DdType> const& psiStates, bool qualitative, ValueType lowerBound, ValueType upperBound) {
    // If the time bounds are [0, inf], we rather call untimed reachability.
    if (storm::utility::isZero(lowerBound) && upperBound == storm::utility::infinity<ValueType>()) {
        return computeUntilProbabilities(env, model, rateMatrix, exitRateVector, phiStates, psiStates, qualitative);
    }

    if (env.solver().isForceSoundness() && env.solver().timeBounded().getRelativeTerminationCriterion()) {
        // Forward this query to the sparse engine
        storm::utility::Stopwatch conversionWatch(true);
        storm::dd::Odd odd = model.getReachableStates().createOdd();
        storm::storage::SparseMatrix<ValueType> explicitRateMatrix = rateMatrix.toMatrix(odd, odd);
        std::vector<ValueType> explicitExitRateVector = exitRateVector.toVector(odd);
        storm::solver::SolveGoal<ValueType> goal;
        if (onlyInitialStatesRelevant) {
            goal.setRelevantValues(model.getInitialStates().toVector(odd));
        }
        conversionWatch.stop();
        STORM_LOG_INFO("Converting symbolic matrix/vector to explicit representation done in " << conversionWatch.getTimeInMilliseconds() << "ms.");

        std::vector<ValueType> result = storm::modelchecker::helper::SparseCtmcCslHelper::computeBoundedUntilProbabilities<ValueType>(
            env, std::move(goal), explicitRateMatrix, explicitRateMatrix.transpose(true), phiStates.toVector(odd), psiStates.toVector(odd),
            explicitExitRateVector, qualitative, lowerBound, upperBound);

        return std::unique_ptr<CheckResult>(new HybridQuantitativeCheckResult<DdType, ValueType>(
            model.getReachableStates(), model.getManager().getBddZero(), model.getManager().template getAddZero<ValueType>(), model.getReachableStates(),
            std::move(odd), std::move(result)));
    }

    // Set the possible (absolute) error allowed for truncation (epsilon for fox-glynn)
    ValueType epsilon = storm::utility::convertNumber<ValueType>(env.solver().timeBounded().getPrecision()) / 8.0;

    // From this point on, we know that we have to solve a more complicated problem [t, t'] with either t != 0
    // or t' != inf.

    // If we identify the states that have probability 0 of reaching the target states, we can exclude them from the
    // further computations.
    storm::dd::Bdd<DdType> statesWithProbabilityGreater0 = storm::utility::graph::performProbGreater0(model, rateMatrix.notZero(), phiStates, psiStates);
    STORM_LOG_INFO("Found " << statesWithProbabilityGreater0.getNonZeroCount() << " states with probability greater 0.");
    storm::dd::Bdd<DdType> statesWithProbabilityGreater0NonPsi = statesWithProbabilityGreater0 && !psiStates;
    STORM_LOG_INFO("Found " << statesWithProbabilityGreater0NonPsi.getNonZeroCount() << " 'maybe' states.");

    if (!statesWithProbabilityGreater0NonPsi.isZero()) {
        if (storm::utility::isZero(upperBound)) {
            // In this case, the interval is of the form [0, 0].
            return std::unique_ptr<CheckResult>(
                new SymbolicQuantitativeCheckResult<DdType, ValueType>(model.getReachableStates(), psiStates.template toAdd<ValueType>()));
        } else {
            if (storm::utility::isZero(lowerBound)) {
                // In this case, the interval is of the form [0, t].
                // Note that this excludes [0, inf] since this is untimed reachability and we considered this case earlier.

                // Find the maximal rate of all 'maybe' states to take it as the uniformization rate.
                ValueType uniformizationRate = 1.02 * (statesWithProbabilityGreater0NonPsi.template toAdd<ValueType>() * exitRateVector).getMax();
                STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

                // Compute the uniformized matrix.
                storm::dd::Add<DdType, ValueType> uniformizedMatrix =
                    computeUniformizedMatrix(model, rateMatrix, exitRateVector, statesWithProbabilityGreater0NonPsi, uniformizationRate);

                // Compute the vector that is to be added as a compensation for removing the absorbing states.
                storm::dd::Add<DdType, ValueType> b = (statesWithProbabilityGreater0NonPsi.template toAdd<ValueType>() * rateMatrix *
                                                       psiStates.swapVariables(model.getRowColumnMetaVariablePairs()).template toAdd<ValueType>())
                                                          .sumAbstract(model.getColumnVariables()) /
                                                      model.getManager().getConstant(uniformizationRate);

                storm::utility::Stopwatch conversionWatch(true);

                // Create an ODD for the translation to an explicit representation.
                storm::dd::Odd odd = statesWithProbabilityGreater0NonPsi.createOdd();

                // Convert the symbolic parts to their explicit representation.
                storm::storage::SparseMatrix<ValueType> explicitUniformizedMatrix = uniformizedMatrix.toMatrix(odd, odd);
                std::vector<ValueType> explicitB = b.toVector(odd);
                conversionWatch.stop();
                STORM_LOG_INFO("Converting symbolic matrix/vector to explicit representation done in " << conversionWatch.getTimeInMilliseconds() << "ms.");

                // Finally compute the transient probabilities.
                std::vector<ValueType> values(statesWithProbabilityGreater0NonPsi.getNonZeroCount(), storm::utility::zero<ValueType>());
                std::vector<ValueType> subresult = storm::modelchecker::helper::SparseCtmcCslHelper::computeTransientProbabilities(
                    env, explicitUniformizedMatrix, &explicitB, upperBound, uniformizationRate, values, epsilon);

                return std::unique_ptr<CheckResult>(new HybridQuantitativeCheckResult<DdType>(
                    model.getReachableStates(), (psiStates || !statesWithProbabilityGreater0) && model.getReachableStates(),
                    psiStates.template toAdd<ValueType>(), statesWithProbabilityGreater0NonPsi, odd, subresult));
            } else if (upperBound == storm::utility::infinity<ValueType>()) {
                // In this case, the interval is of the form [t, inf] with t != 0.

                // Start by computing the (unbounded) reachability probabilities of reaching psi states while
                // staying in phi states.
                std::unique_ptr<CheckResult> unboundedResult =
                    computeUntilProbabilities(env, model, rateMatrix, exitRateVector, phiStates, psiStates, qualitative);

                // Compute the set of relevant states.
                storm::dd::Bdd<DdType> relevantStates = statesWithProbabilityGreater0 && phiStates;

                // Filter the unbounded result such that it only contains values for the relevant states.
                unboundedResult->filter(SymbolicQualitativeCheckResult<DdType>(model.getReachableStates(), relevantStates));

                storm::utility::Stopwatch conversionWatch;

                // Build an ODD for the relevant states.
                conversionWatch.start();
                storm::dd::Odd odd = relevantStates.createOdd();
                conversionWatch.stop();

                std::vector<ValueType> result;
                if (unboundedResult->isHybridQuantitativeCheckResult()) {
                    conversionWatch.start();
                    std::unique_ptr<CheckResult> explicitUnboundedResult =
                        unboundedResult->asHybridQuantitativeCheckResult<DdType, ValueType>().toExplicitQuantitativeCheckResult();
                    conversionWatch.stop();
                    result = std::move(explicitUnboundedResult->asExplicitQuantitativeCheckResult<ValueType>().getValueVector());
                } else {
                    STORM_LOG_THROW(unboundedResult->isSymbolicQuantitativeCheckResult(), storm::exceptions::InvalidStateException,
                                    "Expected check result of different type.");
                    result = unboundedResult->asSymbolicQuantitativeCheckResult<DdType, ValueType>().getValueVector().toVector(odd);
                }

                // Determine the uniformization rate for the transient probability computation.
                ValueType uniformizationRate = 1.02 * (relevantStates.template toAdd<ValueType>() * exitRateVector).getMax();

                // Compute the uniformized matrix.
                storm::dd::Add<DdType, ValueType> uniformizedMatrix =
                    computeUniformizedMatrix(model, rateMatrix, exitRateVector, relevantStates, uniformizationRate);
                conversionWatch.start();
                storm::storage::SparseMatrix<ValueType> explicitUniformizedMatrix = uniformizedMatrix.toMatrix(odd, odd);
                conversionWatch.stop();
                STORM_LOG_INFO("Converting symbolic matrix/vector to explicit representation done in " << conversionWatch.getTimeInMilliseconds() << "ms.");

                // Compute the transient probabilities.
                result = storm::modelchecker::helper::SparseCtmcCslHelper::computeTransientProbabilities<ValueType>(
                    env, explicitUniformizedMatrix, nullptr, lowerBound, uniformizationRate, result, epsilon);

                return std::unique_ptr<CheckResult>(
                    new HybridQuantitativeCheckResult<DdType>(model.getReachableStates(), !relevantStates && model.getReachableStates(),
                                                              model.getManager().template getAddZero<ValueType>(), relevantStates, odd, result));
            } else {
                // In this case, the interval is of the form [t, t'] with t != 0 and t' != inf.

                if (lowerBound != upperBound) {
                    // In this case, the interval is of the form [t, t'] with t != 0, t' != inf and t != t'.

                    // Find the maximal rate of all 'maybe' states to take it as the uniformization rate.
                    ValueType uniformizationRate = 1.02 * (statesWithProbabilityGreater0NonPsi.template toAdd<ValueType>() * exitRateVector).getMax();
                    STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

                    // Compute the (first) uniformized matrix.
                    storm::dd::Add<DdType, ValueType> uniformizedMatrix =
                        computeUniformizedMatrix(model, rateMatrix, exitRateVector, statesWithProbabilityGreater0NonPsi, uniformizationRate);

                    // Create the one-step vector.
                    storm::dd::Add<DdType, ValueType> b = (statesWithProbabilityGreater0NonPsi.template toAdd<ValueType>() * rateMatrix *
                                                           psiStates.swapVariables(model.getRowColumnMetaVariablePairs()).template toAdd<ValueType>())
                                                              .sumAbstract(model.getColumnVariables()) /
                                                          model.getManager().getConstant(uniformizationRate);

                    // Build an ODD for the relevant states and translate the symbolic parts to their explicit representation.
                    storm::utility::Stopwatch conversionWatch(true);
                    storm::dd::Odd odd = statesWithProbabilityGreater0NonPsi.createOdd();
                    storm::storage::SparseMatrix<ValueType> explicitUniformizedMatrix = uniformizedMatrix.toMatrix(odd, odd);
                    std::vector<ValueType> explicitB = b.toVector(odd);
                    conversionWatch.stop();

                    // Compute the transient probabilities.
                    std::vector<ValueType> values(statesWithProbabilityGreater0NonPsi.getNonZeroCount(), storm::utility::zero<ValueType>());
                    std::vector<ValueType> subResult = storm::modelchecker::helper::SparseCtmcCslHelper::computeTransientProbabilities(
                        env, explicitUniformizedMatrix, &explicitB, upperBound - lowerBound, uniformizationRate, values, epsilon);

                    // Transform the explicit result to a hybrid check result, so we can easily convert it to
                    // a symbolic qualitative format.
                    HybridQuantitativeCheckResult<DdType> hybridResult(
                        model.getReachableStates(), psiStates || (!statesWithProbabilityGreater0 && model.getReachableStates()),
                        psiStates.template toAdd<ValueType>(), statesWithProbabilityGreater0NonPsi, odd, subResult);

                    // Compute the set of relevant states.
                    storm::dd::Bdd<DdType> relevantStates = statesWithProbabilityGreater0 && phiStates;

                    // Filter the unbounded result such that it only contains values for the relevant states.
                    hybridResult.filter(SymbolicQualitativeCheckResult<DdType>(model.getReachableStates(), relevantStates));

                    // Build an ODD for the relevant states.
                    conversionWatch.start();
                    odd = relevantStates.createOdd();

                    std::unique_ptr<CheckResult> explicitResult = hybridResult.toExplicitQuantitativeCheckResult();
                    conversionWatch.stop();
                    std::vector<ValueType> newSubresult = std::move(explicitResult->asExplicitQuantitativeCheckResult<ValueType>().getValueVector());

                    // Then compute the transient probabilities of being in such a state after t time units. For this,
                    // we must re-uniformize the CTMC, so we need to compute the second uniformized matrix.
                    uniformizationRate = 1.02 * (relevantStates.template toAdd<ValueType>() * exitRateVector).getMax();
                    STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

                    // If the lower and upper bounds coincide, we have only determined the relevant states at this
                    // point, but we still need to construct the starting vector.
                    if (lowerBound == upperBound) {
                        odd = relevantStates.createOdd();
                        newSubresult = psiStates.template toAdd<ValueType>().toVector(odd);
                    }

                    // Finally, we compute the second set of transient probabilities.
                    uniformizedMatrix = computeUniformizedMatrix(model, rateMatrix, exitRateVector, relevantStates, uniformizationRate);
                    conversionWatch.start();
                    explicitUniformizedMatrix = uniformizedMatrix.toMatrix(odd, odd);
                    conversionWatch.stop();
                    STORM_LOG_INFO("Converting symbolic matrix/vector to explicit representation done in " << conversionWatch.getTimeInMilliseconds() << "ms.");

                    newSubresult = storm::modelchecker::helper::SparseCtmcCslHelper::computeTransientProbabilities<ValueType>(
                        env, explicitUniformizedMatrix, nullptr, lowerBound, uniformizationRate, newSubresult, epsilon);

                    return std::unique_ptr<CheckResult>(
                        new HybridQuantitativeCheckResult<DdType>(model.getReachableStates(), !relevantStates && model.getReachableStates(),
                                                                  model.getManager().template getAddZero<ValueType>(), relevantStates, odd, newSubresult));
                } else {
                    // In this case, the interval is of the form [t, t] with t != 0, t != inf.

                    storm::utility::Stopwatch conversionWatch;

                    // Build an ODD for the relevant states.
                    conversionWatch.start();
                    storm::dd::Odd odd = statesWithProbabilityGreater0.createOdd();

                    std::vector<ValueType> newSubresult = psiStates.template toAdd<ValueType>().toVector(odd);
                    conversionWatch.stop();

                    // Then compute the transient probabilities of being in such a state after t time units. For this,
                    // we must re-uniformize the CTMC, so we need to compute the second uniformized matrix.
                    ValueType uniformizationRate = 1.02 * (statesWithProbabilityGreater0.template toAdd<ValueType>() * exitRateVector).getMax();
                    STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

                    // Finally, we compute the second set of transient probabilities.
                    storm::dd::Add<DdType, ValueType> uniformizedMatrix =
                        computeUniformizedMatrix(model, rateMatrix, exitRateVector, statesWithProbabilityGreater0, uniformizationRate);
                    conversionWatch.start();
                    storm::storage::SparseMatrix<ValueType> explicitUniformizedMatrix = uniformizedMatrix.toMatrix(odd, odd);
                    conversionWatch.stop();
                    STORM_LOG_INFO("Converting symbolic matrix/vector to explicit representation done in " << conversionWatch.getTimeInMilliseconds() << "ms.");

                    newSubresult = storm::modelchecker::helper::SparseCtmcCslHelper::computeTransientProbabilities<ValueType>(
                        env, explicitUniformizedMatrix, nullptr, lowerBound, uniformizationRate, newSubresult, epsilon);

                    return std::unique_ptr<CheckResult>(new HybridQuantitativeCheckResult<DdType>(
                        model.getReachableStates(), !statesWithProbabilityGreater0 && model.getReachableStates(),
                        model.getManager().template getAddZero<ValueType>(), statesWithProbabilityGreater0, odd, newSubresult));
                }
            }
        }
    } else {
        return std::unique_ptr<CheckResult>(new SymbolicQuantitativeCheckResult<DdType>(model.getReachableStates(), psiStates.template toAdd<ValueType>()));
    }
}

template<storm::dd::DdType DdType, typename ValueType>
    requires storm::NumberTraits<ValueType>::SupportsExponential
std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeInstantaneousRewards(
    Environment const& env, storm::models::symbolic::Ctmc<DdType, ValueType> const& model, bool onlyInitialStatesRelevant,
    storm::dd::Add<DdType, ValueType> const& rateMatrix, storm::dd::Add<DdType, ValueType> const& exitRateVector,
    typename storm::models::symbolic::Model<DdType, ValueType>::RewardModelType const& rewardModel, ValueType timeBound) {
    // Only compute the result if the model has a state-based reward model.
    STORM_LOG_THROW(rewardModel.hasStateRewards(), storm::exceptions::InvalidPropertyException,
                    "Computing instantaneous rewards for a reward model that does not define any state-rewards. The result is trivially 0.");

    storm::utility::Stopwatch conversionWatch;

    // Create ODD for the translation.
    conversionWatch.start();
    storm::dd::Odd odd = model.getReachableStates().createOdd();
    conversionWatch.stop();

    // Initialize result to state rewards of the model.
    auto rewardsAdd = rewardModel.getStateRewardVector();
    std::vector<ValueType> result = rewardsAdd.toVector(odd);
    ValueType maxValue = std::max(rewardsAdd.getMax(), -rewardsAdd.getMin());

    // If the rewards are not zero and the time-bound is not zero, we need to perform a transient analysis.
    if (!storm::utility::isZero(maxValue) && timeBound > 0) {
        ValueType uniformizationRate = 1.02 * exitRateVector.getMax();
        STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

        storm::dd::Add<DdType, ValueType> uniformizedMatrix =
            computeUniformizedMatrix(model, rateMatrix, exitRateVector, model.getReachableStates(), uniformizationRate);

        conversionWatch.start();
        storm::storage::SparseMatrix<ValueType> explicitUniformizedMatrix = uniformizedMatrix.toMatrix(odd, odd);
        conversionWatch.stop();
        STORM_LOG_INFO("Converting symbolic matrix/vector to explicit representation done in " << conversionWatch.getTimeInMilliseconds() << "ms.");

        // Set the possible error allowed for truncation (epsilon for fox-glynn)
        ValueType epsilon = storm::utility::convertNumber<ValueType>(env.solver().timeBounded().getPrecision());
        if (env.solver().timeBounded().getRelativeTerminationCriterion()) {
            // Be more precise, if the maximum value is very small (This still gives no sound guarantee!)
            epsilon *= std::min(storm::utility::one<ValueType>(), maxValue);
        } else {
            // Be more precise, if the maximal possible value is very large
            epsilon /= std::max(storm::utility::one<ValueType>(), maxValue);
        }

        storm::storage::BitVector relevantValues;
        if (onlyInitialStatesRelevant) {
            relevantValues = model.getInitialStates().toVector(odd);
        } else {
            relevantValues = storm::storage::BitVector(result.size(), true);
        }

        // Loop until the desired precision is reached.
        do {
            result = storm::modelchecker::helper::SparseCtmcCslHelper::computeTransientProbabilities<ValueType>(env, explicitUniformizedMatrix, nullptr,
                                                                                                                timeBound, uniformizationRate, result, epsilon);
        } while (storm::modelchecker::helper::SparseCtmcCslHelper::checkAndUpdateTransientProbabilityEpsilon(env, epsilon, result, relevantValues));
    }

    return std::unique_ptr<CheckResult>(new HybridQuantitativeCheckResult<DdType, ValueType>(model.getReachableStates(), model.getManager().getBddZero(),
                                                                                             model.getManager().template getAddZero<ValueType>(),
                                                                                             model.getReachableStates(), odd, result));
}

template<storm::dd::DdType DdType, typename ValueType>
    requires storm::NumberTraits<ValueType>::SupportsExponential
std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeCumulativeRewards(
    Environment const& env, storm::models::symbolic::Ctmc<DdType, ValueType> const& model, bool onlyInitialStatesRelevant,
    storm::dd::Add<DdType, ValueType> const& rateMatrix, storm::dd::Add<DdType, ValueType> const& exitRateVector,
    typename storm::models::symbolic::Model<DdType, ValueType>::RewardModelType const& rewardModel, ValueType timeBound) {
    // Only compute the result if the model has a state-based reward model.
    STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");

    // If the time bound is zero, the result is the constant zero vector.
    if (timeBound == 0) {
        return std::unique_ptr<CheckResult>(
            new SymbolicQuantitativeCheckResult<DdType, ValueType>(model.getReachableStates(), model.getManager().template getAddZero<ValueType>()));
    }

    // Otherwise, we need to perform some computations.

    // Start with the uniformization.
    ValueType uniformizationRate = 1.02 * exitRateVector.getMax();
    STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

    storm::utility::Stopwatch conversionWatch;

    // Create ODD for the translation.
    conversionWatch.start();
    storm::dd::Odd odd = model.getReachableStates().createOdd();
    conversionWatch.stop();

    // Compute the uniformized matrix.
    storm::dd::Add<DdType, ValueType> uniformizedMatrix =
        computeUniformizedMatrix(model, rateMatrix, exitRateVector, model.getReachableStates(), uniformizationRate);
    conversionWatch.start();
    storm::storage::SparseMatrix<ValueType> explicitUniformizedMatrix = uniformizedMatrix.toMatrix(odd, odd);
    conversionWatch.stop();

    // Then compute the state reward vector to use in the computation.
    storm::dd::Add<DdType, ValueType> totalRewardVector = rewardModel.getTotalRewardVector(rateMatrix, model.getColumnVariables(), exitRateVector, false);
    conversionWatch.start();
    std::vector<ValueType> explicitTotalRewardVector = totalRewardVector.toVector(odd);
    conversionWatch.stop();
    STORM_LOG_INFO("Converting symbolic matrix/vector to explicit representation done in " << conversionWatch.getTimeInMilliseconds() << "ms.");

    ValueType maxReward = std::max(totalRewardVector.getMax(), -totalRewardVector.getMin());

    // If all rewards are zero, the result is the constant zero vector.
    if (storm::utility::isZero(maxReward)) {
        return std::unique_ptr<CheckResult>(
            new SymbolicQuantitativeCheckResult<DdType, ValueType>(model.getReachableStates(), model.getManager().template getAddZero<ValueType>()));
    }

    // Set the possible (absolute) error allowed for truncation (epsilon for fox-glynn)
    ValueType epsilon = storm::utility::convertNumber<ValueType>(env.solver().timeBounded().getPrecision());
    if (env.solver().timeBounded().getRelativeTerminationCriterion()) {
        // Be more precise, if the value is very small (this still gives no sound guarantee)
        epsilon *= std::min(storm::utility::one<ValueType>(), maxReward);
    } else {
        // Be more precise, if the maximal possible value is very large
        epsilon /= std::max(storm::utility::one<ValueType>(), maxReward * timeBound);
    }

    storm::storage::BitVector relevantValues;
    if (onlyInitialStatesRelevant) {
        relevantValues = model.getInitialStates().toVector(odd);
    } else {
        relevantValues = storm::storage::BitVector(explicitTotalRewardVector.size(), true);
    }

    std::vector<ValueType> result;
    // Finally, compute the transient probabilities.
    // Loop until the desired precision is reached.
    do {
        result = storm::modelchecker::helper::SparseCtmcCslHelper::computeTransientProbabilities<ValueType, true>(
            env, explicitUniformizedMatrix, nullptr, timeBound, uniformizationRate, explicitTotalRewardVector, epsilon);
    } while (storm::modelchecker::helper::SparseCtmcCslHelper::checkAndUpdateTransientProbabilityEpsilon(env, epsilon, result, relevantValues));

    return std::unique_ptr<CheckResult>(new HybridQuantitativeCheckResult<DdType, ValueType>(model.getReachableStates(), model.getManager().getBddZero(),
                                                                                             model.getManager().template getAddZero<ValueType>(),
                                                                                             model.getReachableStates(), std::move(odd), std::move(result)));
}

template<storm::dd::DdType DdType, class ValueType>
storm::dd::Add<DdType, ValueType> HybridCtmcCslHelper::computeUniformizedMatrix(storm::models::symbolic::Ctmc<DdType, ValueType> const& model,
                                                                                storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                                storm::dd::Add<DdType, ValueType> const& exitRateVector,
                                                                                storm::dd::Bdd<DdType> const& maybeStates, ValueType uniformizationRate) {
    STORM_LOG_DEBUG("Computing uniformized matrix using uniformization rate " << uniformizationRate << ".");
    STORM_LOG_DEBUG("Keeping " << maybeStates.getNonZeroCount() << " rows.");

    // Cut all non-maybe rows/columns from the transition matrix.
    storm::dd::Add<DdType, ValueType> uniformizedMatrix = transitionMatrix * maybeStates.template toAdd<ValueType>() *
                                                          maybeStates.swapVariables(model.getRowColumnMetaVariablePairs()).template toAdd<ValueType>();

    // Now perform the uniformization.
    uniformizedMatrix = uniformizedMatrix / model.getManager().getConstant(uniformizationRate);
    storm::dd::Add<DdType, ValueType> diagonal = model.getRowColumnIdentity() * maybeStates.template toAdd<ValueType>();
    storm::dd::Add<DdType, ValueType> diagonalOffset = diagonal;
    diagonalOffset -= diagonal * (exitRateVector / model.getManager().getConstant(uniformizationRate));
    uniformizedMatrix += diagonalOffset;

    return uniformizedMatrix;
}

template<storm::dd::DdType DdType, class ValueType>
storm::dd::Add<DdType, ValueType> HybridCtmcCslHelper::computeProbabilityMatrix(storm::dd::Add<DdType, ValueType> const& rateMatrix,
                                                                                storm::dd::Add<DdType, ValueType> const& exitRateVector) {
    return rateMatrix / exitRateVector;
}

// Explicit instantiations.

// Cudd, double.
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeBoundedUntilProbabilities(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::CUDD, double> const& model, bool onlyInitialStatesRelevant,
    storm::dd::Add<storm::dd::DdType::CUDD, double> const& rateMatrix, storm::dd::Add<storm::dd::DdType::CUDD, double> const& exitRateVector,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates, storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates, bool qualitative, double lowerBound,
    double upperBound);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeInstantaneousRewards(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::CUDD, double> const& model, bool onlyInitialStatesRelevant,
    storm::dd::Add<storm::dd::DdType::CUDD, double> const& rateMatrix, storm::dd::Add<storm::dd::DdType::CUDD, double> const& exitRateVector,
    typename storm::models::symbolic::Model<storm::dd::DdType::CUDD, double>::RewardModelType const& rewardModel, double timeBound);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeCumulativeRewards(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::CUDD, double> const& model, bool onlyInitialStatesRelevant,
    storm::dd::Add<storm::dd::DdType::CUDD, double> const& rateMatrix, storm::dd::Add<storm::dd::DdType::CUDD, double> const& exitRateVector,
    typename storm::models::symbolic::Model<storm::dd::DdType::CUDD, double>::RewardModelType const& rewardModel, double timeBound);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeUntilProbabilities(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::CUDD, double> const& model,
    storm::dd::Add<storm::dd::DdType::CUDD, double> const& rateMatrix, storm::dd::Add<storm::dd::DdType::CUDD, double> const& exitRateVector,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& phiStates, storm::dd::Bdd<storm::dd::DdType::CUDD> const& psiStates, bool qualitative);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeReachabilityRewards(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::CUDD, double> const& model,
    storm::dd::Add<storm::dd::DdType::CUDD, double> const& rateMatrix, storm::dd::Add<storm::dd::DdType::CUDD, double> const& exitRateVector,
    typename storm::models::symbolic::Model<storm::dd::DdType::CUDD, double>::RewardModelType const& rewardModel,
    storm::dd::Bdd<storm::dd::DdType::CUDD> const& targetStates, bool qualitative);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeNextProbabilities(Environment const& env,
                                                                                    storm::models::symbolic::Ctmc<storm::dd::DdType::CUDD, double> const& model,
                                                                                    storm::dd::Add<storm::dd::DdType::CUDD, double> const& rateMatrix,
                                                                                    storm::dd::Add<storm::dd::DdType::CUDD, double> const& exitRateVector,
                                                                                    storm::dd::Bdd<storm::dd::DdType::CUDD> const& nextStates);
template storm::dd::Add<storm::dd::DdType::CUDD, double> HybridCtmcCslHelper::computeProbabilityMatrix(
    storm::dd::Add<storm::dd::DdType::CUDD, double> const& rateMatrix, storm::dd::Add<storm::dd::DdType::CUDD, double> const& exitRateVector);
template storm::dd::Add<storm::dd::DdType::CUDD, double> HybridCtmcCslHelper::computeUniformizedMatrix(
    storm::models::symbolic::Ctmc<storm::dd::DdType::CUDD, double> const& model, storm::dd::Add<storm::dd::DdType::CUDD, double> const& transitionMatrix,
    storm::dd::Add<storm::dd::DdType::CUDD, double> const& exitRateVector, storm::dd::Bdd<storm::dd::DdType::CUDD> const& maybeStates,
    double uniformizationRate);

// Sylvan, double.
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeBoundedUntilProbabilities(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, double> const& model, bool onlyInitialStatesRelevant,
    storm::dd::Add<storm::dd::DdType::Sylvan, double> const& rateMatrix, storm::dd::Add<storm::dd::DdType::Sylvan, double> const& exitRateVector,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, bool qualitative, double lowerBound,
    double upperBound);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeInstantaneousRewards(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, double> const& model, bool onlyInitialStatesRelevant,
    storm::dd::Add<storm::dd::DdType::Sylvan, double> const& rateMatrix, storm::dd::Add<storm::dd::DdType::Sylvan, double> const& exitRateVector,
    typename storm::models::symbolic::Model<storm::dd::DdType::Sylvan, double>::RewardModelType const& rewardModel, double timeBound);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeCumulativeRewards(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, double> const& model, bool onlyInitialStatesRelevant,
    storm::dd::Add<storm::dd::DdType::Sylvan, double> const& rateMatrix, storm::dd::Add<storm::dd::DdType::Sylvan, double> const& exitRateVector,
    typename storm::models::symbolic::Model<storm::dd::DdType::Sylvan, double>::RewardModelType const& rewardModel, double timeBound);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeUntilProbabilities(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, double> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, double> const& rateMatrix, storm::dd::Add<storm::dd::DdType::Sylvan, double> const& exitRateVector,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, bool qualitative);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeReachabilityRewards(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, double> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, double> const& rateMatrix, storm::dd::Add<storm::dd::DdType::Sylvan, double> const& exitRateVector,
    typename storm::models::symbolic::Model<storm::dd::DdType::Sylvan, double>::RewardModelType const& rewardModel,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& targetStates, bool qualitative);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeNextProbabilities(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, double> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, double> const& rateMatrix, storm::dd::Add<storm::dd::DdType::Sylvan, double> const& exitRateVector,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nextStates);
template storm::dd::Add<storm::dd::DdType::Sylvan, double> HybridCtmcCslHelper::computeProbabilityMatrix(
    storm::dd::Add<storm::dd::DdType::Sylvan, double> const& rateMatrix, storm::dd::Add<storm::dd::DdType::Sylvan, double> const& exitRateVector);
template storm::dd::Add<storm::dd::DdType::Sylvan, double> HybridCtmcCslHelper::computeUniformizedMatrix(
    storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, double> const& model, storm::dd::Add<storm::dd::DdType::Sylvan, double> const& transitionMatrix,
    storm::dd::Add<storm::dd::DdType::Sylvan, double> const& exitRateVector, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& maybeStates,
    double uniformizationRate);

// Sylvan, rational number.
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeUntilProbabilities(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& rateMatrix,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& exitRateVector, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, bool qualitative);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeReachabilityRewards(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& rateMatrix,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& exitRateVector,
    typename storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalNumber>::RewardModelType const& rewardModel,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& targetStates, bool qualitative);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeNextProbabilities(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& rateMatrix,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& exitRateVector, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nextStates);
template storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> HybridCtmcCslHelper::computeProbabilityMatrix(
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& rateMatrix,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& exitRateVector);
template storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> HybridCtmcCslHelper::computeUniformizedMatrix(
    storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalNumber> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& transitionMatrix,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalNumber> const& exitRateVector, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& maybeStates,
    storm::RationalNumber uniformizationRate);

// Sylvan, rational function.
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeUntilProbabilities(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalFunction> const& rateMatrix,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalFunction> const& exitRateVector, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& phiStates,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& psiStates, bool qualitative);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeReachabilityRewards(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalFunction> const& rateMatrix,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalFunction> const& exitRateVector,
    typename storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalFunction>::RewardModelType const& rewardModel,
    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& targetStates, bool qualitative);
template std::unique_ptr<CheckResult> HybridCtmcCslHelper::computeNextProbabilities(
    Environment const& env, storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalFunction> const& rateMatrix,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalFunction> const& exitRateVector, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& nextStates);
template storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalFunction> HybridCtmcCslHelper::computeProbabilityMatrix(
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalFunction> const& rateMatrix,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalFunction> const& exitRateVector);
template storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalFunction> HybridCtmcCslHelper::computeUniformizedMatrix(
    storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalFunction> const& model,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalFunction> const& transitionMatrix,
    storm::dd::Add<storm::dd::DdType::Sylvan, storm::RationalFunction> const& exitRateVector, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& maybeStates,
    storm::RationalFunction uniformizationRate);

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
