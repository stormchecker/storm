#pragma once

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "storm/environment/Environment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/modelchecker/cvar/CvarComputationResult.h"
#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/modelchecker/cvar/preprocessing/WeightedReachabilityCvarPreprocessingResult.h"
#include "storm/modelchecker/prctl/helper/SparseMdpPrctlHelper.h"
#include "storm/solver/LpSolver.h"
#include "storm/solver/SolveGoal.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/expressions/BinaryRelationType.h"
#include "storm/utility/ConstantsComparator.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/solver.h"

namespace storm {
namespace modelchecker {
namespace cvar {

template<typename ValueType>
struct CvarThresholdData {
    ValueType threshold;
    storm::storage::BitVector targetStatesBelowThreshold;
    storm::storage::BitVector targetStatesAtThreshold;
    storm::storage::BitVector targetStatesBelowOrAtThreshold;
};

template<typename ValueType>
struct CvarRewardBucket {
    ValueType reward;
    std::vector<uint64_t> targetStates;
};

template<typename ValueType>
struct WeightedReachabilityCvarLpData {
    double alpha;
    storm::solver::OptimizationDirection optimizationDirection;
    uint64_t initialState;
    storm::storage::BitVector initialStates;
    std::string rewardModelName;
    storm::storage::BitVector targetStates;
    std::vector<ValueType> terminalRewards;
    std::vector<CvarRewardBucket<ValueType>> rewardBuckets;
    storm::storage::SparseMatrix<ValueType> transitionMatrix;
    storm::storage::SparseMatrix<ValueType> backwardChoices;
    storm::storage::SparseMatrix<ValueType> backwardTransitions;
};

template<typename ValueType>
std::vector<CvarRewardBucket<ValueType>> collectRewardBuckets(storm::storage::BitVector const& targetStates, std::vector<ValueType> const& terminalRewards,
                                                              storm::storage::BitVector const& reachableStates) {
    std::map<ValueType, std::vector<uint64_t>> buckets;
    for (auto state : targetStates) {
        if (!reachableStates[state]) {
            continue;
        }
        buckets[terminalRewards[state]].push_back(state);
    }

    std::vector<CvarRewardBucket<ValueType>> result;
    result.reserve(buckets.size());
    for (auto& bucket : buckets) {
        result.push_back({bucket.first, std::move(bucket.second)});
    }
    return result;
}

/*!
 * Solves an LP for Conditional Value-at-Risk on an MDP with a terminal reward objective.
 *
 * Supported CLI shape:
 *   storm --prism model.nm --prop 'R{"reward"}min/max=? [ F "target" ]' --cvar <alpha>
 *   storm --prism model.nm --prop 'R{"reward"}min/max=? [ F "target" ]' --cvar <alpha> --cvar:method wr
 *
 * The --cvar option requires exactly one selected property. That property must be unfiltered and must be an unbounded
 * reward query with an optimization direction (min or max) and an eventually formula F phi whose target phi is a state
 * formula. The reward model may be named explicitly (R{"reward"}...) or omitted if the model has a unique reward model.
 *
 * Requirements: sparse MDP, 0 < alpha < 1, exactly one initial state, state-based terminal rewards,
 * reward 0 on non-target states, and absorbing original target states.
 *
 * @see https://doi.org/10.1145/3209108.3209176 Fig. 4 for a description of the algorithm as implemented (and slightly altered) here.
 */
template<typename ValueType>
class SparseWeightedReachabilityCvarLpHelper {
   public:
    SparseWeightedReachabilityCvarLpHelper(CvarQueryInformation const& queryInformation,
                                           preprocessing::WeightedReachabilityCvarPreprocessingResult<ValueType> const& weightedReachabilityPreprocessingResult)
        : lpData(createLpData(queryInformation, weightedReachabilityPreprocessingResult)) {}

    CvarComputationResult<ValueType> computeCvar(Environment const& env, bool produceScheduler = false) const {
        STORM_LOG_THROW(!lpData.rewardBuckets.empty(), storm::exceptions::NotImplementedException,
                        "CVaR model checking requires at least one target reward threshold candidate.");

        std::optional<ValueType> bestValue;
        std::optional<uint64_t> bestThresholdIndex;
        std::unique_ptr<storm::storage::Scheduler<ValueType>> bestScheduler;
        auto candidateRange = computeCandidateRange(env);
        storm::storage::BitVector targetStatesBelowThreshold = createPrefixTargetStates(candidateRange.first);
        for (uint64_t thresholdIndex = candidateRange.first; thresholdIndex < candidateRange.second; ++thresholdIndex) {
            auto thresholdData = createThresholdData(thresholdIndex, targetStatesBelowThreshold);
            auto thresholdResult = buildLpForThreshold(thresholdData, false);
            if (!thresholdResult.has_value()) {
                addBucketStates(targetStatesBelowThreshold, thresholdIndex);
                continue;
            }
            if (!bestValue.has_value()) {
                bestValue = thresholdResult->value;
                bestThresholdIndex = thresholdIndex;
            } else if ((storm::solver::minimize(lpData.optimizationDirection) && thresholdResult->value < bestValue.value()) ||
                       (storm::solver::maximize(lpData.optimizationDirection) && thresholdResult->value > bestValue.value())) {
                bestValue = thresholdResult->value;
                bestThresholdIndex = thresholdIndex;
            }
            addBucketStates(targetStatesBelowThreshold, thresholdIndex);
        }

        STORM_LOG_THROW(bestValue.has_value(), storm::exceptions::UnexpectedException,
                        "CVaR model checking did not find a feasible LP for any threshold candidate.");
        if (produceScheduler) {
            STORM_LOG_ASSERT(bestThresholdIndex.has_value(), "Expected a threshold index for the best CVaR LP value.");
            auto thresholdData = createThresholdData(bestThresholdIndex.value());
            auto thresholdResult = buildLpForThreshold(thresholdData, true);
            STORM_LOG_THROW(thresholdResult.has_value(), storm::exceptions::UnexpectedException,
                            "The previously optimal CVaR LP threshold became infeasible when extracting a scheduler.");
            bestScheduler = std::move(thresholdResult->scheduler);
        }
        return {bestValue.value() / storm::utility::convertNumber<ValueType>(lpData.alpha), std::move(bestScheduler)};
    }

   private:
    static WeightedReachabilityCvarLpData<ValueType> createLpData(
        CvarQueryInformation const& queryInformation,
        preprocessing::WeightedReachabilityCvarPreprocessingResult<ValueType> const& weightedReachabilityPreprocessingResult) {
        auto initialStates = createInitialStateBitVector(weightedReachabilityPreprocessingResult.transitionMatrix.getRowGroupCount(),
                                                         weightedReachabilityPreprocessingResult.initialState);
        storm::storage::BitVector allStates(weightedReachabilityPreprocessingResult.transitionMatrix.getRowGroupCount(), true);
        storm::storage::BitVector noStates(weightedReachabilityPreprocessingResult.transitionMatrix.getRowGroupCount(), false);
        auto reachableStates =
            storm::utility::graph::getReachableStates(weightedReachabilityPreprocessingResult.transitionMatrix, initialStates, allStates, noStates);
        auto rewardBuckets = collectRewardBuckets(weightedReachabilityPreprocessingResult.effectiveTargetStates,
                                                  weightedReachabilityPreprocessingResult.terminalRewards, reachableStates);
        return {queryInformation.alpha,
                queryInformation.optimizationDirection,
                weightedReachabilityPreprocessingResult.initialState,
                std::move(initialStates),
                weightedReachabilityPreprocessingResult.rewardModelName,
                weightedReachabilityPreprocessingResult.effectiveTargetStates,
                weightedReachabilityPreprocessingResult.terminalRewards,
                std::move(rewardBuckets),
                weightedReachabilityPreprocessingResult.transitionMatrix,
                weightedReachabilityPreprocessingResult.transitionMatrix.transpose(),
                weightedReachabilityPreprocessingResult.transitionMatrix.transpose(true)};
    }

    static storm::storage::BitVector createInitialStateBitVector(uint64_t stateCount, uint64_t initialState) {
        storm::storage::BitVector initialStates(stateCount, false);
        initialStates.set(initialState, true);
        return initialStates;
    }

    void addBucketStates(storm::storage::BitVector& states, uint64_t bucketIndex) const {
        for (auto state : lpData.rewardBuckets[bucketIndex].targetStates) {
            states.set(state, true);
        }
    }

    storm::storage::BitVector createBucketTargetStates(uint64_t bucketIndex) const {
        storm::storage::BitVector states(lpData.transitionMatrix.getRowGroupCount(), false);
        addBucketStates(states, bucketIndex);
        return states;
    }

    storm::storage::BitVector createPrefixTargetStates(uint64_t endBucketIndex) const {
        storm::storage::BitVector states(lpData.transitionMatrix.getRowGroupCount(), false);
        for (uint64_t bucketIndex = 0; bucketIndex < endBucketIndex; ++bucketIndex) {
            addBucketStates(states, bucketIndex);
        }
        return states;
    }

    storm::storage::BitVector const& getCachedPrefixTargetStates(uint64_t endBucketIndex, std::map<uint64_t, storm::storage::BitVector>& cache) const {
        auto cachedPrefix = cache.find(endBucketIndex);
        if (cachedPrefix != cache.end()) {
            return cachedPrefix->second;
        }
        return cache.emplace(endBucketIndex, createPrefixTargetStates(endBucketIndex)).first->second;
    }

    CvarThresholdData<ValueType> createThresholdData(uint64_t thresholdIndex) const {
        auto targetStatesBelowThreshold = createPrefixTargetStates(thresholdIndex);
        return createThresholdData(thresholdIndex, targetStatesBelowThreshold);
    }

    CvarThresholdData<ValueType> createThresholdData(uint64_t thresholdIndex, storm::storage::BitVector const& targetStatesBelowThreshold) const {
        auto targetStatesAtThreshold = createBucketTargetStates(thresholdIndex);
        auto targetStatesBelowOrAtThreshold = targetStatesBelowThreshold | targetStatesAtThreshold;
        return {lpData.rewardBuckets[thresholdIndex].reward, targetStatesBelowThreshold, std::move(targetStatesAtThreshold),
                std::move(targetStatesBelowOrAtThreshold)};
    }

    ValueType computeReachabilityProbability(Environment const& env, storm::solver::OptimizationDirection direction,
                                             storm::storage::BitVector const& targetStates) const {
        if (targetStates.empty()) {
            return storm::utility::zero<ValueType>();
        }
        if (targetStates[lpData.initialState]) {
            return storm::utility::one<ValueType>();
        }

        storm::storage::BitVector allStates(lpData.transitionMatrix.getRowGroupCount(), true);
        auto result = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, ValueType>::computeUntilProbabilities(
            env, storm::solver::SolveGoal<ValueType, ValueType>(direction, lpData.initialStates), lpData.transitionMatrix, lpData.backwardTransitions,
            allStates, targetStates, false, false);
        return result.values[lpData.initialState];
    }

    std::pair<uint64_t, uint64_t> computeCandidateRange(Environment const& env) const {
        uint64_t const bucketCount = lpData.rewardBuckets.size();
        ValueType const alpha = storm::utility::convertNumber<ValueType>(lpData.alpha);
        storm::utility::ConstantsComparator<ValueType> comparator(storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision()));
        std::map<uint64_t, storm::storage::BitVector> prefixTargetStateCache;

        uint64_t lower = 0;
        uint64_t upper = bucketCount;
        while (lower < upper) {
            uint64_t const mid = lower + (upper - lower) / 2;
            auto const& targetStatesBelowOrAtThreshold = getCachedPrefixTargetStates(mid + 1, prefixTargetStateCache);
            auto maxReachability = computeReachabilityProbability(env, storm::solver::OptimizationDirection::Maximize, targetStatesBelowOrAtThreshold);
            if (comparator.isLess(maxReachability, alpha)) {
                lower = mid + 1;
            } else {
                upper = mid;
            }
        }
        uint64_t const firstNotTooLow = lower;

        lower = firstNotTooLow;
        upper = bucketCount;
        while (lower < upper) {
            uint64_t const mid = lower + (upper - lower) / 2;
            auto const& targetStatesBelowThreshold = getCachedPrefixTargetStates(mid, prefixTargetStateCache);
            auto minReachability = computeReachabilityProbability(env, storm::solver::OptimizationDirection::Minimize, targetStatesBelowThreshold);
            if (comparator.isLess(alpha, minReachability)) {
                upper = mid;
            } else {
                lower = mid + 1;
            }
        }

        return {firstNotTooLow, lower};
    }

    std::optional<CvarComputationResult<ValueType>> buildLpForThreshold(CvarThresholdData<ValueType> const& thresholdData, bool produceScheduler) const {
        using RawLpSolver = storm::solver::LpSolver<ValueType, true>;
        using RawLpConstraint = storm::solver::RawLpConstraint<ValueType>;

        auto lpSolverFactory = storm::utility::solver::getLpSolverFactory<ValueType>();
        auto solver = lpSolverFactory->createRaw("cvar");
        solver->setOptimizationDirection(lpData.optimizationDirection);

        std::vector<typename RawLpSolver::Variable> actionFlowVariables;
        actionFlowVariables.reserve(lpData.transitionMatrix.getRowCount());
        for (uint64_t row = 0; row < lpData.transitionMatrix.getRowCount(); ++row) {
            actionFlowVariables.push_back(solver->addLowerBoundedContinuousVariable("y_" + std::to_string(row), storm::utility::zero<ValueType>()));
        }

        std::vector<std::optional<typename RawLpSolver::Variable>> recurrentFlowVariables(lpData.transitionMatrix.getRowGroupCount(), std::nullopt);
        for (uint64_t state = 0; state < lpData.transitionMatrix.getRowGroupCount(); ++state) {
            if (lpData.targetStates[state]) {
                recurrentFlowVariables[state] = solver->addLowerBoundedContinuousVariable("x_" + std::to_string(state), storm::utility::zero<ValueType>());
            }
        }

        std::vector<std::optional<typename RawLpSolver::Variable>> splitFlowVariables(lpData.transitionMatrix.getRowGroupCount(), std::nullopt);
        for (uint64_t state = 0; state < lpData.transitionMatrix.getRowGroupCount(); ++state) {
            if (thresholdData.targetStatesBelowOrAtThreshold[state]) {
                splitFlowVariables[state] =
                    solver->addLowerBoundedContinuousVariable("xb_" + std::to_string(state), storm::utility::zero<ValueType>(), lpData.terminalRewards[state]);
            }
        }

        solver->update();

        for (uint64_t state = 0; state < lpData.transitionMatrix.getRowGroupCount(); ++state) {
            auto outgoingActions = lpData.transitionMatrix.getRowGroupIndices(state);
            auto incomingActions = lpData.backwardChoices.getRow(state);
            uint64_t reservedSize = outgoingActions.size() + incomingActions.getNumberOfEntries() + (recurrentFlowVariables[state].has_value() ? 1 : 0);
            RawLpConstraint constraint(storm::expressions::RelationType::Equal,
                                       state == lpData.initialState ? storm::utility::one<ValueType>() : storm::utility::zero<ValueType>(), reservedSize);
            std::map<typename RawLpSolver::Variable, ValueType> actionCoefficients;

            for (auto const& incomingAction : incomingActions) {
                actionCoefficients[actionFlowVariables[incomingAction.getColumn()]] -= incomingAction.getValue();
            }
            for (auto const& action : outgoingActions) {
                actionCoefficients[actionFlowVariables[action]] += storm::utility::one<ValueType>();
            }
            for (auto const& actionCoefficient : actionCoefficients) {
                if (!storm::utility::isZero(actionCoefficient.second)) {
                    constraint.addToLhs(actionCoefficient.first, actionCoefficient.second);
                }
            }
            if (recurrentFlowVariables[state].has_value()) {
                constraint.addToLhs(recurrentFlowVariables[state].value(), storm::utility::one<ValueType>());
            }

            solver->addConstraint("transient_flow_" + std::to_string(state), constraint);
        }

        RawLpConstraint recurrentConstraint(storm::expressions::RelationType::Equal, storm::utility::one<ValueType>(),
                                            lpData.targetStates.getNumberOfSetBits());

        for (auto state : lpData.targetStates) {
            recurrentConstraint.addToLhs(recurrentFlowVariables[state].value(), storm::utility::one<ValueType>());
        }
        solver->addConstraint("recurrent_behaviour", recurrentConstraint);

        for (auto state : thresholdData.targetStatesBelowThreshold) {
            RawLpConstraint splitEqualityConstraint(storm::expressions::RelationType::Equal, storm::utility::zero<ValueType>(), 2);
            splitEqualityConstraint.addToLhs(splitFlowVariables[state].value(), storm::utility::one<ValueType>());
            splitEqualityConstraint.addToLhs(recurrentFlowVariables[state].value(), -storm::utility::one<ValueType>());
            solver->addConstraint("split_eq_" + std::to_string(state), splitEqualityConstraint);
        }
        for (auto state : thresholdData.targetStatesAtThreshold) {
            RawLpConstraint splitInequalityConstraint(storm::expressions::RelationType::LessOrEqual, storm::utility::zero<ValueType>(), 2);
            splitInequalityConstraint.addToLhs(splitFlowVariables[state].value(), storm::utility::one<ValueType>());
            splitInequalityConstraint.addToLhs(recurrentFlowVariables[state].value(), -storm::utility::one<ValueType>());
            solver->addConstraint("split_le_" + std::to_string(state), splitInequalityConstraint);
        }

        RawLpConstraint probabilityConsistentSplitConstraint(storm::expressions::RelationType::Equal, storm::utility::convertNumber<ValueType>(lpData.alpha),
                                                             thresholdData.targetStatesBelowOrAtThreshold.getNumberOfSetBits());
        for (auto state : thresholdData.targetStatesBelowOrAtThreshold) {
            probabilityConsistentSplitConstraint.addToLhs(splitFlowVariables[state].value(), storm::utility::one<ValueType>());
        }
        solver->addConstraint("probability_consistent_split", probabilityConsistentSplitConstraint);

        solver->optimize();
        if (solver->isInfeasible()) {
            return std::nullopt;
        }
        STORM_LOG_THROW(!solver->isUnbounded(), storm::exceptions::UnexpectedException,
                        "The CVaR LP for threshold " << thresholdData.threshold << " is unbounded.");
        STORM_LOG_THROW(solver->isOptimal(), storm::exceptions::UnexpectedException,
                        "The CVaR LP for threshold " << thresholdData.threshold << " did not reach an optimal solution.");

        std::unique_ptr<storm::storage::Scheduler<ValueType>> scheduler;
        if (produceScheduler) {
            scheduler = std::make_unique<storm::storage::Scheduler<ValueType>>(lpData.transitionMatrix.getRowGroupCount());
            for (uint64_t state = 0; state < lpData.transitionMatrix.getRowGroupCount(); ++state) {
                if (lpData.targetStates[state]) {
                    scheduler->setDontCare(state);
                    continue;
                }

                uint64_t firstRow = lpData.transitionMatrix.getRowGroupIndices()[state];
                uint64_t lastRow = lpData.transitionMatrix.getRowGroupIndices()[state + 1];
                storm::storage::Distribution<ValueType, uint_fast64_t> actionDistribution;
                actionDistribution.reserve(lastRow - firstRow);

                for (uint64_t row = firstRow; row < lastRow; ++row) {
                    auto flow = solver->getContinuousValue(actionFlowVariables[row]);
                    if (storm::utility::isAlmostZero(flow)) {
                        continue;
                    }
                    actionDistribution.addProbability(row - firstRow, flow);
                }

                if (actionDistribution.size() == 0) {
                    scheduler->setDontCare(state);
                    continue;
                }

                actionDistribution.normalize();
                scheduler->setChoice(storm::storage::SchedulerChoice<ValueType>(std::move(actionDistribution)), state);
            }
        }

        return CvarComputationResult<ValueType>{solver->getObjectiveValue(), std::move(scheduler)};
    }

    WeightedReachabilityCvarLpData<ValueType> lpData;
};
}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
