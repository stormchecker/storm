#include "storm/transformer/GoalStateMerger.h"

#include <limits>
#include <memory>
#include <type_traits>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/DeterministicModel.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/NondeterministicModel.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/builder.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace transformer {

namespace {
// Whether the given operator formula asks for the minimizing (as opposed to maximizing) value.
bool isMinimizing(storm::logic::OperatorFormula const& formula) {
    return formula.hasOptimalityType() ? storm::solver::minimize(formula.getOptimalityType()) : storm::logic::isLowerBound(formula.getBound().comparisonType);
}

// Checks the given (propositional) subformula and returns the truth values vector, or std::nullopt if the subformula is not propositional.
template<typename SparseModelType>
std::optional<storm::storage::BitVector> const checkPropositional(storm::modelchecker::SparsePropositionalModelChecker<SparseModelType>& checker,
                                                                  storm::logic::Formula const& subformula) {
    if (!checker.canHandle(subformula)) {
        return std::nullopt;
    }
    return std::move(checker.check(subformula)->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>().getTruthValuesVector());
}

std::optional<uint64_t> representative(storm::storage::BitVector const& b) {
    if (auto it = b.begin(); it != b.end()) {
        return *it;
    }
    return std::nullopt;
}

}  // namespace

template<typename ValueType>
GoalStateMerger<ValueType>::GoalStateMerger(storm::models::sparse::Model<ValueType> const& model) : originalModel(model) {
    STORM_LOG_THROW(model.isOfType(storm::models::ModelType::Dtmc) || model.isOfType(storm::models::ModelType::Ctmc) ||
                        model.isOfType(storm::models::ModelType::Mdp) || model.isOfType(storm::models::ModelType::MarkovAutomaton),
                    storm::exceptions::NotSupportedException, "GoalStateMerger does not support models of type " << model.getType() << ".");
}

template<typename ValueType>
typename GoalStateMerger<ValueType>::ReturnType GoalStateMerger<ValueType>::mergeTargetAndSinkStates(
    storm::storage::BitVector const& maybeStates, storm::storage::BitVector const& targetStates, storm::storage::BitVector const& sinkStates,
    std::vector<std::string> const& selectedRewardModels, std::optional<storm::storage::BitVector> const& choiceFilter,
    std::optional<uint64_t> const representativeTargetState, std::optional<uint64_t> const representativeSinkState) const {
    STORM_LOG_THROW(maybeStates.isDisjointFrom(targetStates) && targetStates.isDisjointFrom(sinkStates) && sinkStates.isDisjointFrom(maybeStates),
                    storm::exceptions::InvalidArgumentException,
                    "Maybestates, targetstates, and sinkstates are assumed to be disjoint when creating the submodel. However, this is not the case.");

    auto result = initialize(maybeStates, targetStates, sinkStates, choiceFilter);

    // Initialize model components
    auto transitionMatrix = buildTransitionMatrix(maybeStates, result.first, result.second);
    auto labeling = buildStateLabeling(maybeStates, targetStates, sinkStates, result.first, representativeTargetState, representativeSinkState);
    auto rewardModels = buildRewardModels(maybeStates, result.first, selectedRewardModels);
    storm::storage::sparse::ModelComponents<ValueType> modelComponents(std::move(transitionMatrix), std::move(labeling), std::move(rewardModels));

    uint64_t const newStateCount = modelComponents.transitionMatrix.getRowGroupCount();
    uint64_t const newChoiceCount = modelComponents.transitionMatrix.getRowCount();

    // Obtain model type-specific data
    using enum storm::models::ModelType;
    if (originalModel.isOfType(Ctmc)) {
        // The transition matrix entries are rates (as opposed to probabilities that would need to be scaled by the exit rate).
        modelComponents.rateTransitions = true;
    } else if (originalModel.isOfType(MarkovAutomaton)) {
        auto const& ma = *originalModel.template as<storm::models::sparse::MarkovAutomaton<ValueType>>();
        modelComponents.markovianStates = ma.getMarkovianStates() % maybeStates;
        modelComponents.markovianStates->resize(newStateCount, true);
        modelComponents.exitRates = storm::utility::vector::filterVector(ma.getExitRates(), maybeStates);
        modelComponents.exitRates->resize(newStateCount, storm::utility::one<ValueType>());
    } else {
        STORM_LOG_ASSERT(originalModel.isOfType(Dtmc) || originalModel.isOfType(Mdp), "Unhandled model type: " << originalModel.getType());
    }

    // Reapply state valuations, choice labeling, choice origins if available
    if (originalModel.hasStateValuations()) {
        std::vector<uint64_t> newToOldStateIndices = maybeStates.getNumberOfSetBitsBeforeIndices();
        if (result.first.targetState.has_value()) {
            STORM_LOG_ASSERT(result.first.targetState.value() == newToOldStateIndices.size(), "unexpected position of target state.");
            newToOldStateIndices.push_back(representativeTargetState.value_or(*targetStates.begin()));
        }
        if (result.first.sinkState.has_value()) {
            STORM_LOG_ASSERT(result.first.sinkState.value() == newToOldStateIndices.size(), "unexpected position of sink state.");
            newToOldStateIndices.push_back(representativeSinkState.value_or(*sinkStates.begin()));
        }
        modelComponents.stateValuations = originalModel.getStateValuations().selectEntities(newToOldStateIndices);
    }
    if (originalModel.hasChoiceLabeling()) {
        modelComponents.choiceLabeling.emplace(newChoiceCount);
        for (auto const& label : originalModel.getChoiceLabeling().getLabels()) {
            storm::storage::BitVector choiceLabes = originalModel.getChoiceLabeling().getChoices(label) % result.first.keptChoices;
            choiceLabes.resize(newChoiceCount, false);  // no label for the choice at target/sink states
            modelComponents.choiceLabeling->addLabel(label, std::move(choiceLabes));
        }
    }
    if (originalModel.hasChoiceOrigins()) {
        std::vector<uint64_t> newToOldChoiceIndices = maybeStates.getNumberOfSetBitsBeforeIndices();
        newToOldChoiceIndices.resize(newChoiceCount, std::numeric_limits<uint64_t>::max());  // target/sink choices will not have an origin
        modelComponents.choiceOrigins = originalModel.getChoiceOrigins()->selectChoices(newToOldChoiceIndices);
    }

    // Finalize result
    result.first.model = storm::utility::builder::buildModelFromComponents(originalModel.getType(), std::move(modelComponents));
    return result.first;
}

template<typename ValueType>
std::optional<typename GoalStateMerger<ValueType>::ReturnType> GoalStateMerger<ValueType>::mergeForFormula(storm::logic::Formula const& formula,
                                                                                                           bool const dropUnreachableFromInit) const {
    if (formula.isProbabilityOperatorFormula()) {
        auto const& probabilityOperatorFormula = formula.asProbabilityOperatorFormula();
        if (probabilityOperatorFormula.getSubformula().isUntilFormula()) {
            return mergeForUntilProbabilities(probabilityOperatorFormula, dropUnreachableFromInit);
        } else if (probabilityOperatorFormula.getSubformula().isEventuallyFormula()) {
            return mergeForUntilProbabilities(probabilityOperatorFormula, dropUnreachableFromInit);
        } else if (probabilityOperatorFormula.getSubformula().isBoundedUntilFormula()) {
            return mergeForBoundedUntilProbabilities(probabilityOperatorFormula, dropUnreachableFromInit);
        }
    } else if (formula.isRewardOperatorFormula()) {
        auto const& rewardOperatorFormula = formula.asRewardOperatorFormula();
        if (rewardOperatorFormula.getSubformula().isReachabilityRewardFormula()) {
            return mergeForReachabilityRewards(rewardOperatorFormula, dropUnreachableFromInit);
        } else if (rewardOperatorFormula.getSubformula().isCumulativeRewardFormula()) {
            return mergeForCumulativeRewards(rewardOperatorFormula);
        }
    }
    return std::nullopt;
}

template<typename ValueType>
std::optional<typename GoalStateMerger<ValueType>::ReturnType> GoalStateMerger<ValueType>::mergeForUntilProbabilities(
    storm::logic::ProbabilityOperatorFormula const& formula, bool const dropUnreachableFromInit) const {
    std::optional<storm::storage::BitVector> phiStates, psiStates;
    if (formula.getSubformula().isUntilFormula()) {
        auto const& untilFormula = formula.getSubformula().asUntilFormula();
        storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<ValueType>> propositionalChecker(originalModel);
        phiStates = checkPropositional(propositionalChecker, untilFormula.getLeftSubformula());
        psiStates = checkPropositional(propositionalChecker, untilFormula.getRightSubformula());
    } else {
        STORM_LOG_ASSERT(formula.getSubformula().isEventuallyFormula(), "Unexpected formula type: " << formula.getSubformula());
        auto const eventuallyFormula = formula.getSubformula().asEventuallyFormula();
        STORM_LOG_ASSERT(eventuallyFormula.isReachabilityProbabilityFormula(), "Unexpected formula context");
        storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<ValueType>> propositionalChecker(originalModel);
        phiStates.emplace(originalModel.getNumberOfStates(), true);
        psiStates = checkPropositional(propositionalChecker, eventuallyFormula.getSubformula());
    }
    if (!phiStates || !psiStates) {
        return std::nullopt;
    }

    auto const& transitions = originalModel.getTransitionMatrix();
    storm::storage::SparseMatrix<ValueType> const backwardTransitions = originalModel.getBackwardTransitions();
    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01;
    if (originalModel.isNondeterministicModel()) {
        statesWithProbability01 =
            isMinimizing(formula)
                ? storm::utility::graph::performProb01Min(transitions, transitions.getRowGroupIndices(), backwardTransitions, *phiStates, *psiStates)
                : storm::utility::graph::performProb01Max(transitions, transitions.getRowGroupIndices(), backwardTransitions, *phiStates, *psiStates);
    } else {
        statesWithProbability01 = storm::utility::graph::performProb01(backwardTransitions, *phiStates, *psiStates);
    }

    storm::storage::BitVector maybeStates;
    if (dropUnreachableFromInit) {
        // Only consider the maybestates that are reachable from one initial state without hopping over a target (i.e., prob1) state
        storm::storage::BitVector reachableGreater0States = storm::utility::graph::getReachableStates(
            transitions, originalModel.getInitialStates() & ~statesWithProbability01.first, ~statesWithProbability01.first, statesWithProbability01.second);
        maybeStates = reachableGreater0States & ~statesWithProbability01.second;
    } else {
        // Equivalent to the above with all states considered initial: every non-prob0, non-prob1 state is a maybestate.
        maybeStates = ~(statesWithProbability01.first | statesWithProbability01.second);
    }

    // Ensure that we pick an actual psiState as representative for the target and an actual sink (prob0) state as representative for the sink. Note that a
    // phi-violating state is not necessarily a sink state, as it might coincide with a psi (target) state, so we can not take a shortcut via phiStates here.
    std::optional<uint64_t> const representativeTarget = representative(*psiStates);
    std::optional<uint64_t> const representativeSink = representative(statesWithProbability01.first);
    return mergeTargetAndSinkStates(maybeStates, statesWithProbability01.second, statesWithProbability01.first, std::vector<std::string>(), std::nullopt,
                                    representativeTarget, representativeSink);
}

template<typename ValueType>
std::optional<typename GoalStateMerger<ValueType>::ReturnType> GoalStateMerger<ValueType>::mergeForBoundedUntilProbabilities(
    storm::logic::ProbabilityOperatorFormula const& formula, bool const dropUnreachableFromInit) const {
    auto const& boundedUntilFormula = formula.getSubformula().asBoundedUntilFormula();
    if (boundedUntilFormula.isMultiDimensional() || boundedUntilFormula.hasLowerBound()) {
        // we can not make the psiStates absorbing or we have to deal with multi-dimensionality. We don't consider these here.
        return std::nullopt;
    }

    std::optional<uint64_t> upperStepBound;
    if (originalModel.isDiscreteTimeModel() && boundedUntilFormula.hasUpperBound() && !boundedUntilFormula.getTimeBoundReference().isRewardBound() &&
        boundedUntilFormula.getUpperBound().getBaseExpression().isIntegerLiteralExpression()) {
        upperStepBound = boundedUntilFormula.getUpperBound().evaluateAsInt();
        if (boundedUntilFormula.isUpperBoundStrict()) {
            if (upperStepBound.value() == 0) {
                // The formula has the form `phi U<0 psi`. Not a case we want to treat here.
                return std::nullopt;
            }
            --upperStepBound.value();
        }
    }

    // Get the prob0, prob1 and the maybeStates
    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<ValueType>> propositionalChecker(originalModel);
    auto phiStates = checkPropositional(propositionalChecker, boundedUntilFormula.getLeftSubformula());
    auto psiStates = checkPropositional(propositionalChecker, boundedUntilFormula.getRightSubformula());
    if (!phiStates || !psiStates) {
        return std::nullopt;
    }

    auto const& transitions = originalModel.getTransitionMatrix();
    storm::storage::SparseMatrix<ValueType> const backwardTransitions = originalModel.getBackwardTransitions();
    storm::storage::BitVector probGreater0States;
    if (originalModel.isNondeterministicModel()) {
        probGreater0States = isMinimizing(formula)
                                 ? storm::utility::graph::performProbGreater0A(transitions, transitions.getRowGroupIndices(), backwardTransitions, *phiStates,
                                                                               *psiStates, upperStepBound.has_value(), upperStepBound.value_or(0))
                                 : storm::utility::graph::performProbGreater0E(backwardTransitions, *phiStates, *psiStates, upperStepBound.has_value(),
                                                                               upperStepBound.value_or(0));
    } else {
        probGreater0States =
            storm::utility::graph::performProbGreater0(backwardTransitions, *phiStates, *psiStates, upperStepBound.has_value(), upperStepBound.value_or(0));
    }

    storm::storage::BitVector maybeStates;
    storm::storage::BitVector prob0States;
    if (dropUnreachableFromInit) {
        // Only consider the maybestates that are reachable from one initial probGreater0 state within the given amount of steps and without hopping over a
        // target state
        storm::storage::BitVector reachableGreater0States =
            storm::utility::graph::getReachableStates(transitions, originalModel.getInitialStates() & probGreater0States, probGreater0States, *psiStates,
                                                      upperStepBound.has_value(), upperStepBound.value_or(0));
        maybeStates = reachableGreater0States & ~*psiStates;
        prob0States = ~reachableGreater0States & ~*psiStates;
    } else {
        // Equivalent to the above with all states considered initial.
        maybeStates = probGreater0States & ~*psiStates;
        prob0States = ~probGreater0States & ~*psiStates;
    }

    // For a reward-bounded formula we must preserve that reward model.
    std::vector<std::string> rewardModels;
    if (boundedUntilFormula.getTimeBoundReference().isRewardBound()) {
        auto rewName = boundedUntilFormula.getTimeBoundReference().getOptionalRewardModelName();
        if (!rewName) {
            rewName = originalModel.getUniqueRewardModelName();
        }
        rewardModels.push_back(rewName.value());
    }
    // Ensure that we pick an actual psiState as representative for the target and an actual sink (prob0) state as representative for the sink. Note that a
    // phi-violating state is not necessarily a sink state, as it might coincide with a psi (target) state, so we can not take a shortcut via phiStates here.
    std::optional<uint64_t> const representativeTarget = representative(*psiStates);
    std::optional<uint64_t> const representativeSink = representative(prob0States);
    return mergeTargetAndSinkStates(maybeStates, psiStates.value(), prob0States, rewardModels, std::nullopt, representativeTarget, representativeSink);
}

template<typename ValueType>
std::optional<typename GoalStateMerger<ValueType>::ReturnType> GoalStateMerger<ValueType>::mergeForReachabilityRewards(
    storm::logic::RewardOperatorFormula const& formula, bool const dropUnreachableFromInit) const {
    auto const& originalRewardModel =
        formula.hasRewardModelName() ? originalModel.getRewardModel(formula.getRewardModelName()) : originalModel.getUniqueRewardModel();

    // Get the prob1 and the maybeStates
    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<ValueType>> propositionalChecker(originalModel);
    auto const& eventuallyFormula = formula.getSubformula().asEventuallyFormula();
    auto checkedTargetStates = checkPropositional(propositionalChecker, eventuallyFormula.getSubformula());
    if (!checkedTargetStates) {
        return std::nullopt;
    }
    storm::storage::BitVector targetStates = std::move(*checkedTargetStates);
    // we extend the target states below, so we must pick our representative here already.
    auto representativeTargetState = representative(targetStates);

    auto const& transitions = originalModel.getTransitionMatrix();
    auto const backwardTransitions = originalModel.getBackwardTransitions();
    storm::storage::BitVector statesWithProb1;
    if (originalModel.isNondeterministicModel()) {
        // The set of target states can be extended by the states that reach target with probability 1 without collecting any reward
        targetStates =
            isMinimizing(formula)
                ? storm::utility::graph::performProb1E(transitions, transitions.getRowGroupIndices(), backwardTransitions,
                                                       originalRewardModel.getStatesWithZeroReward(originalModel.getTransitionMatrix()), targetStates)
                : storm::utility::graph::performProb1A(transitions, transitions.getRowGroupIndices(), backwardTransitions,
                                                       originalRewardModel.getStatesWithZeroReward(originalModel.getTransitionMatrix()), targetStates);
        statesWithProb1 = isMinimizing(formula)
                              ? storm::utility::graph::performProb1E(transitions, transitions.getRowGroupIndices(), backwardTransitions,
                                                                     storm::storage::BitVector(originalModel.getNumberOfStates(), true), targetStates)
                              : storm::utility::graph::performProb1A(transitions, transitions.getRowGroupIndices(), backwardTransitions,
                                                                     storm::storage::BitVector(originalModel.getNumberOfStates(), true), targetStates);
    } else {
        targetStates = storm::utility::graph::performProb1(backwardTransitions,
                                                           originalRewardModel.getStatesWithZeroReward(originalModel.getTransitionMatrix()), targetStates);
        statesWithProb1 =
            storm::utility::graph::performProb1(backwardTransitions, storm::storage::BitVector(originalModel.getNumberOfStates(), true), targetStates);
    }
    storm::storage::BitVector infinityStates = ~statesWithProb1;
    storm::storage::BitVector maybeStates;
    if (dropUnreachableFromInit) {
        // Only consider the states that are reachable from an initial state without hopping over a target state
        storm::storage::BitVector reachableStates =
            storm::utility::graph::getReachableStates(transitions, originalModel.getInitialStates() & statesWithProb1, statesWithProb1, targetStates);
        maybeStates = reachableStates & ~targetStates;
    } else {
        // Equivalent to the above with all states considered initial.
        maybeStates = statesWithProb1 & ~targetStates;
    }

    std::vector<std::string> rewardModelNameAsVector(
        1, formula.hasRewardModelName() ? formula.getRewardModelName() : originalModel.getRewardModels().begin()->first);
    return mergeTargetAndSinkStates(maybeStates, targetStates, infinityStates, rewardModelNameAsVector, std::nullopt, representativeTargetState,
                                    representative(infinityStates));
}

template<typename ValueType>
std::optional<typename GoalStateMerger<ValueType>::ReturnType> GoalStateMerger<ValueType>::mergeForCumulativeRewards(
    storm::logic::RewardOperatorFormula const& formula) const {
    auto const& cumulativeRewardFormula = formula.getSubformula().asCumulativeRewardFormula();
    if (cumulativeRewardFormula.isMultiDimensional()) {
        return std::nullopt;  // we don't handle those more sophisticated formulas here.
    }

    std::optional<uint64_t> stepBound;
    if (originalModel.isDiscreteTimeModel() && !cumulativeRewardFormula.getTimeBoundReference().isRewardBound() &&
        cumulativeRewardFormula.getBound().getBaseExpression().isIntegerLiteralExpression()) {
        stepBound = cumulativeRewardFormula.getBound().evaluateAsInt();
        if (cumulativeRewardFormula.isBoundStrict()) {
            if (stepBound.value() == 0) {
                return std::nullopt;  // Don't treat formulas of the form C<0 here.
            }
            --stepBound.value();
        }
    }

    auto const& originalRewardModel =
        formula.hasRewardModelName() ? originalModel.getRewardModel(formula.getRewardModelName()) : originalModel.getUniqueRewardModel();

    // Get the states for which no reward is reachable (within the step bound)
    auto const& transitions = originalModel.getTransitionMatrix();
    auto const backwardTransitions = originalModel.getBackwardTransitions();
    storm::storage::BitVector maybeStates;
    storm::storage::BitVector const statesWithNonZeroRewards = ~originalRewardModel.getStatesWithZeroReward(transitions);
    storm::storage::BitVector const allStates(originalModel.getNumberOfStates(), true);
    if (originalModel.isNondeterministicModel()) {
        maybeStates = isMinimizing(formula)
                          ? storm::utility::graph::performProbGreater0A(transitions, transitions.getRowGroupIndices(), backwardTransitions, allStates,
                                                                        statesWithNonZeroRewards, stepBound.has_value(), stepBound.value_or(0))
                          : storm::utility::graph::performProbGreater0E(backwardTransitions, allStates, statesWithNonZeroRewards, stepBound.has_value(),
                                                                        stepBound.value_or(0));
    } else {
        maybeStates =
            storm::utility::graph::performProbGreater0(backwardTransitions, allStates, statesWithNonZeroRewards, stepBound.has_value(), stepBound.value_or(0));
    }
    auto const zeroExpectedRewardStates = ~maybeStates;
    auto const noStates = ~allStates;

    auto rewardModels = formula.getReferencedRewardModels();
    if (originalModel.hasUniqueRewardModel()) {
        rewardModels.emplace(originalModel.getUniqueRewardModelName());
    }
    std::vector<std::string> rewardModelNameAsVector(rewardModels.begin(), rewardModels.end());
    return mergeTargetAndSinkStates(maybeStates, noStates, zeroExpectedRewardStates, rewardModelNameAsVector, std::nullopt, std::nullopt,
                                    representative(zeroExpectedRewardStates));
}

template<typename ValueType>
std::pair<typename GoalStateMerger<ValueType>::ReturnType, uint64_t> GoalStateMerger<ValueType>::initialize(
    storm::storage::BitVector const& maybeStates, storm::storage::BitVector const& targetStates, storm::storage::BitVector const& sinkStates,
    std::optional<storm::storage::BitVector> const& choiceFilter) const {
    storm::storage::SparseMatrix<ValueType> const& origMatrix = originalModel.getTransitionMatrix();

    ReturnType result;
    result.keptChoices = storm::storage::BitVector(origMatrix.getRowCount(), false);
    result.oldToNewStateIndexMapping = std::vector<uint64_t>(maybeStates.size(), std::numeric_limits<uint64_t>::max());  // init with some invalid state

    uint64_t transitionCount(0), stateCount(0);
    bool targetStateRequired = !originalModel.getInitialStates().isDisjointFrom(targetStates);
    bool sinkStateRequired = !originalModel.getInitialStates().isDisjointFrom(sinkStates);
    for (auto state : maybeStates) {
        result.oldToNewStateIndexMapping[state] = stateCount;

        auto const& endOfRowGroup = origMatrix.getRowGroupIndices()[state + 1];
        bool stateIsDeadlock = true;
        for (uint64_t row = origMatrix.getRowGroupIndices()[state]; row < endOfRowGroup; ++row) {
            uint64_t transitionsToMaybeStates = 0;
            bool keepThisRow(true), hasTransitionToTarget(false), hasTransitionToSink(false);
            if (!choiceFilter || choiceFilter->get(row)) {
                for (auto const& entry : origMatrix.getRow(row)) {
                    if (maybeStates.get(entry.getColumn())) {
                        ++transitionsToMaybeStates;
                    } else if (targetStates.get(entry.getColumn())) {
                        hasTransitionToTarget = true;
                    } else if (sinkStates.get(entry.getColumn())) {
                        hasTransitionToSink = true;
                    } else {
                        keepThisRow = false;
                        break;
                    }
                }
                if (keepThisRow) {
                    stateIsDeadlock = false;
                    result.keptChoices.set(row, true);
                    transitionCount += transitionsToMaybeStates;
                    if (hasTransitionToTarget) {
                        ++transitionCount;
                        targetStateRequired = true;
                    }
                    if (hasTransitionToSink) {
                        ++transitionCount;
                        sinkStateRequired = true;
                    }
                }
            }
        }
        STORM_LOG_THROW(!stateIsDeadlock, storm::exceptions::InvalidArgumentException, "Merging goal states leads to deadlocks!");
        ++stateCount;
    }

    // Treat the target and sink states (if these states will exist)
    if (targetStateRequired) {
        result.targetState = stateCount;
        ++stateCount;
        ++transitionCount;
        storm::utility::vector::setVectorValues(result.oldToNewStateIndexMapping, targetStates, *result.targetState);
    }

    if (sinkStateRequired) {
        result.sinkState = stateCount;
        ++stateCount;
        ++transitionCount;
        storm::utility::vector::setVectorValues(result.oldToNewStateIndexMapping, sinkStates, *result.sinkState);
    }

    return std::make_pair(std::move(result), std::move(transitionCount));
}

template<typename ValueType>
storm::storage::SparseMatrix<ValueType> GoalStateMerger<ValueType>::buildTransitionMatrix(storm::storage::BitVector const& maybeStates,
                                                                                          ReturnType const& resultData, uint64_t transitionCount) const {
    storm::storage::SparseMatrix<ValueType> const& origMatrix = originalModel.getTransitionMatrix();

    uint64_t rowCount = resultData.keptChoices.getNumberOfSetBits() + (resultData.targetState ? 1 : 0) + (resultData.sinkState ? 1 : 0);
    uint64_t maybeStateCount = maybeStates.getNumberOfSetBits();
    uint64_t stateCount = maybeStateCount + (resultData.targetState ? 1 : 0) + (resultData.sinkState ? 1 : 0);
    storm::storage::SparseMatrixBuilder<ValueType> builder(rowCount, stateCount, transitionCount, true, !origMatrix.hasTrivialRowGrouping(),
                                                           origMatrix.hasTrivialRowGrouping() ? 0 : stateCount);

    uint64_t currRow = 0;
    for (auto state : maybeStates) {
        if (!origMatrix.hasTrivialRowGrouping()) {
            builder.newRowGroup(currRow);
        }
        auto const& endOfRowGroup = origMatrix.getRowGroupIndices()[state + 1];
        for (uint64_t row = resultData.keptChoices.getNextSetIndex(origMatrix.getRowGroupIndices()[state]); row < endOfRowGroup;
             row = resultData.keptChoices.getNextSetIndex(row + 1)) {
            std::optional<ValueType> targetValue, sinkValue;
            for (auto const& entry : origMatrix.getRow(row)) {
                uint64_t const& newColumn = resultData.oldToNewStateIndexMapping[entry.getColumn()];
                if (newColumn < maybeStateCount) {
                    builder.addNextValue(currRow, newColumn, entry.getValue());
                } else if (resultData.targetState && newColumn == resultData.targetState.value()) {
                    targetValue = targetValue.has_value() ? *targetValue + entry.getValue() : entry.getValue();
                } else if (resultData.sinkState && newColumn == resultData.sinkState.value()) {
                    sinkValue = sinkValue.has_value() ? *sinkValue + entry.getValue() : entry.getValue();
                } else {
                    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException,
                                    "There is a transition originating from a maybestate that does not lead to a maybe-, target-, or sinkstate.");
                }
            }
            if (targetValue) {
                builder.addNextValue(currRow, *resultData.targetState, storm::utility::simplify(*targetValue));
            }
            if (sinkValue) {
                builder.addNextValue(currRow, *resultData.sinkState, storm::utility::simplify(*sinkValue));
            }
            ++currRow;
        }
    }

    // Add the selfloops at target and sink
    if (resultData.targetState) {
        if (!origMatrix.hasTrivialRowGrouping()) {
            builder.newRowGroup(currRow);
        }
        builder.addNextValue(currRow, *resultData.targetState, storm::utility::one<ValueType>());
        ++currRow;
    }
    if (resultData.sinkState) {
        if (!origMatrix.hasTrivialRowGrouping()) {
            builder.newRowGroup(currRow);
        }
        builder.addNextValue(currRow, *resultData.sinkState, storm::utility::one<ValueType>());
        ++currRow;
    }

    return builder.build();
}

template<typename ValueType>
storm::models::sparse::StateLabeling GoalStateMerger<ValueType>::buildStateLabeling(storm::storage::BitVector const& maybeStates,
                                                                                    storm::storage::BitVector const& targetStates,
                                                                                    storm::storage::BitVector const& sinkStates, ReturnType const& resultData,
                                                                                    std::optional<uint64_t> const representativeTargetState,
                                                                                    std::optional<uint64_t> const representativeSinkState) const {
    uint64_t stateCount = maybeStates.getNumberOfSetBits() + (resultData.targetState ? 1 : 0) + (resultData.sinkState ? 1 : 0);
    storm::models::sparse::StateLabeling labeling(stateCount);

    for (auto const& label : originalModel.getStateLabeling().getLabels()) {
        storm::storage::BitVector const& oldStatesWithLabel = originalModel.getStates(label);
        storm::storage::BitVector newStatesWithLabel = oldStatesWithLabel % maybeStates;
        newStatesWithLabel.resize(stateCount, false);

        // decide whether to mark the target/sink state with that label.
        // There are two cases where we would do so:
        // - either a representative target (or sink) state is given and contains label or
        // - it is not given but one of the target (or sink) states has that label
        // The init label is special: there we always assign the label if one of the targets/sinks is init.
        bool const isInitialStatesLabel = label == "init";
        bool const markTarget = (representativeTargetState && !isInitialStatesLabel) ? oldStatesWithLabel.get(representativeTargetState.value())
                                                                                     : !oldStatesWithLabel.isDisjointFrom(targetStates);
        if (markTarget && resultData.targetState.has_value()) {
            newStatesWithLabel.set(*resultData.targetState, true);
        }
        bool const markSink = (representativeSinkState && !isInitialStatesLabel) ? oldStatesWithLabel.get(representativeSinkState.value())
                                                                                 : !oldStatesWithLabel.isDisjointFrom(sinkStates);
        if (markSink && resultData.sinkState.has_value()) {
            newStatesWithLabel.set(*resultData.sinkState, true);
        }
        labeling.addLabel(label, std::move(newStatesWithLabel));
    }

    return labeling;
}

template<typename ValueType>
std::unordered_map<std::string, models::sparse::StandardRewardModel<ValueType>> GoalStateMerger<ValueType>::buildRewardModels(
    storm::storage::BitVector const& maybeStates, ReturnType const& resultData, std::vector<std::string> const& selectedRewardModels) const {
    uint64_t maybeStateCount = maybeStates.getNumberOfSetBits();
    uint64_t stateCount = maybeStateCount + (resultData.targetState ? 1 : 0) + (resultData.sinkState ? 1 : 0);
    uint64_t choiceCount = resultData.keptChoices.getNumberOfSetBits() + (resultData.targetState ? 1 : 0) + (resultData.sinkState ? 1 : 0);

    std::unordered_map<std::string, models::sparse::StandardRewardModel<ValueType>> rewardModels;
    for (auto rewardModelName : selectedRewardModels) {
        auto origRewardModel = originalModel.getRewardModel(rewardModelName);

        std::optional<std::vector<ValueType>> stateRewards;
        if (origRewardModel.hasStateRewards()) {
            stateRewards = storm::utility::vector::filterVector(origRewardModel.getStateRewardVector(), maybeStates);
            stateRewards->resize(stateCount, storm::utility::zero<ValueType>());
        }

        std::optional<std::vector<ValueType>> stateActionRewards;
        if (origRewardModel.hasStateActionRewards()) {
            stateActionRewards = storm::utility::vector::filterVector(origRewardModel.getStateActionRewardVector(), resultData.keptChoices);
            stateActionRewards->resize(choiceCount, storm::utility::zero<ValueType>());
        }

        std::optional<storm::storage::SparseMatrix<ValueType>> transitionRewards;
        if (origRewardModel.hasTransitionRewards()) {
            storm::storage::SparseMatrixBuilder<ValueType> builder(choiceCount, stateCount, 0, true);
            for (auto row : resultData.keptChoices) {
                std::optional<ValueType> targetValue, sinkValue;
                for (auto const& entry : origRewardModel.getTransitionRewardMatrix().getRow(row)) {
                    uint64_t const& newColumn = resultData.oldToNewStateIndexMapping[entry.getColumn()];
                    if (newColumn < maybeStateCount) {
                        builder.addNextValue(row, newColumn, entry.getValue());
                    } else if (resultData.targetState && newColumn == resultData.targetState.value()) {
                        targetValue = targetValue.has_value() ? *targetValue + entry.getValue() : entry.getValue();
                    } else if (resultData.sinkState && newColumn == resultData.sinkState.value()) {
                        sinkValue = sinkValue.has_value() ? *sinkValue + entry.getValue() : entry.getValue();
                    } else {
                        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException,
                                        "There is a transition reward originating from a maybestate that does not lead to a maybe-, target-, or sinkstate.");
                    }
                }
                if (targetValue) {
                    builder.addNextValue(row, *resultData.targetState, storm::utility::simplify(*targetValue));
                }
                if (sinkValue) {
                    builder.addNextValue(row, *resultData.sinkState, storm::utility::simplify(*sinkValue));
                }
            }
            transitionRewards = builder.build();
        }

        rewardModels.emplace(rewardModelName, models::sparse::StandardRewardModel<ValueType>(std::move(stateRewards), std::move(stateActionRewards),
                                                                                             std::move(transitionRewards)));
    }
    return rewardModels;
}

template class GoalStateMerger<double>;
template class GoalStateMerger<storm::RationalNumber>;
template class GoalStateMerger<storm::RationalFunction>;

}  // namespace transformer
}  // namespace storm
