#include "storm/modelchecker/csl/helper/SparseCtmcCslHelper.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/environment/solver/TimeBoundedSolverEnvironment.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/modelchecker/prctl/helper/SparseDtmcPrctlHelper.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/solver/multiplier/Multiplier.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/numerical.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType>
bool SparseCtmcCslHelper::checkAndUpdateTransientProbabilityEpsilon(storm::Environment const& env, ValueType& epsilon,
                                                                    std::vector<ValueType> const& resultVector,
                                                                    storm::storage::BitVector const& relevantPositions) {
    // Check if the check is necessary for the provided environment
    if (!env.solver().isForceSoundness() || !env.solver().timeBounded().getRelativeTerminationCriterion()) {
        // No need to update epsilon
        return false;
    }

    ValueType precision = storm::utility::convertNumber<ValueType>(env.solver().timeBounded().getPrecision());
    // If we need to compute values with relative precision, it might be necessary to increase the precision requirements (epsilon)
    ValueType newEpsilon = epsilon;
    // Only consider positions that are relevant for the solve goal (e.g. initial states of the model) and are supposed to have a non-zero value
    for (auto state : relevantPositions) {
        if (storm::utility::isZero(resultVector[state])) {
            newEpsilon = std::min(epsilon * storm::utility::convertNumber<ValueType>(0.1), newEpsilon);
        } else {
            ValueType relativeError = epsilon / resultVector[state];  // epsilon is an upper bound for the absolute error we made
            if (relativeError > precision) {
                newEpsilon = std::min(resultVector[state] * precision, newEpsilon);
            }
        }
    }
    if (newEpsilon < epsilon) {
        STORM_LOG_INFO("Re-computing transient probabilities with new truncation error " << newEpsilon
                                                                                         << " to guarantee sound results with relative precision.");
        epsilon = newEpsilon;
        return true;
    } else {
        return false;
    }
}

template<typename ValueType>
    requires storm::NumberTraits<ValueType>::SupportsExponential
std::vector<ValueType> SparseCtmcCslHelper::computeBoundedUntilProbabilities(
    Environment const& env, storm::solver::SolveGoal<ValueType>&& goal, storm::storage::SparseMatrix<ValueType> const& rateMatrix,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
    std::vector<ValueType> const& exitRates, bool qualitative, ValueType lowerBound, ValueType upperBound) {
    STORM_LOG_THROW(!env.solver().isForceExact(), storm::exceptions::InvalidOperationException,
                    "Exact computations not possible for bounded until probabilities.");

    uint_fast64_t numberOfStates = rateMatrix.getRowCount();

    // If the time bounds are [0, inf], we rather call untimed reachability.
    if (storm::utility::isZero(lowerBound) && upperBound == storm::utility::infinity<ValueType>()) {
        return computeUntilProbabilities(env, std::move(goal), rateMatrix, backwardTransitions, exitRates, phiStates, psiStates, qualitative);
    }

    // From this point on, we know that we have to solve a more complicated problem [t, t'] with either t != 0
    // or t' != inf.

    // Create the result vector.
    std::vector<ValueType> result;

    // Set the possible (absolute) error allowed for truncation (epsilon for fox-glynn)
    ValueType epsilon = storm::utility::convertNumber<ValueType>(env.solver().timeBounded().getPrecision()) / 8.0;

    // If we identify the states that have probability 0 of reaching the target states, we can exclude them from the
    // further computations.
    storm::storage::BitVector statesWithProbabilityGreater0 = storm::utility::graph::performProbGreater0(backwardTransitions, phiStates, psiStates);
    STORM_LOG_INFO("Found " << statesWithProbabilityGreater0.getNumberOfSetBits() << " states with probability greater 0.");
    storm::storage::BitVector statesWithProbabilityGreater0NonPsi = statesWithProbabilityGreater0 & ~psiStates;
    STORM_LOG_INFO("Found " << statesWithProbabilityGreater0NonPsi.getNumberOfSetBits() << " 'maybe' states.");

    // the positions within the result for which the precision needs to be checked
    storm::storage::BitVector relevantValues;
    if (goal.hasRelevantValues()) {
        relevantValues = std::move(goal.relevantValues());
        relevantValues &= statesWithProbabilityGreater0;
    } else {
        relevantValues = statesWithProbabilityGreater0;
    }

    do {  // Iterate until the desired precision is reached (only relevant for relative precision criterion)
        if (!statesWithProbabilityGreater0.empty()) {
            if (storm::utility::isZero(upperBound)) {
                // In this case, the interval is of the form [0, 0].
                result = std::vector<ValueType>(numberOfStates, storm::utility::zero<ValueType>());
                storm::utility::vector::setVectorValues<ValueType>(result, psiStates, storm::utility::one<ValueType>());
            } else {
                if (storm::utility::isZero(lowerBound)) {
                    // In this case, the interval is of the form [0, t].
                    // Note that this excludes [0, inf] since this is untimed reachability and we considered this case earlier.

                    result = std::vector<ValueType>(numberOfStates, storm::utility::zero<ValueType>());
                    storm::utility::vector::setVectorValues<ValueType>(result, psiStates, storm::utility::one<ValueType>());
                    if (!statesWithProbabilityGreater0NonPsi.empty()) {
                        // Find the maximal rate of all 'maybe' states to take it as the uniformization rate.
                        ValueType uniformizationRate = 0;
                        for (auto state : statesWithProbabilityGreater0NonPsi) {
                            uniformizationRate = std::max(uniformizationRate, exitRates[state]);
                        }
                        uniformizationRate *= 1.02;
                        STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

                        // Compute the uniformized matrix.
                        storm::storage::SparseMatrix<ValueType> uniformizedMatrix =
                            computeUniformizedMatrix(rateMatrix, statesWithProbabilityGreater0NonPsi, uniformizationRate, exitRates);

                        // Compute the vector that is to be added as a compensation for removing the absorbing states.
                        std::vector<ValueType> b = rateMatrix.getConstrainedRowSumVector(statesWithProbabilityGreater0NonPsi, psiStates);
                        for (auto& element : b) {
                            element /= uniformizationRate;
                        }

                        // Finally compute the transient probabilities.
                        std::vector<ValueType> values(statesWithProbabilityGreater0NonPsi.getNumberOfSetBits(), storm::utility::zero<ValueType>());
                        std::vector<ValueType> subresult =
                            computeTransientProbabilities(env, uniformizedMatrix, &b, upperBound, uniformizationRate, values, epsilon);
                        storm::utility::vector::setVectorValues(result, statesWithProbabilityGreater0NonPsi, subresult);
                    }
                } else if (upperBound == storm::utility::infinity<ValueType>()) {
                    // In this case, the interval is of the form [t, inf] with t != 0.

                    // Start by computing the (unbounded) reachability probabilities of reaching psi states while
                    // staying in phi states.
                    result = computeUntilProbabilities(env, storm::solver::SolveGoal<ValueType>(), rateMatrix, backwardTransitions, exitRates, phiStates,
                                                       psiStates, qualitative);

                    // Determine the set of states that must be considered further.
                    storm::storage::BitVector relevantStates = statesWithProbabilityGreater0 & phiStates;
                    std::vector<ValueType> subResult(relevantStates.getNumberOfSetBits());
                    storm::utility::vector::selectVectorValues(subResult, relevantStates, result);

                    ValueType uniformizationRate = 0;
                    for (auto state : relevantStates) {
                        uniformizationRate = std::max(uniformizationRate, exitRates[state]);
                    }
                    uniformizationRate *= 1.02;
                    STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

                    // Compute the uniformized matrix.
                    storm::storage::SparseMatrix<ValueType> uniformizedMatrix =
                        computeUniformizedMatrix(rateMatrix, relevantStates, uniformizationRate, exitRates);

                    // Compute the transient probabilities.
                    subResult = computeTransientProbabilities<ValueType>(env, uniformizedMatrix, nullptr, lowerBound, uniformizationRate, subResult, epsilon);

                    // Fill in the correct values.
                    storm::utility::vector::setVectorValues(result, ~relevantStates, storm::utility::zero<ValueType>());
                    storm::utility::vector::setVectorValues(result, relevantStates, subResult);
                } else {
                    // In this case, the interval is of the form [t, t'] with t != 0 and t' != inf.

                    if (lowerBound != upperBound) {
                        // In this case, the interval is of the form [t, t'] with t != 0, t' != inf and t != t'.

                        storm::storage::BitVector relevantStates = statesWithProbabilityGreater0 & phiStates;
                        std::vector<ValueType> newSubresult(relevantStates.getNumberOfSetBits(), storm::utility::zero<ValueType>());
                        storm::utility::vector::setVectorValues(newSubresult, psiStates % relevantStates, storm::utility::one<ValueType>());
                        if (!statesWithProbabilityGreater0NonPsi.empty()) {
                            // Find the maximal rate of all 'maybe' states to take it as the uniformization rate.
                            ValueType uniformizationRate = storm::utility::zero<ValueType>();
                            for (auto state : statesWithProbabilityGreater0NonPsi) {
                                uniformizationRate = std::max(uniformizationRate, exitRates[state]);
                            }
                            uniformizationRate *= 1.02;
                            STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

                            // Compute the (first) uniformized matrix.
                            storm::storage::SparseMatrix<ValueType> uniformizedMatrix =
                                computeUniformizedMatrix(rateMatrix, statesWithProbabilityGreater0NonPsi, uniformizationRate, exitRates);

                            // Compute the vector that is to be added as a compensation for removing the absorbing states.
                            std::vector<ValueType> b = rateMatrix.getConstrainedRowSumVector(statesWithProbabilityGreater0NonPsi, psiStates);
                            for (auto& element : b) {
                                element /= uniformizationRate;
                            }

                            // Start by computing the transient probabilities of reaching a psi state in time t' - t.
                            std::vector<ValueType> values(statesWithProbabilityGreater0NonPsi.getNumberOfSetBits(), storm::utility::zero<ValueType>());
                            // divide the possible error by two since we will make this error two times.
                            std::vector<ValueType> subresult =
                                computeTransientProbabilities(env, uniformizedMatrix, &b, upperBound - lowerBound, uniformizationRate, values,
                                                              epsilon / storm::utility::convertNumber<ValueType>(2.0));
                            storm::utility::vector::setVectorValues(newSubresult, statesWithProbabilityGreater0NonPsi % relevantStates, subresult);
                        }

                        // Then compute the transient probabilities of being in such a state after t time units. For this,
                        // we must re-uniformize the CTMC, so we need to compute the second uniformized matrix.
                        ValueType uniformizationRate = storm::utility::zero<ValueType>();
                        for (auto state : relevantStates) {
                            uniformizationRate = std::max(uniformizationRate, exitRates[state]);
                        }
                        uniformizationRate *= 1.02;
                        STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

                        // Finally, we compute the second set of transient probabilities.
                        storm::storage::SparseMatrix<ValueType> uniformizedMatrix =
                            computeUniformizedMatrix(rateMatrix, relevantStates, uniformizationRate, exitRates);
                        newSubresult = computeTransientProbabilities<ValueType>(env, uniformizedMatrix, nullptr, lowerBound, uniformizationRate, newSubresult,
                                                                                epsilon / storm::utility::convertNumber<ValueType>(2.0));

                        // Fill in the correct values.
                        result = std::vector<ValueType>(numberOfStates, storm::utility::zero<ValueType>());
                        storm::utility::vector::setVectorValues(result, ~relevantStates, storm::utility::zero<ValueType>());
                        storm::utility::vector::setVectorValues(result, relevantStates, newSubresult);
                    } else {
                        // In this case, the interval is of the form [t, t] with t != 0, t != inf.

                        std::vector<ValueType> newSubresult = std::vector<ValueType>(statesWithProbabilityGreater0.getNumberOfSetBits());
                        storm::utility::vector::setVectorValues(newSubresult, psiStates % statesWithProbabilityGreater0, storm::utility::one<ValueType>());

                        // Then compute the transient probabilities of being in such a state after t time units. For this,
                        // we must re-uniformize the CTMC, so we need to compute the second uniformized matrix.
                        ValueType uniformizationRate = storm::utility::zero<ValueType>();
                        for (auto state : statesWithProbabilityGreater0) {
                            uniformizationRate = std::max(uniformizationRate, exitRates[state]);
                        }
                        uniformizationRate *= 1.02;
                        STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

                        // Finally, we compute the second set of transient probabilities.
                        storm::storage::SparseMatrix<ValueType> uniformizedMatrix =
                            computeUniformizedMatrix(rateMatrix, statesWithProbabilityGreater0, uniformizationRate, exitRates);
                        newSubresult =
                            computeTransientProbabilities<ValueType>(env, uniformizedMatrix, nullptr, lowerBound, uniformizationRate, newSubresult, epsilon);

                        // Fill in the correct values.
                        result = std::vector<ValueType>(numberOfStates, storm::utility::zero<ValueType>());
                        storm::utility::vector::setVectorValues(result, ~statesWithProbabilityGreater0, storm::utility::zero<ValueType>());
                        storm::utility::vector::setVectorValues(result, statesWithProbabilityGreater0, newSubresult);
                    }
                }
            }
        } else {
            result = std::vector<ValueType>(numberOfStates, storm::utility::zero<ValueType>());
        }
    } while (checkAndUpdateTransientProbabilityEpsilon(env, epsilon, result, relevantValues));
    return result;
}

template<typename ValueType>
std::vector<ValueType> SparseCtmcCslHelper::computeUntilProbabilities(Environment const& env, storm::solver::SolveGoal<ValueType>&& goal,
                                                                      storm::storage::SparseMatrix<ValueType> const& rateMatrix,
                                                                      storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                      std::vector<ValueType> const& exitRateVector, storm::storage::BitVector const& phiStates,
                                                                      storm::storage::BitVector const& psiStates, bool qualitative) {
    return SparseDtmcPrctlHelper<ValueType>::computeUntilProbabilities(env, std::move(goal), computeProbabilityMatrix(rateMatrix, exitRateVector),
                                                                       backwardTransitions, phiStates, psiStates, qualitative);
}

template<typename ValueType>
std::vector<ValueType> SparseCtmcCslHelper::computeAllUntilProbabilities(Environment const& env, storm::solver::SolveGoal<ValueType>&& goal,
                                                                         storm::storage::SparseMatrix<ValueType> const& rateMatrix,
                                                                         std::vector<ValueType> const& exitRateVector,
                                                                         storm::storage::BitVector const& initialStates,
                                                                         storm::storage::BitVector const& phiStates,
                                                                         storm::storage::BitVector const& psiStates) {
    return SparseDtmcPrctlHelper<ValueType>::computeAllUntilProbabilities(env, std::move(goal), computeProbabilityMatrix(rateMatrix, exitRateVector),
                                                                          initialStates, phiStates, psiStates);
}

template<typename ValueType>
std::vector<ValueType> SparseCtmcCslHelper::computeNextProbabilities(Environment const& env, storm::storage::SparseMatrix<ValueType> const& rateMatrix,
                                                                     std::vector<ValueType> const& exitRateVector,
                                                                     storm::storage::BitVector const& nextStates) {
    return SparseDtmcPrctlHelper<ValueType>::computeNextProbabilities(env, computeProbabilityMatrix(rateMatrix, exitRateVector), nextStates);
}

template<typename ValueType, typename RewardModelType>
    requires storm::NumberTraits<ValueType>::SupportsExponential
std::vector<ValueType> SparseCtmcCslHelper::computeInstantaneousRewards(Environment const& env, storm::solver::SolveGoal<ValueType>&& goal,
                                                                        storm::storage::SparseMatrix<ValueType> const& rateMatrix,
                                                                        std::vector<ValueType> const& exitRateVector, RewardModelType const& rewardModel,
                                                                        ValueType timeBound) {
    // Only compute the result if the model has a state-based reward model.
    STORM_LOG_THROW(rewardModel.hasStateRewards(), storm::exceptions::InvalidPropertyException,
                    "Computing instantaneous rewards for a reward model that does not define any state-rewards. The result is trivially 0.");

    uint_fast64_t numberOfStates = rateMatrix.getRowCount();

    // Initialize result to state rewards of the this->getModel().
    std::vector<ValueType> result(rewardModel.getStateRewardVector());

    // If the time-bound is zero, just return the current reward vector
    if (storm::utility::isZero(timeBound)) {
        return result;
    }
    ValueType maxValue = storm::utility::vector::maximumElementAbs(result);

    // If all entries are zero, we return the zero-vector
    if (storm::utility::isZero(maxValue)) {
        return result;
    }

    ValueType uniformizationRate = 0;
    for (auto const& rate : exitRateVector) {
        uniformizationRate = std::max(uniformizationRate, rate);
    }
    uniformizationRate *= 1.02;
    STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

    storm::storage::SparseMatrix<ValueType> uniformizedMatrix =
        computeUniformizedMatrix(rateMatrix, storm::storage::BitVector(numberOfStates, true), uniformizationRate, exitRateVector);

    // Set the possible error allowed for truncation (epsilon for fox-glynn)
    ValueType epsilon = storm::utility::convertNumber<ValueType>(env.solver().timeBounded().getPrecision());
    if (env.solver().timeBounded().getRelativeTerminationCriterion()) {
        // Be more precise, if the maximum value is very small (precision can/has to be refined later)
        epsilon *= std::min(storm::utility::one<ValueType>(), maxValue);
    } else {
        // Be more precise, if the maximal possible value is very large
        epsilon /= std::max(storm::utility::one<ValueType>(), maxValue);
    }

    storm::storage::BitVector relevantValues;
    if (goal.hasRelevantValues()) {
        relevantValues = std::move(goal.relevantValues());
    } else {
        relevantValues = storm::storage::BitVector(result.size(), true);
    }

    // Loop until the desired precision is reached.
    do {
        result = computeTransientProbabilities<ValueType>(env, uniformizedMatrix, nullptr, timeBound, uniformizationRate, result, epsilon);
    } while (checkAndUpdateTransientProbabilityEpsilon(env, epsilon, result, relevantValues));

    return result;
}

template<typename ValueType, typename RewardModelType>
    requires storm::NumberTraits<ValueType>::SupportsExponential
std::vector<ValueType> SparseCtmcCslHelper::computeCumulativeRewards(Environment const& env, storm::solver::SolveGoal<ValueType>&& goal,
                                                                     storm::storage::SparseMatrix<ValueType> const& rateMatrix,
                                                                     std::vector<ValueType> const& exitRateVector, RewardModelType const& rewardModel,
                                                                     ValueType timeBound) {
    STORM_LOG_THROW(!env.solver().isForceExact(), storm::exceptions::InvalidOperationException,
                    "Exact computations not possible for cumulative expected rewards.");

    // Only compute the result if the model has a state-based reward model.
    STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");

    uint_fast64_t numberOfStates = rateMatrix.getRowCount();

    // If the time bound is zero, the result is the constant zero vector.
    if (timeBound == 0) {
        return std::vector<ValueType>(numberOfStates, storm::utility::zero<ValueType>());
    }

    // Otherwise, we need to perform some computations.

    // Start with the uniformization.
    ValueType uniformizationRate = 0;
    for (auto const& rate : exitRateVector) {
        uniformizationRate = std::max(uniformizationRate, rate);
    }
    uniformizationRate *= 1.02;
    STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

    storm::storage::SparseMatrix<ValueType> uniformizedMatrix =
        computeUniformizedMatrix(rateMatrix, storm::storage::BitVector(numberOfStates, true), uniformizationRate, exitRateVector);

    // Compute the total state reward vector.
    std::vector<ValueType> totalRewardVector = rewardModel.getTotalRewardVector(rateMatrix, exitRateVector);
    ValueType maxReward = storm::utility::vector::maximumElementAbs(totalRewardVector);

    // If all rewards are zero, the result is the constant zero vector.
    if (storm::utility::isZero(maxReward)) {
        return std::vector<ValueType>(numberOfStates, storm::utility::zero<ValueType>());
    }

    // Set the possible (absolute) error allowed for truncation (epsilon for fox-glynn)
    ValueType epsilon = storm::utility::convertNumber<ValueType>(env.solver().timeBounded().getPrecision());
    if (env.solver().timeBounded().getRelativeTerminationCriterion()) {
        // Be more precise, if the value is very small (precision can/has to be refined later)
        epsilon *= std::min(storm::utility::one<ValueType>(), maxReward);
    } else {
        // Be more precise, if the maximal possible value is very large
        epsilon /= std::max(storm::utility::one<ValueType>(), maxReward * timeBound);
    }

    storm::storage::BitVector relevantValues;
    if (goal.hasRelevantValues()) {
        relevantValues = std::move(goal.relevantValues());
    } else {
        relevantValues = storm::storage::BitVector(totalRewardVector.size(), true);
    }

    // Loop until the desired precision is reached.
    std::vector<ValueType> result;
    do {
        result = computeTransientProbabilities<ValueType, true>(env, uniformizedMatrix, nullptr, timeBound, uniformizationRate, totalRewardVector, epsilon);
    } while (checkAndUpdateTransientProbabilityEpsilon(env, epsilon, result, relevantValues));

    return result;
}

template<typename ValueType>
std::vector<ValueType> SparseCtmcCslHelper::computeReachabilityTimes(Environment const& env, storm::solver::SolveGoal<ValueType>&& goal,
                                                                     storm::storage::SparseMatrix<ValueType> const& rateMatrix,
                                                                     storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                     std::vector<ValueType> const& exitRateVector,
                                                                     storm::storage::BitVector const& targetStates, bool qualitative) {
    // Compute expected time on CTMC by reduction to DTMC with rewards.
    storm::storage::SparseMatrix<ValueType> probabilityMatrix = computeProbabilityMatrix(rateMatrix, exitRateVector);

    // Initialize rewards.
    std::vector<ValueType> totalRewardVector;
    for (size_t i = 0; i < exitRateVector.size(); ++i) {
        if (targetStates[i] || storm::utility::isZero(exitRateVector[i])) {
            // Set reward for target states or states without outgoing transitions to 0.
            totalRewardVector.push_back(storm::utility::zero<ValueType>());
        } else {
            // Reward is (1 / exitRate).
            totalRewardVector.push_back(storm::utility::one<ValueType>() / exitRateVector[i]);
        }
    }

    return storm::modelchecker::helper::SparseDtmcPrctlHelper<ValueType>::computeReachabilityRewards(
        env, std::move(goal), probabilityMatrix, backwardTransitions, totalRewardVector, targetStates, qualitative);
}

template<typename ValueType, typename RewardModelType>
std::vector<ValueType> SparseCtmcCslHelper::computeReachabilityRewards(Environment const& env, storm::solver::SolveGoal<ValueType>&& goal,
                                                                       storm::storage::SparseMatrix<ValueType> const& rateMatrix,
                                                                       storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                       std::vector<ValueType> const& exitRateVector, RewardModelType const& rewardModel,
                                                                       storm::storage::BitVector const& targetStates, bool qualitative) {
    STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");

    storm::storage::SparseMatrix<ValueType> probabilityMatrix = computeProbabilityMatrix(rateMatrix, exitRateVector);

    std::vector<ValueType> totalRewardVector;
    if (rewardModel.hasStateRewards()) {
        totalRewardVector = rewardModel.getStateRewardVector();
        typename std::vector<ValueType>::const_iterator it2 = exitRateVector.begin();
        for (typename std::vector<ValueType>::iterator it1 = totalRewardVector.begin(), ite1 = totalRewardVector.end(); it1 != ite1; ++it1, ++it2) {
            *it1 /= *it2;
        }
        if (rewardModel.hasStateActionRewards()) {
            storm::utility::vector::addVectors(totalRewardVector, rewardModel.getStateActionRewardVector(), totalRewardVector);
        }
        if (rewardModel.hasTransitionRewards()) {
            storm::utility::vector::addVectors(totalRewardVector, probabilityMatrix.getPointwiseProductRowSumVector(rewardModel.getTransitionRewardMatrix()),
                                               totalRewardVector);
        }
    } else if (rewardModel.hasTransitionRewards()) {
        totalRewardVector = probabilityMatrix.getPointwiseProductRowSumVector(rewardModel.getTransitionRewardMatrix());
        if (rewardModel.hasStateActionRewards()) {
            storm::utility::vector::addVectors(totalRewardVector, rewardModel.getStateActionRewardVector(), totalRewardVector);
        }
    } else {
        totalRewardVector = rewardModel.getStateActionRewardVector();
    }

    return storm::modelchecker::helper::SparseDtmcPrctlHelper<ValueType>::computeReachabilityRewards(
        env, std::move(goal), probabilityMatrix, backwardTransitions, totalRewardVector, targetStates, qualitative);
}

template<typename ValueType, typename RewardModelType>
std::vector<ValueType> SparseCtmcCslHelper::computeTotalRewards(Environment const& env, storm::solver::SolveGoal<ValueType>&& goal,
                                                                storm::storage::SparseMatrix<ValueType> const& rateMatrix,
                                                                storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                std::vector<ValueType> const& exitRateVector, RewardModelType const& rewardModel,
                                                                bool qualitative) {
    STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::InvalidPropertyException, "Missing reward model for formula. Skipping formula.");

    storm::storage::SparseMatrix<ValueType> probabilityMatrix = computeProbabilityMatrix(rateMatrix, exitRateVector);

    std::vector<ValueType> totalRewardVector;
    if (rewardModel.hasStateRewards()) {
        totalRewardVector = rewardModel.getStateRewardVector();
        typename std::vector<ValueType>::const_iterator it2 = exitRateVector.begin();
        for (typename std::vector<ValueType>::iterator it1 = totalRewardVector.begin(), ite1 = totalRewardVector.end(); it1 != ite1; ++it1, ++it2) {
            *it1 /= *it2;
        }
        if (rewardModel.hasStateActionRewards()) {
            storm::utility::vector::addVectors(totalRewardVector, rewardModel.getStateActionRewardVector(), totalRewardVector);
        }
        if (rewardModel.hasTransitionRewards()) {
            storm::utility::vector::addVectors(totalRewardVector, probabilityMatrix.getPointwiseProductRowSumVector(rewardModel.getTransitionRewardMatrix()),
                                               totalRewardVector);
        }
    } else if (rewardModel.hasTransitionRewards()) {
        totalRewardVector = probabilityMatrix.getPointwiseProductRowSumVector(rewardModel.getTransitionRewardMatrix());
        if (rewardModel.hasStateActionRewards()) {
            storm::utility::vector::addVectors(totalRewardVector, rewardModel.getStateActionRewardVector(), totalRewardVector);
        }
    } else {
        totalRewardVector = rewardModel.getStateActionRewardVector();
    }

    RewardModelType dtmcRewardModel(std::move(totalRewardVector));
    return storm::modelchecker::helper::SparseDtmcPrctlHelper<ValueType>::computeTotalRewards(env, std::move(goal), probabilityMatrix, backwardTransitions,
                                                                                              dtmcRewardModel, qualitative);
}

template<typename ValueType>
    requires storm::NumberTraits<ValueType>::SupportsExponential
std::vector<ValueType> SparseCtmcCslHelper::computeAllTransientProbabilities(Environment const& env, storm::storage::SparseMatrix<ValueType> const& rateMatrix,
                                                                             storm::storage::BitVector const& initialStates,
                                                                             storm::storage::BitVector const& phiStates,
                                                                             storm::storage::BitVector const& psiStates,
                                                                             std::vector<ValueType> const& exitRates, ValueType timeBound) {
    // Compute transient probabilities going from initial state
    // Instead of y=Px we now compute y=xP <=> y^T=P^Tx^T via transposition
    uint_fast64_t numberOfStates = rateMatrix.getRowCount();

    // Create the result vector.
    std::vector<ValueType> result = std::vector<ValueType>(numberOfStates, storm::utility::zero<ValueType>());

    storm::storage::SparseMatrix<ValueType> transposedMatrix(rateMatrix);
    transposedMatrix.makeRowsAbsorbing(psiStates);
    std::vector<ValueType> newRates = exitRates;
    for (auto state : psiStates) {
        newRates[state] = storm::utility::one<ValueType>();
    }

    // Identify all maybe states which have a probability greater than 0 to be reached from the initial state.
    // storm::storage::BitVector statesWithProbabilityGreater0 = storm::utility::graph::performProbGreater0(transposedMatrix, phiStates, initialStates);
    // STORM_LOG_INFO("Found " << statesWithProbabilityGreater0.getNumberOfSetBits() << " states with probability greater 0.");

    // storm::storage::BitVector relevantStates = statesWithProbabilityGreater0 & ~initialStates;//phiStates | psiStates;
    storm::storage::BitVector relevantStates(numberOfStates, true);
    STORM_LOG_DEBUG(relevantStates.getNumberOfSetBits() << " relevant states.");

    if (!relevantStates.empty()) {
        // Find the maximal rate of all relevant states to take it as the uniformization rate.
        ValueType uniformizationRate = 0;
        for (auto state : relevantStates) {
            uniformizationRate = std::max(uniformizationRate, newRates[state]);
        }
        uniformizationRate *= 1.02;
        STORM_LOG_THROW(uniformizationRate > 0, storm::exceptions::InvalidStateException, "The uniformization rate must be positive.");

        transposedMatrix = transposedMatrix.transpose();

        // Compute the uniformized matrix.
        storm::storage::SparseMatrix<ValueType> uniformizedMatrix = computeUniformizedMatrix(transposedMatrix, relevantStates, uniformizationRate, newRates);

        // Compute the vector that is to be added as a compensation for removing the absorbing states.
        /*std::vector<ValueType> b = transposedMatrix.getConstrainedRowSumVector(relevantStates, initialStates);
        for (auto& element : b) {
            element /= uniformizationRate;
            std::cout << element << '\n';
        }*/

        ValueType epsilon = storm::utility::convertNumber<ValueType>(env.solver().timeBounded().getPrecision()) / 8.0;
        STORM_LOG_WARN_COND(!env.solver().timeBounded().getRelativeTerminationCriterion(),
                            "Computation of transient probabilities with relative precision not supported. Using absolute precision instead.");
        std::vector<ValueType> values(relevantStates.getNumberOfSetBits(), storm::utility::zero<ValueType>());
        // Set initial states
        size_t i = 0;
        ValueType initDist = storm::utility::one<ValueType>() / initialStates.getNumberOfSetBits();
        for (auto state : relevantStates) {
            if (initialStates.get(state)) {
                values[i] = initDist;
            }
            ++i;
        }
        // Finally compute the transient probabilities.
        std::vector<ValueType> subresult =
            computeTransientProbabilities<ValueType>(env, uniformizedMatrix, nullptr, timeBound, uniformizationRate, values, epsilon);

        storm::utility::vector::setVectorValues(result, relevantStates, subresult);
    }

    return result;
}

template<typename ValueType>
    requires storm::NumberTraits<ValueType>::SupportsExponential
storm::storage::SparseMatrix<ValueType> SparseCtmcCslHelper::computeUniformizedMatrix(storm::storage::SparseMatrix<ValueType> const& rateMatrix,
                                                                                      storm::storage::BitVector const& maybeStates,
                                                                                      ValueType uniformizationRate, std::vector<ValueType> const& exitRates) {
    STORM_LOG_DEBUG("Computing uniformized matrix using uniformization rate " << uniformizationRate << ".");
    STORM_LOG_DEBUG("Keeping " << maybeStates.getNumberOfSetBits() << " rows.");

    // Create the submatrix that only contains the states with a positive probability (including the
    // psi states) and reserve space for elements on the diagonal.
    storm::storage::SparseMatrix<ValueType> uniformizedMatrix = rateMatrix.getSubmatrix(false, maybeStates, maybeStates, true);

    // Now we need to perform the actual uniformization. That is, all entries need to be divided by
    // the uniformization rate, and the diagonal needs to be set to the negative exit rate of the
    // state plus the self-loop rate and then increased by one.
    uint_fast64_t currentRow = 0;
    for (auto state : maybeStates) {
        for (auto& element : uniformizedMatrix.getRow(currentRow)) {
            if (element.getColumn() == currentRow) {
                element.setValue((element.getValue() - exitRates[state]) / uniformizationRate + storm::utility::one<ValueType>());
            } else {
                element.setValue(element.getValue() / uniformizationRate);
            }
        }
        ++currentRow;
    }

    return uniformizedMatrix;
}

template<typename ValueType, bool useMixedPoissonProbabilities>
    requires storm::NumberTraits<ValueType>::SupportsExponential
std::vector<ValueType> SparseCtmcCslHelper::computeTransientProbabilities(Environment const& env,
                                                                          storm::storage::SparseMatrix<ValueType> const& uniformizedMatrix,
                                                                          std::vector<ValueType> const* addVector, ValueType timeBound,
                                                                          ValueType uniformizationRate, std::vector<ValueType> values, ValueType epsilon) {
    STORM_LOG_WARN_COND(epsilon > storm::utility::convertNumber<ValueType>(1e-20),
                        "Very low truncation error " << epsilon << " requested. Numerical inaccuracies are possible.");
    ValueType lambda = timeBound * uniformizationRate;

    // If no time can pass, the current values are the result.
    if (storm::utility::isZero(lambda)) {
        return values;
    }

    // Use Fox-Glynn to get the truncation points and the weights.
    storm::utility::numerical::FoxGlynnResult<ValueType> foxGlynnResult = storm::utility::numerical::foxGlynn(lambda, epsilon);
    STORM_LOG_DEBUG("Fox-Glynn cutoff points: left=" << foxGlynnResult.left << ", right=" << foxGlynnResult.right);
    // foxGlynnResult.weights do not sum up to one. This is to enhance numerical stability.

    // If the cumulative reward is to be computed, we need to adjust the weights.
    if (useMixedPoissonProbabilities) {
        // The following computes a vector v such that
        // v[i] = foxGlynnResult.totalWeight - sum_{j=0}^{i} foxGlynnResult.weights[j]
        //      = sum_{j=i+1}^{n-1} foxGlynnResult.weights[j]  for i=0,...,n-1
        // and then sets foxGlynnResult.weights = v / uniformizationRate.
        // We do this in place and with numerical stability in mind. Note that the weights commonly range to values from 1e-200 to 1e+200
        uint64_t l{0ull}, r{foxGlynnResult.weights.size() - 1};
        ValueType sumLeft{storm::utility::zero<ValueType>()}, sumRight{storm::utility::zero<ValueType>()};
        while (l <= r) {
            if (foxGlynnResult.weights[l] < foxGlynnResult.weights[r]) {
                sumLeft += foxGlynnResult.weights[l];
                foxGlynnResult.weights[l] = (foxGlynnResult.totalWeight - sumLeft) / uniformizationRate;
                ++l;
            } else {
                auto const tmp = foxGlynnResult.weights[r];
                foxGlynnResult.weights[r] = sumRight / uniformizationRate;
                sumRight += tmp;
                if (r == 0) {
                    // Avoid underflow for unsigned int
                    break;
                }
                --r;
            }
        }
        auto const relDiff = storm::utility::abs<ValueType>(foxGlynnResult.totalWeight - (sumLeft + sumRight)) / foxGlynnResult.totalWeight;
        STORM_LOG_WARN_COND(relDiff < storm::utility::convertNumber<ValueType>(1e-8),
                            "Numerical instability when adjusting the FoxGlynn weights. Relative Difference: " << relDiff << ".");
    }

    STORM_LOG_DEBUG("Starting iterations with " << uniformizedMatrix.getRowCount() << " x " << uniformizedMatrix.getColumnCount() << " matrix.");

    // Initialize result.
    std::vector<ValueType> result;
    uint_fast64_t startingIteration = foxGlynnResult.left;
    if (startingIteration == 0) {
        result = values;
        storm::utility::vector::scaleVectorInPlace(result, foxGlynnResult.weights.front());
        ++startingIteration;
    } else {
        if (useMixedPoissonProbabilities) {
            result = std::vector<ValueType>(values.size());
            std::function<ValueType(ValueType const&)> scaleWithUniformizationRate = [&uniformizationRate](ValueType const& a) -> ValueType {
                return a / uniformizationRate;
            };
            storm::utility::vector::applyPointwise(values, result, scaleWithUniformizationRate);
        } else {
            result = std::vector<ValueType>(values.size());
        }
    }

    auto multiplier = storm::solver::MultiplierFactory<ValueType>().create(env, uniformizedMatrix);
    if (!useMixedPoissonProbabilities && foxGlynnResult.left > 1) {
        // Perform the matrix-vector multiplications (without adding).
        multiplier->repeatedMultiply(env, values, addVector, foxGlynnResult.left - 1);
    } else if (useMixedPoissonProbabilities) {
        std::function<ValueType(ValueType const&, ValueType const&)> addAndScale = [&uniformizationRate](ValueType const& a, ValueType const& b) {
            return a + b / uniformizationRate;
        };

        // For the iterations below the left truncation point, we need to add and scale the result with the uniformization rate.
        for (uint_fast64_t index = 1; index < startingIteration; ++index) {
            multiplier->multiply(env, values, nullptr, values);
            storm::utility::vector::applyPointwise(result, values, result, addAndScale);
        }
        // To make sure that the values obtained before the left truncation point have the same 'impact' on the total result as the values obtained
        // between the left and right truncation point, we scale them here with the total sum of the weights.
        // Note that we divide with this value afterwards. This is to improve numerical stability.
        if (foxGlynnResult.left > 0) {
            storm::utility::vector::scaleVectorInPlace<ValueType, ValueType>(result, foxGlynnResult.totalWeight);
        }
    }

    // For the indices that fall in between the truncation points, we need to perform the matrix-vector
    // multiplication, scale and add the result.
    ValueType weight = 0;
    std::function<ValueType(ValueType const&, ValueType const&)> addAndScale = [&weight](ValueType const& a, ValueType const& b) { return a + weight * b; };
    for (uint_fast64_t index = startingIteration; index <= foxGlynnResult.right; ++index) {
        multiplier->multiply(env, values, addVector, values);

        weight = foxGlynnResult.weights[index - foxGlynnResult.left];
        storm::utility::vector::applyPointwise(result, values, result, addAndScale);
    }

    // Finally, divide the result by the total weight
    storm::utility::vector::scaleVectorInPlace<ValueType, ValueType>(result, storm::utility::one<ValueType>() / foxGlynnResult.totalWeight);
    return result;
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> SparseCtmcCslHelper::computeProbabilityMatrix(storm::storage::SparseMatrix<ValueType> const& rateMatrix,
                                                                                      std::vector<ValueType> const& exitRates) {
    // Turn the rates into probabilities by scaling each row with the exit rate of the state.
    storm::storage::SparseMatrix<ValueType> result(rateMatrix);
    for (uint_fast64_t row = 0; row < result.getRowCount(); ++row) {
        for (auto& entry : result.getRow(row)) {
            entry.setValue(entry.getValue() / exitRates[row]);
        }
    }
    return result;
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> SparseCtmcCslHelper::computeGeneratorMatrix(storm::storage::SparseMatrix<ValueType> const& rateMatrix,
                                                                                    std::vector<ValueType> const& exitRates) {
    storm::storage::SparseMatrix<ValueType> generatorMatrix(rateMatrix, true);

    // Place the negative exit rate on the diagonal.
    for (uint_fast64_t row = 0; row < generatorMatrix.getRowCount(); ++row) {
        for (auto& entry : generatorMatrix.getRow(row)) {
            if (entry.getColumn() == row) {
                entry.setValue(-exitRates[row] + entry.getValue());
            }
        }
    }

    return generatorMatrix;
}

template std::vector<double> SparseCtmcCslHelper::computeBoundedUntilProbabilities(
    Environment const& env, storm::solver::SolveGoal<double>&& goal, storm::storage::SparseMatrix<double> const& rateMatrix,
    storm::storage::SparseMatrix<double> const& backwardTransitions, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
    std::vector<double> const& exitRates, bool qualitative, double lowerBound, double upperBound);

template std::vector<double> SparseCtmcCslHelper::computeUntilProbabilities(Environment const& env, storm::solver::SolveGoal<double>&& goal,
                                                                            storm::storage::SparseMatrix<double> const& rateMatrix,
                                                                            storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                                            std::vector<double> const& exitRateVector,
                                                                            storm::storage::BitVector const& phiStates,
                                                                            storm::storage::BitVector const& psiStates, bool qualitative);

template std::vector<double> SparseCtmcCslHelper::computeAllUntilProbabilities(Environment const& env, storm::solver::SolveGoal<double>&& goal,
                                                                               storm::storage::SparseMatrix<double> const& rateMatrix,
                                                                               std::vector<double> const& exitRateVector,
                                                                               storm::storage::BitVector const& initialStates,
                                                                               storm::storage::BitVector const& phiStates,
                                                                               storm::storage::BitVector const& psiStates);

template std::vector<double> SparseCtmcCslHelper::computeNextProbabilities(Environment const& env, storm::storage::SparseMatrix<double> const& rateMatrix,
                                                                           std::vector<double> const& exitRateVector,
                                                                           storm::storage::BitVector const& nextStates);

template std::vector<double> SparseCtmcCslHelper::computeInstantaneousRewards(Environment const& env, storm::solver::SolveGoal<double>&& goal,
                                                                              storm::storage::SparseMatrix<double> const& rateMatrix,
                                                                              std::vector<double> const& exitRateVector,
                                                                              storm::models::sparse::StandardRewardModel<double> const& rewardModel,
                                                                              double timeBound);

template std::vector<double> SparseCtmcCslHelper::computeReachabilityTimes(Environment const& env, storm::solver::SolveGoal<double>&& goal,
                                                                           storm::storage::SparseMatrix<double> const& rateMatrix,
                                                                           storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                                           std::vector<double> const& exitRateVector,
                                                                           storm::storage::BitVector const& targetStates, bool qualitative);

template std::vector<double> SparseCtmcCslHelper::computeReachabilityRewards(Environment const& env, storm::solver::SolveGoal<double>&& goal,
                                                                             storm::storage::SparseMatrix<double> const& rateMatrix,
                                                                             storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                                             std::vector<double> const& exitRateVector,
                                                                             storm::models::sparse::StandardRewardModel<double> const& rewardModel,
                                                                             storm::storage::BitVector const& targetStates, bool qualitative);

template std::vector<double> SparseCtmcCslHelper::computeTotalRewards(Environment const& env, storm::solver::SolveGoal<double>&& goal,
                                                                      storm::storage::SparseMatrix<double> const& rateMatrix,
                                                                      storm::storage::SparseMatrix<double> const& backwardTransitions,
                                                                      std::vector<double> const& exitRateVector,
                                                                      storm::models::sparse::StandardRewardModel<double> const& rewardModel, bool qualitative);

template std::vector<double> SparseCtmcCslHelper::computeCumulativeRewards(Environment const& env, storm::solver::SolveGoal<double>&& goal,
                                                                           storm::storage::SparseMatrix<double> const& rateMatrix,
                                                                           std::vector<double> const& exitRateVector,
                                                                           storm::models::sparse::StandardRewardModel<double> const& rewardModel,
                                                                           double timeBound);

template std::vector<double> SparseCtmcCslHelper::computeAllTransientProbabilities(
    Environment const& env, storm::storage::SparseMatrix<double> const& rateMatrix, storm::storage::BitVector const& initialStates,
    storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates, std::vector<double> const& exitRates, double timeBound);

template storm::storage::SparseMatrix<double> SparseCtmcCslHelper::computeUniformizedMatrix(storm::storage::SparseMatrix<double> const& rateMatrix,
                                                                                            storm::storage::BitVector const& maybeStates,
                                                                                            double uniformizationRate, std::vector<double> const& exitRates);

template std::vector<double> SparseCtmcCslHelper::computeTransientProbabilities(Environment const& env,
                                                                                storm::storage::SparseMatrix<double> const& uniformizedMatrix,
                                                                                std::vector<double> const* addVector, double timeBound,
                                                                                double uniformizationRate, std::vector<double> values, double epsilon);

template std::vector<storm::RationalNumber> SparseCtmcCslHelper::computeUntilProbabilities(
    Environment const& env, storm::solver::SolveGoal<storm::RationalNumber>&& goal, storm::storage::SparseMatrix<storm::RationalNumber> const& rateMatrix,
    storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions, std::vector<storm::RationalNumber> const& exitRateVector,
    storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates, bool qualitative);
template std::vector<storm::RationalFunction> SparseCtmcCslHelper::computeUntilProbabilities(
    Environment const& env, storm::solver::SolveGoal<storm::RationalFunction>&& goal, storm::storage::SparseMatrix<storm::RationalFunction> const& rateMatrix,
    storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions, std::vector<storm::RationalFunction> const& exitRateVector,
    storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates, bool qualitative);

template std::vector<storm::RationalNumber> SparseCtmcCslHelper::computeAllUntilProbabilities(
    Environment const& env, storm::solver::SolveGoal<storm::RationalNumber>&& goal, storm::storage::SparseMatrix<storm::RationalNumber> const& rateMatrix,
    std::vector<storm::RationalNumber> const& exitRateVector, storm::storage::BitVector const& initialStates, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);
template std::vector<storm::RationalFunction> SparseCtmcCslHelper::computeAllUntilProbabilities(
    Environment const& env, storm::solver::SolveGoal<storm::RationalFunction>&& goal, storm::storage::SparseMatrix<storm::RationalFunction> const& rateMatrix,
    std::vector<storm::RationalFunction> const& exitRateVector, storm::storage::BitVector const& initialStates, storm::storage::BitVector const& phiStates,
    storm::storage::BitVector const& psiStates);

template std::vector<storm::RationalNumber> SparseCtmcCslHelper::computeNextProbabilities(Environment const& env,
                                                                                          storm::storage::SparseMatrix<storm::RationalNumber> const& rateMatrix,
                                                                                          std::vector<storm::RationalNumber> const& exitRateVector,
                                                                                          storm::storage::BitVector const& nextStates);
template std::vector<storm::RationalFunction> SparseCtmcCslHelper::computeNextProbabilities(
    Environment const& env, storm::storage::SparseMatrix<storm::RationalFunction> const& rateMatrix, std::vector<storm::RationalFunction> const& exitRateVector,
    storm::storage::BitVector const& nextStates);

template std::vector<storm::RationalNumber> SparseCtmcCslHelper::computeReachabilityTimes(
    Environment const& env, storm::solver::SolveGoal<storm::RationalNumber>&& goal, storm::storage::SparseMatrix<storm::RationalNumber> const& rateMatrix,
    storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions, std::vector<storm::RationalNumber> const& exitRateVector,
    storm::storage::BitVector const& targetStates, bool qualitative);
template std::vector<storm::RationalFunction> SparseCtmcCslHelper::computeReachabilityTimes(
    Environment const& env, storm::solver::SolveGoal<storm::RationalFunction>&& goal, storm::storage::SparseMatrix<storm::RationalFunction> const& rateMatrix,
    storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions, std::vector<storm::RationalFunction> const& exitRateVector,
    storm::storage::BitVector const& targetStates, bool qualitative);

template std::vector<storm::RationalNumber> SparseCtmcCslHelper::computeReachabilityRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalNumber>&& goal, storm::storage::SparseMatrix<storm::RationalNumber> const& rateMatrix,
    storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions, std::vector<storm::RationalNumber> const& exitRateVector,
    storm::models::sparse::StandardRewardModel<storm::RationalNumber> const& rewardModel, storm::storage::BitVector const& targetStates, bool qualitative);
template std::vector<storm::RationalFunction> SparseCtmcCslHelper::computeReachabilityRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalFunction>&& goal, storm::storage::SparseMatrix<storm::RationalFunction> const& rateMatrix,
    storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions, std::vector<storm::RationalFunction> const& exitRateVector,
    storm::models::sparse::StandardRewardModel<storm::RationalFunction> const& rewardModel, storm::storage::BitVector const& targetStates, bool qualitative);

template std::vector<storm::RationalNumber> SparseCtmcCslHelper::computeTotalRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalNumber>&& goal, storm::storage::SparseMatrix<storm::RationalNumber> const& rateMatrix,
    storm::storage::SparseMatrix<storm::RationalNumber> const& backwardTransitions, std::vector<storm::RationalNumber> const& exitRateVector,
    storm::models::sparse::StandardRewardModel<storm::RationalNumber> const& rewardModel, bool qualitative);
template std::vector<storm::RationalFunction> SparseCtmcCslHelper::computeTotalRewards(
    Environment const& env, storm::solver::SolveGoal<storm::RationalFunction>&& goal, storm::storage::SparseMatrix<storm::RationalFunction> const& rateMatrix,
    storm::storage::SparseMatrix<storm::RationalFunction> const& backwardTransitions, std::vector<storm::RationalFunction> const& exitRateVector,
    storm::models::sparse::StandardRewardModel<storm::RationalFunction> const& rewardModel, bool qualitative);

template storm::storage::SparseMatrix<double> SparseCtmcCslHelper::computeProbabilityMatrix(storm::storage::SparseMatrix<double> const& rateMatrix,
                                                                                            std::vector<double> const& exitRates);
template storm::storage::SparseMatrix<storm::RationalNumber> SparseCtmcCslHelper::computeProbabilityMatrix(
    storm::storage::SparseMatrix<storm::RationalNumber> const& rateMatrix, std::vector<storm::RationalNumber> const& exitRates);
template storm::storage::SparseMatrix<storm::RationalFunction> SparseCtmcCslHelper::computeProbabilityMatrix(
    storm::storage::SparseMatrix<storm::RationalFunction> const& rateMatrix, std::vector<storm::RationalFunction> const& exitRates);

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
