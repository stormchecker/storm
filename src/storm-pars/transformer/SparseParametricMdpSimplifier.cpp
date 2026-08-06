#include "storm-pars/transformer/SparseParametricMdpSimplifier.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/logic/CloneVisitor.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/transformer/EndComponentEliminator.h"
#include "storm/transformer/GoalStateMerger.h"
#include "storm/utility/graph.h"
#include "storm/utility/vector.h"

namespace storm {
namespace transformer {

template<typename SparseModelType>
SparseParametricMdpSimplifier<SparseModelType>::SparseParametricMdpSimplifier(SparseModelType const& model)
    : SparseParametricModelSimplifier<SparseModelType>(model) {
    // intentionally left empty
}

template<typename SparseModelType>
bool SparseParametricMdpSimplifier<SparseModelType>::simplifyForUntilProbabilities(storm::logic::ProbabilityOperatorFormula const& formula) {
    bool minimizing =
        formula.hasOptimalityType() ? storm::solver::minimize(formula.getOptimalityType()) : storm::logic::isLowerBound(formula.getBound().comparisonType);

    // Get the prob0, prob1 and the maybeStates
    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> propositionalChecker(this->originalModel);
    if (!propositionalChecker.canHandle(formula.getSubformula().asUntilFormula().getLeftSubformula()) ||
        !propositionalChecker.canHandle(formula.getSubformula().asUntilFormula().getRightSubformula())) {
        STORM_LOG_DEBUG("Can not simplify when Until-formula has non-propositional subformula(s). Formula: " << formula);
        return false;
    }
    storm::storage::BitVector phiStates = std::move(propositionalChecker.check(formula.getSubformula().asUntilFormula().getLeftSubformula())
                                                        ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                        .getTruthValuesVector());
    storm::storage::BitVector psiStates = std::move(propositionalChecker.check(formula.getSubformula().asUntilFormula().getRightSubformula())
                                                        ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                        .getTruthValuesVector());
    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 =
        minimizing ? storm::utility::graph::performProb01Min(this->originalModel, phiStates, psiStates)
                   : storm::utility::graph::performProb01Max(this->originalModel, phiStates, psiStates);

    // Only consider the maybestates that are reachable from one initial state without hopping over a target (i.e., prob1) state
    storm::storage::BitVector reachableGreater0States = storm::utility::graph::getReachableStates(
        this->originalModel.getTransitionMatrix(), this->originalModel.getInitialStates() & ~statesWithProbability01.first, ~statesWithProbability01.first,
        statesWithProbability01.second);
    storm::storage::BitVector maybeStates = reachableGreater0States & ~statesWithProbability01.second;

    // obtain the resulting subsystem
    storm::transformer::GoalStateMerger<SparseModelType> goalStateMerger(this->originalModel);
    typename storm::transformer::GoalStateMerger<SparseModelType>::ReturnType mergerResult =
        goalStateMerger.mergeTargetAndSinkStates(maybeStates, statesWithProbability01.second, statesWithProbability01.first);
    this->simplifiedModel = mergerResult.model;
    statesWithProbability01.first = storm::storage::BitVector(this->simplifiedModel->getNumberOfStates(), false);
    if (mergerResult.sinkState) {
        statesWithProbability01.first.set(mergerResult.sinkState.get(), true);
    }
    std::string sinkLabel = "sink";
    while (this->simplifiedModel->hasLabel(sinkLabel)) {
        sinkLabel = "_" + sinkLabel;
    }
    this->simplifiedModel->getStateLabeling().addLabel(sinkLabel, std::move(statesWithProbability01.first));
    statesWithProbability01.second = storm::storage::BitVector(this->simplifiedModel->getNumberOfStates(), false);
    if (mergerResult.targetState) {
        statesWithProbability01.second.set(mergerResult.targetState.get(), true);
    }
    std::string targetLabel = "target";
    while (this->simplifiedModel->hasLabel(targetLabel)) {
        targetLabel = "_" + targetLabel;
    }
    this->simplifiedModel->getStateLabeling().addLabel(targetLabel, std::move(statesWithProbability01.second));

    // obtain the simplified formula for the simplified model
    auto labelFormula = std::make_shared<storm::logic::AtomicLabelFormula const>(targetLabel);
    auto eventuallyFormula = std::make_shared<storm::logic::EventuallyFormula const>(labelFormula, storm::logic::FormulaContext::Probability);
    this->simplifiedFormula = std::make_shared<storm::logic::ProbabilityOperatorFormula const>(eventuallyFormula, formula.getOperatorInformation());

    // Eliminate all states for which all outgoing transitions are constant
    storm::storage::BitVector considerForElimination = ~this->simplifiedModel->getInitialStates();
    if (mergerResult.targetState) {
        considerForElimination.set(*mergerResult.targetState, false);
    }
    if (mergerResult.sinkState) {
        considerForElimination.set(*mergerResult.sinkState, false);
    }
    this->simplifiedModel = this->eliminateConstantDeterministicStates(*this->simplifiedModel, considerForElimination);

    // Eliminate the end components that do not contain a target or a sink state (only required if the probability is maximized)
    if (!minimizing) {
        this->simplifiedModel = SparseParametricMdpSimplifier<SparseModelType>::eliminateNeutralEndComponents(
            *this->simplifiedModel, this->simplifiedModel->getStates(targetLabel) | this->simplifiedModel->getStates(sinkLabel));
    }

    return true;
}

template<typename SparseModelType>
bool SparseParametricMdpSimplifier<SparseModelType>::simplifyForBoundedUntilProbabilities(storm::logic::ProbabilityOperatorFormula const& formula) {
    STORM_LOG_THROW(!formula.getSubformula().asBoundedUntilFormula().hasLowerBound(), storm::exceptions::NotSupportedException,
                    "Lower step bounds are not supported.");
    STORM_LOG_THROW(formula.getSubformula().asBoundedUntilFormula().hasUpperBound(), storm::exceptions::UnexpectedException,
                    "Expected a bounded until formula with an upper bound.");
    STORM_LOG_THROW(formula.getSubformula().asBoundedUntilFormula().getUpperBound().getBaseExpression().isIntegerLiteralExpression(),
                    storm::exceptions::UnexpectedException, "Expected a bounded until formula with integral bounds.");

    bool minimizing =
        formula.hasOptimalityType() ? storm::solver::minimize(formula.getOptimalityType()) : storm::logic::isLowerBound(formula.getBound().comparisonType);
    uint_fast64_t upperStepBound = formula.getSubformula().asBoundedUntilFormula().getUpperBound().evaluateAsInt();
    if (formula.getSubformula().asBoundedUntilFormula().isUpperBoundStrict()) {
        STORM_LOG_THROW(upperStepBound > 0, storm::exceptions::UnexpectedException, "Expected a strict upper bound that is greater than zero.");
        --upperStepBound;
    }

    // Get the prob0, target, and the maybeStates
    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> propositionalChecker(this->originalModel);
    if (!propositionalChecker.canHandle(formula.getSubformula().asBoundedUntilFormula().getLeftSubformula()) ||
        !propositionalChecker.canHandle(formula.getSubformula().asBoundedUntilFormula().getRightSubformula())) {
        STORM_LOG_DEBUG("Can not simplify when Until-formula has non-propositional subformula(s). Formula: " << formula);
        return false;
    }
    storm::storage::BitVector phiStates = std::move(propositionalChecker.check(formula.getSubformula().asBoundedUntilFormula().getLeftSubformula())
                                                        ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                        .getTruthValuesVector());
    storm::storage::BitVector psiStates = std::move(propositionalChecker.check(formula.getSubformula().asBoundedUntilFormula().getRightSubformula())
                                                        ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                        .getTruthValuesVector());
    storm::storage::BitVector probGreater0States =
        minimizing ? storm::utility::graph::performProbGreater0A(this->originalModel.getTransitionMatrix(),
                                                                 this->originalModel.getTransitionMatrix().getRowGroupIndices(),
                                                                 this->originalModel.getBackwardTransitions(), phiStates, psiStates, true, upperStepBound)
                   : storm::utility::graph::performProbGreater0E(this->originalModel.getBackwardTransitions(), phiStates, psiStates, true, upperStepBound);

    // Only consider the maybestates that are reachable from one initial probGreater0 state within the given amount of steps and without hopping over a target
    // state
    storm::storage::BitVector reachableGreater0States =
        storm::utility::graph::getReachableStates(this->originalModel.getTransitionMatrix(), this->originalModel.getInitialStates() & probGreater0States,
                                                  probGreater0States, psiStates, true, upperStepBound);
    storm::storage::BitVector maybeStates = reachableGreater0States & ~psiStates;
    storm::storage::BitVector prob0States = ~reachableGreater0States & ~psiStates;

    // obtain the resulting subsystem
    storm::transformer::GoalStateMerger<SparseModelType> goalStateMerger(this->originalModel);
    typename storm::transformer::GoalStateMerger<SparseModelType>::ReturnType mergerResult =
        goalStateMerger.mergeTargetAndSinkStates(maybeStates, psiStates, prob0States);
    this->simplifiedModel = mergerResult.model;
    psiStates = storm::storage::BitVector(this->simplifiedModel->getNumberOfStates(), false);
    if (mergerResult.targetState) {
        psiStates.set(mergerResult.targetState.get(), true);
    }
    std::string targetLabel = "target";
    while (this->simplifiedModel->hasLabel(targetLabel)) {
        targetLabel = "_" + targetLabel;
    }
    this->simplifiedModel->getStateLabeling().addLabel(targetLabel, std::move(psiStates));

    // obtain the simplified formula for the simplified model
    auto labelFormula = std::make_shared<storm::logic::AtomicLabelFormula const>(targetLabel);
    auto boundedUntilFormula =
        std::make_shared<storm::logic::BoundedUntilFormula const>(storm::logic::Formula::getTrueFormula(), labelFormula, boost::none,
                                                                  storm::logic::TimeBound(formula.getSubformula().asBoundedUntilFormula().isUpperBoundStrict(),
                                                                                          formula.getSubformula().asBoundedUntilFormula().getUpperBound()),
                                                                  storm::logic::TimeBoundReference(storm::logic::TimeBoundType::Steps));
    this->simplifiedFormula = std::make_shared<storm::logic::ProbabilityOperatorFormula const>(boundedUntilFormula, formula.getOperatorInformation());

    return true;
}

template<typename SparseModelType>
bool SparseParametricMdpSimplifier<SparseModelType>::simplifyForReachabilityRewards(storm::logic::RewardOperatorFormula const& formula) {
    typename SparseModelType::RewardModelType const& originalRewardModel =
        formula.hasRewardModelName() ? this->originalModel.getRewardModel(formula.getRewardModelName()) : this->originalModel.getUniqueRewardModel();

    bool minimizing =
        formula.hasOptimalityType() ? storm::solver::minimize(formula.getOptimalityType()) : storm::logic::isLowerBound(formula.getBound().comparisonType);

    // Get the prob1 and the maybeStates
    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> propositionalChecker(this->originalModel);
    if (!propositionalChecker.canHandle(formula.getSubformula().asEventuallyFormula().getSubformula())) {
        STORM_LOG_DEBUG("Can not simplify when reachability reward formula has non-propositional subformula(s). Formula: " << formula);
        return false;
    }
    storm::storage::BitVector targetStates = std::move(propositionalChecker.check(formula.getSubformula().asEventuallyFormula().getSubformula())
                                                           ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                           .getTruthValuesVector());
    // The set of target states can be extended by the states that reach target with probability 1 without collecting any reward
    // TODO for the call of Prob1E we could restrict the analysis to actions with zero reward instead of states with zero reward
    targetStates =
        minimizing ? storm::utility::graph::performProb1E(this->originalModel, this->originalModel.getBackwardTransitions(),
                                                          originalRewardModel.getStatesWithZeroReward(this->originalModel.getTransitionMatrix()), targetStates)
                   : storm::utility::graph::performProb1A(this->originalModel, this->originalModel.getBackwardTransitions(),
                                                          originalRewardModel.getStatesWithZeroReward(this->originalModel.getTransitionMatrix()), targetStates);
    storm::storage::BitVector statesWithProb1 =
        minimizing ? storm::utility::graph::performProb1E(this->originalModel, this->originalModel.getBackwardTransitions(),
                                                          storm::storage::BitVector(this->originalModel.getNumberOfStates(), true), targetStates)
                   : storm::utility::graph::performProb1A(this->originalModel, this->originalModel.getBackwardTransitions(),
                                                          storm::storage::BitVector(this->originalModel.getNumberOfStates(), true), targetStates);
    storm::storage::BitVector infinityStates = ~statesWithProb1;
    // Only consider the states that are reachable from an initial state without hopping over a target state
    storm::storage::BitVector reachableStates = storm::utility::graph::getReachableStates(
        this->originalModel.getTransitionMatrix(), this->originalModel.getInitialStates() & statesWithProb1, statesWithProb1, targetStates);
    storm::storage::BitVector maybeStates = reachableStates & ~targetStates;

    // obtain the resulting subsystem
    std::vector<std::string> rewardModelNameAsVector(
        1, formula.hasRewardModelName() ? formula.getRewardModelName() : this->originalModel.getRewardModels().begin()->first);
    storm::transformer::GoalStateMerger<SparseModelType> goalStateMerger(this->originalModel);
    typename storm::transformer::GoalStateMerger<SparseModelType>::ReturnType mergerResult =
        goalStateMerger.mergeTargetAndSinkStates(maybeStates, targetStates, infinityStates, rewardModelNameAsVector);
    this->simplifiedModel = mergerResult.model;
    infinityStates = storm::storage::BitVector(this->simplifiedModel->getNumberOfStates(), false);
    if (mergerResult.sinkState) {
        infinityStates.set(mergerResult.sinkState.get(), true);
    }
    std::string sinkLabel = "sink";
    while (this->simplifiedModel->hasLabel(sinkLabel)) {
        sinkLabel = "_" + sinkLabel;
    }
    this->simplifiedModel->getStateLabeling().addLabel(sinkLabel, std::move(infinityStates));

    targetStates = storm::storage::BitVector(this->simplifiedModel->getNumberOfStates(), false);
    if (mergerResult.targetState) {
        targetStates.set(mergerResult.targetState.get(), true);
    }
    std::string targetLabel = "target";
    while (this->simplifiedModel->hasLabel(targetLabel)) {
        targetLabel = "_" + targetLabel;
    }
    this->simplifiedModel->getStateLabeling().addLabel(targetLabel, std::move(targetStates));

    // obtain the simplified formula for the simplified model
    auto labelFormula = std::make_shared<storm::logic::AtomicLabelFormula const>(targetLabel);
    auto eventuallyFormula = std::make_shared<storm::logic::EventuallyFormula const>(labelFormula, storm::logic::FormulaContext::Reward);
    this->simplifiedFormula =
        std::make_shared<storm::logic::RewardOperatorFormula const>(eventuallyFormula, rewardModelNameAsVector.front(), formula.getOperatorInformation());

    // Eliminate all states for which all outgoing transitions are constant
    storm::storage::BitVector considerForElimination = ~this->simplifiedModel->getInitialStates();
    if (mergerResult.targetState) {
        considerForElimination.set(*mergerResult.targetState, false);
    }
    if (mergerResult.sinkState) {
        considerForElimination.set(*mergerResult.sinkState, false);
    }
    this->simplifiedModel = this->eliminateConstantDeterministicStates(*this->simplifiedModel, considerForElimination, rewardModelNameAsVector.front());

    // Eliminate the end components in which no reward is collected (only required if rewards are minimized)
    if (minimizing) {
        this->simplifiedModel = SparseParametricMdpSimplifier<SparseModelType>::eliminateNeutralEndComponents(
            *this->simplifiedModel, this->simplifiedModel->getStates(targetLabel) | this->simplifiedModel->getStates(sinkLabel),
            rewardModelNameAsVector.front());
    }
    return true;
}

template<typename SparseModelType>
bool SparseParametricMdpSimplifier<SparseModelType>::simplifyForCumulativeRewards(storm::logic::RewardOperatorFormula const& formula) {
    STORM_LOG_THROW(formula.getSubformula().asCumulativeRewardFormula().getBound().getBaseExpression().isIntegerLiteralExpression(),
                    storm::exceptions::UnexpectedException, "Expected a cumulative reward formula with integral bound.");

    typename SparseModelType::RewardModelType const& originalRewardModel =
        formula.hasRewardModelName() ? this->originalModel.getRewardModel(formula.getRewardModelName()) : this->originalModel.getUniqueRewardModel();

    bool minimizing =
        formula.hasOptimalityType() ? storm::solver::minimize(formula.getOptimalityType()) : storm::logic::isLowerBound(formula.getBound().comparisonType);
    uint_fast64_t stepBound = formula.getSubformula().asCumulativeRewardFormula().getBound().evaluateAsInt();
    if (formula.getSubformula().asCumulativeRewardFormula().isBoundStrict()) {
        STORM_LOG_THROW(stepBound > 0, storm::exceptions::UnexpectedException, "Expected a strict upper bound that is greater than zero.");
        --stepBound;
    }

    // Get the states with non-zero reward
    storm::storage::BitVector maybeStates =
        minimizing ? storm::utility::graph::performProbGreater0A(
                         this->originalModel.getTransitionMatrix(), this->originalModel.getTransitionMatrix().getRowGroupIndices(),
                         this->originalModel.getBackwardTransitions(), storm::storage::BitVector(this->originalModel.getNumberOfStates(), true),
                         ~originalRewardModel.getStatesWithZeroReward(this->originalModel.getTransitionMatrix()), true, stepBound)
                   : storm::utility::graph::performProbGreater0E(
                         this->originalModel.getBackwardTransitions(), storm::storage::BitVector(this->originalModel.getNumberOfStates(), true),
                         ~originalRewardModel.getStatesWithZeroReward(this->originalModel.getTransitionMatrix()), true, stepBound);
    storm::storage::BitVector zeroRewardStates = ~maybeStates;
    storm::storage::BitVector noStates(this->originalModel.getNumberOfStates(), false);

    // obtain the resulting subsystem
    std::vector<std::string> rewardModelNameAsVector(
        1, formula.hasRewardModelName() ? formula.getRewardModelName() : this->originalModel.getRewardModels().begin()->first);
    storm::transformer::GoalStateMerger<SparseModelType> goalStateMerger(this->originalModel);
    typename storm::transformer::GoalStateMerger<SparseModelType>::ReturnType mergerResult =
        goalStateMerger.mergeTargetAndSinkStates(maybeStates, noStates, zeroRewardStates, rewardModelNameAsVector);
    this->simplifiedModel = mergerResult.model;

    // obtain the simplified formula for the simplified model
    this->simplifiedFormula = storm::logic::CloneVisitor().clone(formula);

    return true;
}

template<typename SparseModelType>
std::shared_ptr<SparseModelType> SparseParametricMdpSimplifier<SparseModelType>::eliminateNeutralEndComponents(
    SparseModelType const& model, storm::storage::BitVector const& ignoredStates, boost::optional<std::string> const& rewardModelName) {
    // Get the actions that can be part of an EC
    storm::storage::BitVector possibleECActions(model.getNumberOfChoices(), true);
    for (auto state : ignoredStates) {
        for (uint_fast64_t actionIndex = model.getTransitionMatrix().getRowGroupIndices()[state];
             actionIndex < model.getTransitionMatrix().getRowGroupIndices()[state + 1]; ++actionIndex) {
            possibleECActions.set(actionIndex, false);
        }
    }

    // Get the action-based reward values and unselect non-zero reward actions
    std::vector<typename SparseModelType::ValueType> actionRewards;
    if (rewardModelName) {
        actionRewards = model.getRewardModel(*rewardModelName).getTotalRewardVector(model.getTransitionMatrix());
        uint_fast64_t actionIndex = 0;
        for (auto const& actionReward : actionRewards) {
            if (!storm::utility::isZero(actionReward)) {
                possibleECActions.set(actionIndex, false);
            }
            ++actionIndex;
        }
    }

    // Invoke EC Elimination
    auto ecEliminatorResult = storm::transformer::EndComponentEliminator<typename SparseModelType::ValueType>::transform(
        model.getTransitionMatrix(), storm::storage::BitVector(model.getNumberOfStates(), true), possibleECActions,
        storm::storage::BitVector(model.getNumberOfStates(), false));

    // obtain the reward model for the resulting system
    std::unordered_map<std::string, typename SparseModelType::RewardModelType> rewardModels;
    if (rewardModelName) {
        std::vector<typename SparseModelType::ValueType> newActionRewards(ecEliminatorResult.matrix.getRowCount());
        storm::utility::vector::selectVectorValues(newActionRewards, ecEliminatorResult.newToOldRowMapping, actionRewards);
        rewardModels.insert(std::make_pair(*rewardModelName, typename SparseModelType::RewardModelType(std::nullopt, std::move(actionRewards))));
    }

    // the new labeling
    storm::models::sparse::StateLabeling labeling(ecEliminatorResult.matrix.getRowGroupCount());
    for (auto const& label : model.getStateLabeling().getLabels()) {
        auto const& origStatesWithLabel = model.getStates(label);
        storm::storage::BitVector newStatesWithLabel(ecEliminatorResult.matrix.getRowGroupCount(), false);
        for (auto const& origState : origStatesWithLabel) {
            newStatesWithLabel.set(ecEliminatorResult.oldToNewStateMapping[origState], true);
        }
        labeling.addLabel(label, std::move(newStatesWithLabel));
    }

    return std::make_shared<SparseModelType>(std::move(ecEliminatorResult.matrix), std::move(labeling), std::move(rewardModels));
}

template class SparseParametricMdpSimplifier<storm::models::sparse::Mdp<storm::RationalFunction>>;
}  // namespace transformer
}  // namespace storm
