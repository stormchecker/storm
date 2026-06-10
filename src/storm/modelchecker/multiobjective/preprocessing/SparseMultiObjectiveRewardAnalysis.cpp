#include "storm/modelchecker/multiobjective/preprocessing/SparseMultiObjectiveRewardAnalysis.h"

#include <algorithm>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/modelchecker/prctl/helper/BaierUpperRewardBoundsComputer.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/transformer/EndComponentEliminator.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {
namespace preprocessing {

template<typename SparseModelType>
typename SparseMultiObjectiveRewardAnalysis<SparseModelType>::ReturnType SparseMultiObjectiveRewardAnalysis<SparseModelType>::analyze(
    storm::modelchecker::multiobjective::preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType> const& preprocessorResult) {
    ReturnType result;
    auto backwardTransitions = preprocessorResult.preprocessedModel->getBackwardTransitions();

    setReward0States(result, preprocessorResult, backwardTransitions);
    checkRewardFiniteness(result, preprocessorResult, backwardTransitions);

    return result;
}

template<typename SparseModelType>
void SparseMultiObjectiveRewardAnalysis<SparseModelType>::setReward0States(
    ReturnType& result, storm::modelchecker::multiobjective::preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType> const& preprocessorResult,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions) {
    uint_fast64_t stateCount = preprocessorResult.preprocessedModel->getNumberOfStates();
    auto const& transitions = preprocessorResult.preprocessedModel->getTransitionMatrix();
    std::vector<uint_fast64_t> const& groupIndices = transitions.getRowGroupIndices();
    storm::storage::BitVector allStates(stateCount, true);

    // Get the choices without any reward for the various objective types
    storm::storage::BitVector zeroLraRewardChoices(preprocessorResult.preprocessedModel->getNumberOfChoices(), true);
    storm::storage::BitVector zeroTotalRewardChoices(preprocessorResult.preprocessedModel->getNumberOfChoices(), true);
    storm::storage::BitVector zeroCumulativeRewardChoices(preprocessorResult.preprocessedModel->getNumberOfChoices(), true);
    for (auto const& obj : preprocessorResult.objectives) {
        if (obj.formula->isRewardOperatorFormula()) {
            auto const& rewModel = preprocessorResult.preprocessedModel->getRewardModel(obj.formula->asRewardOperatorFormula().getRewardModelName());
            if (obj.formula->getSubformula().isLongRunAverageRewardFormula()) {
                zeroLraRewardChoices &= rewModel.getChoicesWithZeroReward(transitions);
            } else if (obj.formula->getSubformula().isTotalRewardFormula()) {
                zeroTotalRewardChoices &= rewModel.getChoicesWithZeroReward(transitions);
            } else {
                STORM_LOG_WARN_COND(obj.formula->getSubformula().isCumulativeRewardFormula(),
                                    "Analyzing subformula " << obj.formula->getSubformula() << " is not supported properly.");
                zeroCumulativeRewardChoices &= rewModel.getChoicesWithZeroReward(transitions);
            }
        }
    }

    // get the states for which there is a scheduler yielding total reward zero
    auto statesWithTotalRewardForAllChoices = transitions.getRowGroupFilter(~zeroTotalRewardChoices, true);
    result.totalReward0EStates = storm::utility::graph::performProbGreater0A(transitions, groupIndices, backwardTransitions, allStates,
                                                                             statesWithTotalRewardForAllChoices, false, 0, zeroTotalRewardChoices);
    result.totalReward0EStates.complement();

    // Get the states for which all schedulers yield a reward of 0
    // Starting with LRA objectives
    auto statesWithoutLraReward = transitions.getRowGroupFilter(zeroLraRewardChoices, true);
    // Compute Sat(Forall F (Forall G "LRAStatesWithoutReward"))
    auto forallGloballyStatesWithoutLraReward = storm::utility::graph::performProb0A(backwardTransitions, statesWithoutLraReward, ~statesWithoutLraReward);
    result.reward0AStates =
        storm::utility::graph::performProb1A(transitions, groupIndices, backwardTransitions, allStates, forallGloballyStatesWithoutLraReward);
    // Now also incorporate cumulative and total reward objectives
    auto statesWithTotalOrCumulativeReward = transitions.getRowGroupFilter(~(zeroTotalRewardChoices & zeroCumulativeRewardChoices), false);
    result.reward0AStates &= storm::utility::graph::performProb0A(backwardTransitions, allStates, statesWithTotalOrCumulativeReward);
    assert(result.reward0AStates.isSubsetOf(result.totalReward0EStates));
}

template<typename SparseModelType>
void SparseMultiObjectiveRewardAnalysis<SparseModelType>::checkRewardFiniteness(
    ReturnType& result, storm::modelchecker::multiobjective::preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType> const& preprocessorResult,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions) {
    result.rewardFinitenessType = RewardFinitenessType::AllFinite;

    auto const& transitions = preprocessorResult.preprocessedModel->getTransitionMatrix();
    std::vector<uint_fast64_t> const& groupIndices = transitions.getRowGroupIndices();

    // Gather choices where infinite reward is collected if they are taken infinitely often.
    // Distinguish between attracting choices, where (for some objective) optimal policies might take that choice infinitely often
    // and draining choices, where optimal policies should never take that choice infinitely often.
    storm::storage::BitVector attractingInfRewardChoices(preprocessorResult.preprocessedModel->getNumberOfChoices(), true);
    storm::storage::BitVector drainingInfRewardChoices(preprocessorResult.preprocessedModel->getNumberOfChoices(), true);
    for (auto objIndex : preprocessorResult.maybeInfiniteRewardObjectives) {
        STORM_LOG_ASSERT(preprocessorResult.objectives[objIndex].formula->isRewardOperatorFormula(),
                         "Objective needs to be checked for finite reward but has no reward operator.");
        auto const& rewModel = preprocessorResult.preprocessedModel->getRewardModel(
            preprocessorResult.objectives[objIndex].formula->asRewardOperatorFormula().getRewardModelName());
        auto irrelevantChoices = rewModel.getChoicesWithZeroReward(transitions);
        // For (upper) reward bounded cumulative reward formulas, we do not need to consider the choices where boundReward is collected.
        if (preprocessorResult.objectives[objIndex].formula->getSubformula().isCumulativeRewardFormula()) {
            auto const& timeBoundReference =
                preprocessorResult.objectives[objIndex].formula->getSubformula().asCumulativeRewardFormula().getTimeBoundReference();
            // Only reward bounded formulas need a finiteness check
            assert(timeBoundReference.isRewardBound());
            auto const& rewModelOfBound = preprocessorResult.preprocessedModel->getRewardModel(timeBoundReference.getRewardName());
            irrelevantChoices |= ~rewModelOfBound.getChoicesWithZeroReward(transitions);
        }

        bool const maximizing = storm::solver::maximize(preprocessorResult.objectives[objIndex].formula->getOptimalityType());
        bool const negativeRewards = rewModel.hasNegativeRewards();
        bool const positiveRewards = rewModel.hasPositiveRewards();
        bool const hasMixedSignRewards = negativeRewards && positiveRewards;

        if (hasMixedSignRewards || (maximizing && !negativeRewards) || (!maximizing && !positiveRewards)) {
            attractingInfRewardChoices &= irrelevantChoices;
        } else {
            drainingInfRewardChoices &= irrelevantChoices;
        }
    }
    attractingInfRewardChoices.complement();
    drainingInfRewardChoices.complement();

    // Check reward finiteness under all schedulers
    storm::storage::BitVector allStates(preprocessorResult.preprocessedModel->getNumberOfStates(), true);
    if (storm::utility::graph::checkIfECWithChoiceExists(transitions, backwardTransitions, allStates, attractingInfRewardChoices | drainingInfRewardChoices)) {
        // Check whether there is a scheduler yielding infinite reward for a maximizing objective
        if (storm::utility::graph::checkIfECWithChoiceExists(transitions, backwardTransitions, allStates, attractingInfRewardChoices)) {
            result.rewardFinitenessType = RewardFinitenessType::Infinite;
        } else {
            // Check whether there is a scheduler under which all rewards are finite.
            result.totalRewardLessInfinityEStates =
                storm::utility::graph::performProb1E(transitions, groupIndices, backwardTransitions, allStates, result.totalReward0EStates);
            if ((result.totalRewardLessInfinityEStates.get() & preprocessorResult.preprocessedModel->getInitialStates()).empty()) {
                // There is no scheduler that induces finite reward for the initial state
                result.rewardFinitenessType = RewardFinitenessType::Infinite;
            } else {
                result.rewardFinitenessType = RewardFinitenessType::ExistsParetoFinite;
            }
        }
    } else {
        result.totalRewardLessInfinityEStates = allStates;
    }
}

template<typename SparseModelType>
void SparseMultiObjectiveRewardAnalysis<SparseModelType>::computeUpperResultBound(SparseModelType const& model,
                                                                                  storm::modelchecker::multiobjective::Objective<ValueType>& objective,
                                                                                  storm::storage::SparseMatrix<ValueType> const& backwardTransitions) {
    STORM_LOG_INFO_COND(!objective.upperResultBound.is_initialized(),
                        "Tried to find an upper result bound for an objective, but a result bound is already there.");

    if (model.isOfType(storm::models::ModelType::Mdp)) {
        auto const& transitions = model.getTransitionMatrix();

        if (objective.formula->isRewardOperatorFormula()) {
            auto const& rewModel = model.getRewardModel(objective.formula->asRewardOperatorFormula().getRewardModelName());
            auto actionRewards = rewModel.getTotalRewardVector(transitions);

            if (objective.formula->getSubformula().isTotalRewardFormula() || objective.formula->getSubformula().isCumulativeRewardFormula()) {
                // We have to eliminate ECs here to treat zero-reward ECs

                storm::storage::BitVector allStates(model.getNumberOfStates(), true);

                // Get the set of states from which no reward is reachable
                auto nonZeroRewardStates = rewModel.getStatesWithZeroReward(transitions);
                nonZeroRewardStates.complement();
                auto expRewGreater0EStates = storm::utility::graph::performProbGreater0E(backwardTransitions, allStates, nonZeroRewardStates);

                auto zeroRewardChoices = rewModel.getChoicesWithZeroReward(transitions);

                auto ecElimRes =
                    storm::transformer::EndComponentEliminator<ValueType>::transform(transitions, expRewGreater0EStates, zeroRewardChoices, ~allStates);

                allStates.resize(ecElimRes.matrix.getRowGroupCount());
                storm::storage::BitVector outStates(allStates.size(), false);
                std::vector<ValueType> rew0StateProbs;
                rew0StateProbs.reserve(ecElimRes.matrix.getRowCount());
                for (uint64_t state = 0; state < allStates.size(); ++state) {
                    for (uint64_t choice = ecElimRes.matrix.getRowGroupIndices()[state]; choice < ecElimRes.matrix.getRowGroupIndices()[state + 1]; ++choice) {
                        // Check whether the choice lead to a state with expRew 0 in the original model
                        bool isOutChoice = false;
                        uint64_t originalModelChoice = ecElimRes.newToOldRowMapping[choice];
                        for (auto const& entry : transitions.getRow(originalModelChoice)) {
                            if (!expRewGreater0EStates.get(entry.getColumn())) {
                                isOutChoice = true;
                                outStates.set(state, true);
                                rew0StateProbs.push_back(storm::utility::one<ValueType>() - ecElimRes.matrix.getRowSum(choice));
                                assert(!storm::utility::isZero(rew0StateProbs.back()));
                                break;
                            }
                        }
                        if (!isOutChoice) {
                            rew0StateProbs.push_back(storm::utility::zero<ValueType>());
                        }
                    }
                }

                // An upper reward bound can only be computed if it is below infinity
                if (storm::utility::graph::performProb1A(ecElimRes.matrix, ecElimRes.matrix.getRowGroupIndices(), ecElimRes.matrix.transpose(true), allStates,
                                                         outStates)
                        .full()) {
                    std::vector<ValueType> rewards;
                    rewards.reserve(ecElimRes.matrix.getRowCount());
                    for (auto row : ecElimRes.newToOldRowMapping) {
                        rewards.push_back(actionRewards[row]);
                    }

                    auto bounds = storm::modelchecker::helper::BaierUpperRewardBoundsComputer<ValueType>(ecElimRes.matrix, rew0StateProbs)
                                      .computeTotalRewardBounds(rewards);
                    objective.lowerResultBound = std::max(objective.lowerResultBound.value_or(bounds.lower), bounds.lower);
                    objective.upperResultBound = std::min(objective.upperResultBound.value_or(bounds.upper), bounds.upper);
                }
            }
        }

        if (objective.upperResultBound) {
            STORM_LOG_INFO("Computed upper result bound " << objective.upperResultBound.get() << " for objective " << *objective.formula << ".");
        } else {
            STORM_LOG_WARN("Could not compute upper result bound for objective " << *objective.formula);
        }
    }
}

template class SparseMultiObjectiveRewardAnalysis<storm::models::sparse::Mdp<double>>;
template class SparseMultiObjectiveRewardAnalysis<storm::models::sparse::MarkovAutomaton<double>>;

template class SparseMultiObjectiveRewardAnalysis<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class SparseMultiObjectiveRewardAnalysis<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;
}  // namespace preprocessing
}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
