#include "storm/modelchecker/multiobjective/deterministicScheds/DeterministicSchedsObjectiveHelper.h"

#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/multiobjective/deterministicScheds/VisitingTimesHelper.h"
#include "storm/modelchecker/prctl/helper/BaierUpperRewardBoundsComputer.h"
#include "storm/modelchecker/prctl/helper/DsMpiUpperRewardBoundsComputer.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/transformer/EndComponentEliminator.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/FilteredRewardModel.h"
#include "storm/utility/graph.h"
#include "storm/utility/vector.h"

#include "storm/exceptions/UncheckedRequirementException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm::modelchecker::multiobjective {

template<typename ModelType>
DeterministicSchedsObjectiveHelper<ModelType>::DeterministicSchedsObjectiveHelper(ModelType const& model,
                                                                                  storm::modelchecker::multiobjective::Objective<ValueType> const& objective)
    : model(model), objective(objective) {
    initialize();
}

template<typename ModelType>
storm::storage::BitVector evaluatePropositionalFormula(ModelType const& model, storm::logic::Formula const& formula) {
    storm::modelchecker::SparsePropositionalModelChecker<ModelType> mc(model);
    auto checkResult = mc.check(formula);
    STORM_LOG_THROW(checkResult && checkResult->isExplicitQualitativeCheckResult(), storm::exceptions::UnexpectedException,
                    "Unexpected type of check result for subformula " << formula << ".");
    return checkResult->template asExplicitQualitativeCheckResult<typename ModelType::ValueType>().getTruthValuesVector();
}

template<typename ValueType>
std::vector<ValueType> getTotalRewardVector(storm::models::sparse::MarkovAutomaton<ValueType> const& model, storm::logic::Formula const& formula) {
    if (formula.isRewardOperatorFormula()) {
        boost::optional<std::string> rewardModelName = formula.asRewardOperatorFormula().getOptionalRewardModelName();
        typename storm::models::sparse::MarkovAutomaton<ValueType>::RewardModelType const& rewardModel =
            rewardModelName.is_initialized() ? model.getRewardModel(rewardModelName.get()) : model.getUniqueRewardModel();

        // Get a reward model where the state rewards are scaled accordingly
        std::vector<ValueType> stateRewardWeights(model.getNumberOfStates(), storm::utility::zero<ValueType>());
        for (auto const markovianState : model.getMarkovianStates()) {
            stateRewardWeights[markovianState] = storm::utility::one<ValueType>() / model.getExitRate(markovianState);
        }
        return rewardModel.getTotalActionRewardVector(model.getTransitionMatrix(), stateRewardWeights);
    } else {
        assert(formula.isTimeOperatorFormula());
        std::vector<ValueType> result(model.getNumberOfChoices(), storm::utility::zero<ValueType>());
        for (auto const markovianState : model.getMarkovianStates()) {
            for (auto const& choice : model.getTransitionMatrix().getRowGroupIndices(markovianState)) {
                result[choice] = storm::utility::one<ValueType>() / model.getExitRate(markovianState);
            }
        }
        return result;
    }
}

template<typename ValueType>
std::vector<ValueType> getTotalRewardVector(storm::models::sparse::Mdp<ValueType> const& model, storm::logic::Formula const& formula) {
    if (formula.isRewardOperatorFormula()) {
        boost::optional<std::string> rewardModelName = formula.asRewardOperatorFormula().getOptionalRewardModelName();
        typename storm::models::sparse::Mdp<ValueType>::RewardModelType const& rewardModel =
            rewardModelName.is_initialized() ? model.getRewardModel(rewardModelName.get()) : model.getUniqueRewardModel();
        return rewardModel.getTotalRewardVector(model.getTransitionMatrix());
    } else {
        assert(formula.isTimeOperatorFormula());
        return std::vector<ValueType>(model.getNumberOfChoices(), storm::utility::one<ValueType>());
    }
}

template<typename ModelType>
void DeterministicSchedsObjectiveHelper<ModelType>::initialize() {
    STORM_LOG_ASSERT(model.getInitialStates().getNumberOfSetBits() == 1, "Expected a single initial state.");
    uint64_t initialState = *model.getInitialStates().begin();
    relevantZeroRewardChoices = storm::storage::BitVector(model.getTransitionMatrix().getRowCount(), false);
    auto const& formula = *objective.formula;
    if (formula.isProbabilityOperatorFormula() && formula.getSubformula().isUntilFormula()) {
        storm::storage::BitVector phiStates = evaluatePropositionalFormula(model, formula.getSubformula().asUntilFormula().getLeftSubformula());
        storm::storage::BitVector psiStates = evaluatePropositionalFormula(model, formula.getSubformula().asUntilFormula().getRightSubformula());
        auto backwardTransitions = model.getBackwardTransitions();
        auto prob1States = storm::utility::graph::performProb1A(model.getTransitionMatrix(), model.getNondeterministicChoiceIndices(), backwardTransitions,
                                                                phiStates, psiStates);
        auto prob0States = storm::utility::graph::performProb0A(backwardTransitions, phiStates, psiStates);
        if (prob0States.get(initialState)) {
            constantInitialStateValue = storm::utility::zero<ValueType>();
        } else if (prob1States.get(initialState)) {
            constantInitialStateValue = storm::utility::one<ValueType>();
        }
        maybeStates = ~(prob0States | prob1States);
        // Cut away those states that are not reachable, or only reachable via non-maybe states.
        maybeStates &= storm::utility::graph::getReachableStates(model.getTransitionMatrix(), model.getInitialStates(), maybeStates, ~maybeStates);
        for (auto const& state : maybeStates) {
            for (auto const& choice : model.getTransitionMatrix().getRowGroupIndices(state)) {
                auto rowSum = model.getTransitionMatrix().getConstrainedRowSum(choice, prob1States);
                if (storm::utility::isZero(rowSum)) {
                    relevantZeroRewardChoices.set(choice);
                } else {
                    choiceRewards.emplace(choice, rowSum);
                }
            }
        }
    } else if (formula.getSubformula().isEventuallyFormula() && (formula.isRewardOperatorFormula() || formula.isTimeOperatorFormula())) {
        storm::storage::BitVector rew0States = evaluatePropositionalFormula(model, formula.getSubformula().asEventuallyFormula().getSubformula());
        if (formula.isRewardOperatorFormula()) {
            auto const& baseRewardModel = formula.asRewardOperatorFormula().hasRewardModelName()
                                              ? model.getRewardModel(formula.asRewardOperatorFormula().getRewardModelName())
                                              : model.getUniqueRewardModel();
            auto rewardModel =
                storm::utility::createFilteredRewardModel(baseRewardModel, model.isDiscreteTimeModel(), formula.getSubformula().asEventuallyFormula());
            storm::storage::BitVector statesWithoutReward = rewardModel.get().getStatesWithZeroReward(model.getTransitionMatrix());
            rew0States = storm::utility::graph::performProb1A(model.getTransitionMatrix(), model.getNondeterministicChoiceIndices(),
                                                              model.getBackwardTransitions(), statesWithoutReward, rew0States);
        }
        if (rew0States.get(initialState)) {
            constantInitialStateValue = storm::utility::zero<ValueType>();
        }
        maybeStates = ~rew0States;
        // Cut away those states that are not reachable, or only reachable via non-maybe states.
        maybeStates &= storm::utility::graph::getReachableStates(model.getTransitionMatrix(), model.getInitialStates(), maybeStates, ~maybeStates);
        std::vector<ValueType> choiceBasedRewards = getTotalRewardVector(model, *objective.formula);
        for (auto const& state : maybeStates) {
            for (auto const& choice : model.getTransitionMatrix().getRowGroupIndices(state)) {
                auto const& value = choiceBasedRewards[choice];
                if (storm::utility::isZero(value)) {
                    relevantZeroRewardChoices.set(choice);
                } else {
                    choiceRewards.emplace(choice, value);
                }
            }
        }
    } else if (formula.isRewardOperatorFormula() && formula.getSubformula().isTotalRewardFormula()) {
        auto const& baseRewardModel = formula.asRewardOperatorFormula().hasRewardModelName()
                                          ? model.getRewardModel(formula.asRewardOperatorFormula().getRewardModelName())
                                          : model.getUniqueRewardModel();
        auto rewardModel =
            storm::utility::createFilteredRewardModel(baseRewardModel, model.isDiscreteTimeModel(), formula.getSubformula().asTotalRewardFormula());
        storm::storage::BitVector statesWithoutReward = rewardModel.get().getStatesWithZeroReward(model.getTransitionMatrix());
        storm::storage::BitVector rew0States =
            storm::utility::graph::performProbGreater0E(model.getBackwardTransitions(), statesWithoutReward, ~statesWithoutReward);
        rew0States.complement();
        if (rew0States.get(initialState)) {
            constantInitialStateValue = storm::utility::zero<ValueType>();
        }
        maybeStates = ~rew0States;
        // Cut away those states that are not reachable, or only reachable via non-maybe states.
        maybeStates &= storm::utility::graph::getReachableStates(model.getTransitionMatrix(), model.getInitialStates(), maybeStates, ~maybeStates);
        std::vector<ValueType> choiceBasedRewards = getTotalRewardVector(model, *objective.formula);
        for (auto const& state : maybeStates) {
            for (auto const& choice : model.getTransitionMatrix().getRowGroupIndices(state)) {
                auto const& value = choiceBasedRewards[choice];
                if (storm::utility::isZero(value)) {
                    relevantZeroRewardChoices.set(choice);
                } else {
                    choiceRewards.emplace(choice, value);
                }
            }
        }
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "The given formula " << formula << " is not supported.");
    }

    // negate choice rewards for minimizing objectives
    if (storm::solver::minimize(formula.getOptimalityType())) {
        for (auto& entry : choiceRewards) {
            entry.second *= -storm::utility::one<ValueType>();
        }
    }

    // Find out if we have to deal with infinite positive or negative rewards
    storm::storage::BitVector negativeRewardChoices(model.getNumberOfChoices(), false);
    storm::storage::BitVector positiveRewardChoices(model.getNumberOfChoices(), false);
    for (auto const& rew : getChoiceRewards()) {
        if (rew.second > storm::utility::zero<ValueType>()) {
            positiveRewardChoices.set(rew.first, true);
        } else {
            assert(rew.second < storm::utility::zero<ValueType>());
            negativeRewardChoices.set(rew.first, true);
        }
    }
    auto backwardTransitions = model.getBackwardTransitions();
    bool hasNegativeEC =
        storm::utility::graph::checkIfECWithChoiceExists(model.getTransitionMatrix(), backwardTransitions, getMaybeStates(), negativeRewardChoices);
    bool hasPositiveEc =
        storm::utility::graph::checkIfECWithChoiceExists(model.getTransitionMatrix(), backwardTransitions, getMaybeStates(), positiveRewardChoices);
    STORM_LOG_THROW(!(hasNegativeEC && hasPositiveEc), storm::exceptions::NotSupportedException,
                    "Objective is not convergent: Infinite positive and infinite negative reward is possible.");
    infinityCase = hasNegativeEC ? InfinityCase::HasNegativeInfinite : (hasPositiveEc ? InfinityCase::HasPositiveInfinite : InfinityCase::AlwaysFinite);

    if (infinityCase == InfinityCase::HasNegativeInfinite) {
        storm::storage::BitVector negativeEcStates(maybeStates.size(), false);
        storm::storage::MaximalEndComponentDecomposition<ValueType> mecs(model.getTransitionMatrix(), backwardTransitions, getMaybeStates());
        for (auto const& mec : mecs) {
            if (std::any_of(mec.begin(), mec.end(), [&negativeRewardChoices](auto const& sc) {
                    return std::any_of(sc.second.begin(), sc.second.end(), [&negativeRewardChoices](auto const& c) { return negativeRewardChoices.get(c); });
                })) {
                for (auto const& sc : mec) {
                    negativeEcStates.set(sc.first);
                }
            }
        }
        STORM_LOG_ASSERT(!negativeEcStates.empty(), "Expected some negative ec");
        rewMinusInfEStates = storm::utility::graph::performProbGreater0E(backwardTransitions, getMaybeStates(), negativeEcStates);
        STORM_LOG_ASSERT(model.getInitialStates().isSubsetOf(rewMinusInfEStates), "Initial state does not reach all maybestates");
    }
}

template<typename ModelType>
storm::storage::BitVector const& DeterministicSchedsObjectiveHelper<ModelType>::getMaybeStates() const {
    return maybeStates;
}

template<typename ModelType>
storm::storage::BitVector const& DeterministicSchedsObjectiveHelper<ModelType>::getRewMinusInfEStates() const {
    STORM_LOG_ASSERT(getInfinityCase() == InfinityCase::HasNegativeInfinite, "Tried to get -inf states, but there are none");
    return rewMinusInfEStates;
}

template<typename ModelType>
bool DeterministicSchedsObjectiveHelper<ModelType>::hasConstantInitialStateValue() const {
    return constantInitialStateValue.has_value();
}

template<typename ModelType>
typename DeterministicSchedsObjectiveHelper<ModelType>::ValueType DeterministicSchedsObjectiveHelper<ModelType>::getConstantInitialStateValue() const {
    assert(hasConstantInitialStateValue());
    return *constantInitialStateValue;
}

template<typename ModelType>
std::map<uint64_t, typename ModelType::ValueType> const& DeterministicSchedsObjectiveHelper<ModelType>::getChoiceRewards() const {
    return choiceRewards;
}

template<typename ModelType>
storm::storage::BitVector const& DeterministicSchedsObjectiveHelper<ModelType>::getRelevantZeroRewardChoices() const {
    return relevantZeroRewardChoices;
}

template<typename ModelType>
typename ModelType::ValueType const& DeterministicSchedsObjectiveHelper<ModelType>::getUpperValueBoundAtState(uint64_t state) const {
    STORM_LOG_ASSERT(maybeStates.get(state), "Expected a maybestate.");
    STORM_LOG_ASSERT(upperResultBounds.has_value(), "requested upper value bounds but they were not computed.");
    return upperResultBounds->at(state);
}

template<typename ModelType>
typename ModelType::ValueType const& DeterministicSchedsObjectiveHelper<ModelType>::getLowerValueBoundAtState(uint64_t state) const {
    STORM_LOG_ASSERT(maybeStates.get(state), "Expected a maybestate.");
    STORM_LOG_ASSERT(lowerResultBounds.has_value(), "requested lower value bounds but they were not computed.");
    return lowerResultBounds->at(state);
}

template<typename ModelType>
typename DeterministicSchedsObjectiveHelper<ModelType>::InfinityCase const& DeterministicSchedsObjectiveHelper<ModelType>::getInfinityCase() const {
    return infinityCase;
}

template<typename ModelType>
bool DeterministicSchedsObjectiveHelper<ModelType>::isTotalRewardObjective() const {
    return objective.formula->isRewardOperatorFormula() && objective.formula->getSubformula().isTotalRewardFormula();
}

template<typename ModelType>
bool DeterministicSchedsObjectiveHelper<ModelType>::hasThreshold() const {
    return objective.formula->hasBound();
}

template<typename ModelType>
typename DeterministicSchedsObjectiveHelper<ModelType>::ValueType DeterministicSchedsObjectiveHelper<ModelType>::getThreshold() const {
    STORM_LOG_ASSERT(hasThreshold(), "Trying to get a threshold but there is none");
    STORM_LOG_THROW(!storm::logic::isStrict(objective.formula->getBound().comparisonType), storm::exceptions::NotSupportedException,
                    "Objective " << *objective.originalFormula << ":  Strict objective thresholds are not supported.");
    ValueType threshold = objective.formula->template getThresholdAs<ValueType>();
    if (storm::solver::minimize(objective.formula->getOptimalityType())) {
        threshold *= -storm::utility::one<ValueType>();  // negate minimizing thresholds
        STORM_LOG_THROW(!storm::logic::isLowerBound(objective.formula->getBound().comparisonType), storm::exceptions::NotSupportedException,
                        "Objective " << *objective.originalFormula << ":  Minimizing objective should specify an upper bound.");
    } else {
        STORM_LOG_THROW(storm::logic::isLowerBound(objective.formula->getBound().comparisonType), storm::exceptions::NotSupportedException,
                        "Objective " << *objective.originalFormula << ":  Maximizing objective should specify a lower bound.");
    }
    return threshold;
}

template<typename ModelType>
bool DeterministicSchedsObjectiveHelper<ModelType>::minimizing() const {
    return storm::solver::minimize(objective.formula->getOptimalityType());
}

template<typename ValueType>
void setLowerUpperTotalRewardBoundsToSolver(storm::solver::AbstractEquationSolver<ValueType>& solver, storm::storage::SparseMatrix<ValueType> const& matrix,
                                            std::vector<ValueType> const& rewards, std::vector<ValueType> const& exitProbabilities,
                                            std::optional<storm::OptimizationDirection> dir, bool reqLower, bool reqUpper) {
    if (!reqLower && !reqUpper) {
        return;  // nothing to be done!
    }
    STORM_LOG_ASSERT(!rewards.empty(), "empty reward vector,");

    auto [minIt, maxIt] = std::minmax_element(rewards.begin(), rewards.end());
    bool const hasNegativeValues = *minIt < storm::utility::zero<ValueType>();
    bool const hasPositiveValues = *maxIt > storm::utility::zero<ValueType>();

    std::optional<ValueType> lowerBound, upperBound;

    // Get 0 as a  trivial lower/upper bound if possible
    if (!hasNegativeValues) {
        lowerBound = storm::utility::zero<ValueType>();
    }
    if (!hasPositiveValues) {
        upperBound = storm::utility::zero<ValueType>();
    }

    // Invoke the respective reward bound computers if needed
    std::vector<ValueType> tmpRewards;
    if (reqUpper && !upperBound.has_value()) {
        if (dir.has_value() && maximize(*dir)) {
            solver.setUpperBound(
                storm::modelchecker::helper::BaierUpperRewardBoundsComputer<ValueType>(matrix, exitProbabilities).computeTotalRewardBounds(rewards).upper);
        } else {
            if (hasNegativeValues) {
                tmpRewards.resize(rewards.size());
                storm::utility::vector::applyPointwise(rewards, tmpRewards, [](ValueType const& v) { return std::max(storm::utility::zero<ValueType>(), v); });
            }
            solver.setUpperBounds(
                storm::modelchecker::helper::DsMpiMdpUpperRewardBoundsComputer<ValueType>(matrix, hasNegativeValues ? tmpRewards : rewards, exitProbabilities)
                    .computeUpperBounds());
        }
    }
    if (reqLower && !lowerBound.has_value()) {
        if (dir.has_value() && minimize(*dir)) {
            solver.setLowerBound(
                storm::modelchecker::helper::BaierUpperRewardBoundsComputer<ValueType>(matrix, exitProbabilities).computeTotalRewardBounds(rewards).lower);
        } else {
            // For lower bounds we actually compute upper bounds for the negated rewards because DsMpi is not implemented for negative rewards.
            tmpRewards.resize(rewards.size());
            storm::utility::vector::applyPointwise(rewards, tmpRewards,
                                                   [](ValueType const& v) { return std::max<ValueType>(storm::utility::zero<ValueType>(), -v); });
            auto lowerBounds =
                storm::modelchecker::helper::DsMpiMdpUpperRewardBoundsComputer<ValueType>(matrix, tmpRewards, exitProbabilities).computeUpperBounds();
            storm::utility::vector::applyPointwise(lowerBounds, lowerBounds, [](ValueType const& v) { return -v; });
            solver.setLowerBounds(std::move(lowerBounds));
        }
    }
}

template<typename ValueType>
std::vector<ValueType> computeValuesOfReducedSystem(Environment const& env, storm::storage::SparseMatrix<ValueType> const& submatrix,
                                                    std::vector<ValueType> const& exitProbs, std::vector<ValueType> const& rewards,
                                                    storm::OptimizationDirection const& dir) {
    auto minMaxSolver = storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType>().create(env, submatrix);
    minMaxSolver->setHasUniqueSolution(true);
    minMaxSolver->setOptimizationDirection(dir);
    auto req = minMaxSolver->getRequirements(env, dir);
    setLowerUpperTotalRewardBoundsToSolver(*minMaxSolver, submatrix, rewards, exitProbs, dir, req.lowerBounds(), req.upperBounds());
    req.clearBounds();
    if (req.validInitialScheduler()) {
        std::vector<uint64_t> initSched(submatrix.getRowGroupCount());
        auto backwardsTransitions = submatrix.transpose(true);
        storm::storage::BitVector statesWithChoice(initSched.size(), false);
        storm::storage::BitVector foundStates(initSched.size(), false);
        std::vector<uint64_t> stack;
        for (uint64_t state = 0; state < initSched.size(); ++state) {
            if (foundStates.get(state)) {
                continue;
            }
            for (auto choice : submatrix.getRowGroupIndices(state)) {
                if (!storm::utility::isZero(exitProbs[choice])) {
                    initSched[state] = choice - submatrix.getRowGroupIndices()[state];
                    statesWithChoice.set(state);
                    foundStates.set(state);
                    for (auto const& predecessor : backwardsTransitions.getRow(state)) {
                        if (!foundStates.get(predecessor.getColumn())) {
                            stack.push_back(predecessor.getColumn());
                            foundStates.set(predecessor.getColumn());
                        }
                    }
                    break;
                }
            }
        }
        while (!stack.empty()) {
            auto state = stack.back();
            stack.pop_back();
            for (auto choice : submatrix.getRowGroupIndices(state)) {
                auto row = submatrix.getRow(choice);
                if (std::any_of(row.begin(), row.end(), [&statesWithChoice](auto const& entry) {
                        return !storm::utility::isZero(entry.getValue()) && statesWithChoice.get(entry.getColumn());
                    })) {
                    initSched[state] = choice - submatrix.getRowGroupIndices()[state];
                    statesWithChoice.set(state);
                    break;
                }
            }
            assert(statesWithChoice.get(state));
            for (auto const& predecessor : backwardsTransitions.getRow(state)) {
                if (!foundStates.get(predecessor.getColumn())) {
                    stack.push_back(predecessor.getColumn());
                    foundStates.set(predecessor.getColumn());
                }
            }
        }
        assert(statesWithChoice.full());
        minMaxSolver->setInitialScheduler(std::move(initSched));
        req.clearValidInitialScheduler();
    }
    STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                    "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
    minMaxSolver->setRequirementsChecked(true);

    std::vector<ValueType> result(submatrix.getRowGroupCount());
    minMaxSolver->solveEquations(env, result, rewards);

    return result;
}

template<typename ValueType>
void plusMinMaxSolverPrecision(Environment const& env, ValueType& value) {
    if (!storm::NumberTraits<ValueType>::IsExact) {
        auto eps = storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision());
        if (env.solver().minMax().getRelativeTerminationCriterion()) {
            value += value * eps;
        } else {
            value += eps;
        }
    }
}

template<typename ValueType>
void minusMinMaxSolverPrecision(Environment const& env, ValueType& value) {
    if (!storm::NumberTraits<ValueType>::IsExact) {
        auto eps = storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision());
        if (env.solver().minMax().getRelativeTerminationCriterion()) {
            value -= value * eps;
        } else {
            value -= eps;
        }
    }
}

template<storm::OptimizationDirection Dir, typename ValueType>
ValueType sumOfMecRewards(storm::storage::MaximalEndComponent const& mec, std::vector<ValueType> const& rewards) {
    auto sum = storm::utility::zero<ValueType>();
    for (auto const& stateChoices : mec) {
        storm::utility::Extremum<Dir, ValueType> optimalStateValue;
        for (auto const& choice : stateChoices.second) {
            optimalStateValue &= rewards.at(choice);
        }
        sum += *optimalStateValue;
    }
    return sum;
}

template<typename ValueType>
ValueType getLowerBoundForNonZeroReachProb(storm::storage::SparseMatrix<ValueType> const& transitions, uint64_t init, uint64_t target) {
    storm::storage::BitVector allStates(transitions.getRowGroupCount(), true);
    storm::storage::BitVector initialAsBitVector(transitions.getRowGroupCount(), false);
    initialAsBitVector.set(init, true);
    storm::storage::BitVector targetAsBitVector(transitions.getRowGroupCount(), false);
    targetAsBitVector.set(target, true);
    auto relevantStates = storm::utility::graph::getReachableStates(transitions, initialAsBitVector, allStates, targetAsBitVector);
    auto product = storm::utility::one<ValueType>();
    for (auto state : relevantStates) {
        if (state == target) {
            continue;
        }
        storm::utility::Minimum<ValueType> minProb;
        for (auto const& entry : transitions.getRowGroup(state)) {
            if (relevantStates.get(entry.getColumn())) {
                minProb &= entry.getValue();
            }
        }
        product *= *minProb;
    }
    return product;
}

template<typename ModelType>
void DeterministicSchedsObjectiveHelper<ModelType>::computeLowerUpperBounds(Environment const& env) const {
    assert(!upperResultBounds.has_value() && !lowerResultBounds.has_value());
    auto backwardTransitions = model.getBackwardTransitions();
    auto nonMaybeStates = ~maybeStates;
    // Eliminate problematic mecs
    storm::storage::MaximalEndComponentDecomposition<ValueType> problMecs(model.getTransitionMatrix(), backwardTransitions, maybeStates,
                                                                          getRelevantZeroRewardChoices());
    auto quotient1 = storm::transformer::EndComponentEliminator<ValueType>::transform(model.getTransitionMatrix(), problMecs, maybeStates, maybeStates);
    auto backwardTransitions1 = quotient1.matrix.transpose(true);
    std::vector<ValueType> rewards1(quotient1.matrix.getRowCount(), storm::utility::zero<ValueType>());
    std::vector<ValueType> exitProbs1(quotient1.matrix.getRowCount(), storm::utility::zero<ValueType>());
    storm::storage::BitVector subsystemChoices1(quotient1.matrix.getRowCount(), true);
    storm::storage::BitVector allQuotient1States(quotient1.matrix.getRowGroupCount(), true);
    for (uint64_t choice = 0; choice < quotient1.matrix.getRowCount(); ++choice) {
        auto const& oldChoice = quotient1.newToOldRowMapping[choice];
        if (auto findRes = choiceRewards.find(oldChoice); findRes != choiceRewards.end()) {
            rewards1[choice] = findRes->second;
        }
        if (quotient1.sinkRows.get(choice)) {
            subsystemChoices1.set(choice, false);
            exitProbs1[choice] = storm::utility::one<ValueType>();
        } else if (auto prob = model.getTransitionMatrix().getConstrainedRowSum(oldChoice, nonMaybeStates); !storm::utility::isZero(prob)) {
            exitProbs1[choice] = prob;
            subsystemChoices1.set(choice, false);
        }
    }
    // Assert that every state can eventually exit the subsystem. If that is not the case, we get infinite reward as problematic (aka zero mecs) are removed.
    auto exitStates1 = quotient1.matrix.getRowGroupFilter(~subsystemChoices1, false);
    STORM_LOG_THROW(storm::utility::graph::performProbGreater0E(backwardTransitions1, allQuotient1States, exitStates1).full(),
                    storm::exceptions::InvalidOperationException, "Objective " << *objective.originalFormula << " does not induce finite value.");

    // Compute bounds for finite cases
    if (getInfinityCase() != InfinityCase::HasNegativeInfinite) {
        auto result1 = computeValuesOfReducedSystem(env, quotient1.matrix, exitProbs1, rewards1, storm::OptimizationDirection::Minimize);
        lowerResultBounds = std::vector<ValueType>(model.getNumberOfStates(), storm::utility::zero<ValueType>());
        for (auto const& state : maybeStates) {
            ValueType val = result1.at(quotient1.oldToNewStateMapping.at(state));
            minusMinMaxSolverPrecision(env, val);
            (*lowerResultBounds)[state] = val;
        }
    }
    if (getInfinityCase() != InfinityCase::HasPositiveInfinite) {
        auto result1 = computeValuesOfReducedSystem(env, quotient1.matrix, exitProbs1, rewards1, storm::OptimizationDirection::Maximize);
        upperResultBounds = std::vector<ValueType>(model.getNumberOfStates(), storm::utility::zero<ValueType>());
        for (auto const& state : maybeStates) {
            ValueType val = result1.at(quotient1.oldToNewStateMapping.at(state));
            plusMinMaxSolverPrecision(env, val);
            if (infinityCase == InfinityCase::HasNegativeInfinite) {  // Upper bound has to be at least 0 to allow for "trivial solution" in encoding
                val = std::max(val, storm::utility::zero<ValueType>());
            }
            (*upperResultBounds)[state] = val;
        }
    }
    if (getInfinityCase() != InfinityCase::AlwaysFinite) {
        assert(getInfinityCase() == InfinityCase::HasPositiveInfinite || getInfinityCase() == InfinityCase::HasNegativeInfinite);
        STORM_LOG_THROW(
            hasThreshold() || getInfinityCase() == InfinityCase::HasNegativeInfinite, storm::exceptions::NotSupportedException,
            "The upper bound for objective " << *objective.originalFormula << " is infinity at some state. This is only supported for thresholded objectives");
        // Eliminate remaining mecs
        storm::storage::MaximalEndComponentDecomposition<ValueType> remainingMecs(quotient1.matrix, backwardTransitions1, allQuotient1States,
                                                                                  subsystemChoices1);
        STORM_LOG_ASSERT(!remainingMecs.empty(), "Incorrect infinityCase: There is no MEC with rewards.");
        auto quotient2 =
            storm::transformer::EndComponentEliminator<ValueType>::transform(quotient1.matrix, remainingMecs, allQuotient1States, allQuotient1States);
        std::vector<ValueType> rewards2(quotient2.matrix.getRowCount(), storm::utility::zero<ValueType>());
        std::vector<ValueType> exitProbs2(quotient2.matrix.getRowCount(), storm::utility::zero<ValueType>());
        for (uint64_t choice = 0; choice < quotient2.matrix.getRowCount(); ++choice) {
            auto const& oldChoice = quotient2.newToOldRowMapping[choice];
            if (quotient2.sinkRows.get(choice)) {
                exitProbs2[choice] = storm::utility::one<ValueType>();
            } else {
                rewards2[choice] = rewards1[oldChoice];
                exitProbs2[choice] = exitProbs1[oldChoice];
            }
        }

        // Add rewards within ECs
        auto initialState2 = quotient2.oldToNewStateMapping.at(quotient1.oldToNewStateMapping.at(*model.getInitialStates().begin()));
        ValueType rewardValueForPosInfCase;
        if (getInfinityCase() == InfinityCase::HasPositiveInfinite) {
            rewardValueForPosInfCase = getThreshold();
            // We need to substract a lower bound for the value at the initial state
            std::vector<ValueType> rewards2Negative;
            rewards2Negative.reserve(rewards2.size());
            for (auto const& rew : rewards2) {
                rewards2Negative.push_back(std::min(rew, storm::utility::zero<ValueType>()));
            }
            auto lower2 = computeValuesOfReducedSystem(env, quotient2.matrix, exitProbs2, rewards2Negative, storm::OptimizationDirection::Minimize);
            rewardValueForPosInfCase -= lower2.at(initialState2);
        }
        for (auto const& mec : remainingMecs) {
            auto mecState2 = quotient2.oldToNewStateMapping[mec.begin()->first];
            ValueType mecReward = getInfinityCase() == InfinityCase::HasPositiveInfinite
                                      ? sumOfMecRewards<storm::OptimizationDirection::Maximize>(mec, rewards1)
                                      : sumOfMecRewards<storm::OptimizationDirection::Minimize>(mec, rewards1);
            mecReward /= VisitingTimesHelper<ValueType>::computeMecTraversalLowerBound(mec, quotient1.matrix);
            auto groupIndices = quotient2.matrix.getRowGroupIndices(mecState2);
            for (auto const choice2 : groupIndices) {
                rewards2[choice2] += mecReward;
            }
            if (getInfinityCase() == InfinityCase::HasPositiveInfinite) {
                auto sinkChoice = quotient2.sinkRows.getNextSetIndex(*groupIndices.begin());
                STORM_LOG_ASSERT(sinkChoice < quotient2.matrix.getRowGroupIndices()[mecState2 + 1], "EC state in quotient has no sink row.");
                rewards2[sinkChoice] += rewardValueForPosInfCase / getLowerBoundForNonZeroReachProb(quotient2.matrix, initialState2, mecState2);
            }
        }

        // Compute and insert missing bounds
        if (getInfinityCase() == InfinityCase::HasNegativeInfinite) {
            auto result2 = computeValuesOfReducedSystem(env, quotient2.matrix, exitProbs2, rewards2, storm::OptimizationDirection::Minimize);
            lowerResultBounds = std::vector<ValueType>(model.getNumberOfStates(), storm::utility::zero<ValueType>());
            for (auto const& state : maybeStates) {
                ValueType val = result2.at(quotient2.oldToNewStateMapping.at(quotient1.oldToNewStateMapping.at(state)));
                minusMinMaxSolverPrecision(env, val);
                (*lowerResultBounds)[state] = val;
            }
        } else {
            assert(getInfinityCase() == InfinityCase::HasPositiveInfinite);
            auto result2 = computeValuesOfReducedSystem(env, quotient2.matrix, exitProbs2, rewards2, storm::OptimizationDirection::Maximize);
            upperResultBounds = std::vector<ValueType>(model.getNumberOfStates(), storm::utility::zero<ValueType>());
            for (auto const& state : maybeStates) {
                ValueType val = result2.at(quotient2.oldToNewStateMapping.at(quotient1.oldToNewStateMapping.at(state)));
                plusMinMaxSolverPrecision(env, val);
                (*upperResultBounds)[state] = val;
            }
        }
    }
    STORM_LOG_THROW(
        std::all_of(maybeStates.begin(), maybeStates.end(), [this](auto const& s) { return this->lowerResultBounds->at(s) <= this->upperResultBounds->at(s); }),
        storm::exceptions::UnexpectedException, "Pre-computed lower bound exceeds upper bound.");
}

template<typename ModelType>
typename DeterministicSchedsObjectiveHelper<ModelType>::ValueType DeterministicSchedsObjectiveHelper<ModelType>::evaluateScheduler(
    Environment const& env, storm::storage::BitVector const& selectedChoices) const {
    STORM_LOG_ASSERT(model.getInitialStates().getNumberOfSetBits() == 1u, "Expected a single initial state.");
    STORM_LOG_ASSERT(model.getInitialStates().isSubsetOf(getMaybeStates()), "Expected initial state to be maybestate.");
    STORM_LOG_ASSERT(selectedChoices.getNumberOfSetBits() == model.getNumberOfStates(), "invalid choice selection.");
    storm::storage::BitVector allStates(model.getNumberOfStates(), true);
    auto selectedMatrix = model.getTransitionMatrix().getSubmatrix(false, selectedChoices, allStates);
    assert(selectedMatrix.getRowCount() == selectedMatrix.getRowGroupCount());
    selectedMatrix.makeRowGroupingTrivial();
    auto subMaybeStates =
        getMaybeStates() & storm::utility::graph::getReachableStates(selectedMatrix, model.getInitialStates(), getMaybeStates(), ~getMaybeStates());
    auto bsccCandidates = storm::utility::graph::performProbGreater0(selectedMatrix.transpose(true), subMaybeStates, ~subMaybeStates);
    bsccCandidates.complement();  // i.e. states that can not reach non-maybe states
    if (!bsccCandidates.empty()) {
        storm::storage::StronglyConnectedComponentDecompositionOptions bsccOptions;
        bsccOptions.onlyBottomSccs(true).subsystem(bsccCandidates);
        storm::storage::StronglyConnectedComponentDecomposition bsccs(selectedMatrix, bsccOptions);
        for (auto const& bscc : bsccs) {
            for (auto const& s : bscc) {
                subMaybeStates.set(s, false);
            }
        }
    }

    if (subMaybeStates.get(*model.getInitialStates().begin())) {
        storm::solver::GeneralLinearEquationSolverFactory<ValueType> factory;
        bool const useEqSysFormat = factory.getEquationProblemFormat(env) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem;
        auto eqSysMatrix = selectedMatrix.getSubmatrix(true, subMaybeStates, subMaybeStates, useEqSysFormat);
        assert(eqSysMatrix.getRowCount() == eqSysMatrix.getRowGroupCount());
        auto exitProbs = selectedMatrix.getConstrainedRowSumVector(subMaybeStates, ~subMaybeStates);
        std::vector<ValueType> rewards;
        rewards.reserve(exitProbs.size());
        auto const& allRewards = getChoiceRewards();
        for (auto const& state : getMaybeStates()) {
            auto choice = selectedChoices.getNextSetIndex(model.getTransitionMatrix().getRowGroupIndices()[state]);
            STORM_LOG_ASSERT(choice < model.getTransitionMatrix().getRowGroupIndices()[state + 1], " no choice selected at state" << state);
            if (subMaybeStates.get(state)) {
                if (auto findRes = allRewards.find(choice); findRes != allRewards.end()) {
                    rewards.push_back(findRes->second);
                } else {
                    rewards.push_back(storm::utility::zero<ValueType>());
                }
            } else {
                STORM_LOG_ASSERT(!bsccCandidates.get(state) || allRewards.count(choice) == 0, "Strategy selected a bscc with rewards");
            }
        }
        if (useEqSysFormat) {
            eqSysMatrix.convertToEquationSystem();
        }
        auto solver = factory.create(env, eqSysMatrix);
        storm::storage::BitVector init = model.getInitialStates() % subMaybeStates;
        assert(init.getNumberOfSetBits() == 1);
        auto const initState = *init.begin();
        solver->setRelevantValues(std::move(init));
        auto req = solver->getRequirements(env);
        if (!useEqSysFormat) {
            setLowerUpperTotalRewardBoundsToSolver(*solver, eqSysMatrix, rewards, exitProbs, std::nullopt, req.lowerBounds(), req.upperBounds());
            req.clearUpperBounds();
            req.clearLowerBounds();
        }
        STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                        "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
        std::vector<ValueType> x(rewards.size());
        solver->solveEquations(env, x, rewards);

        return x[initState];
    } else {
        // initial state is on a bscc
        return storm::utility::zero<ValueType>();
    }
}

template class DeterministicSchedsObjectiveHelper<storm::models::sparse::Mdp<double>>;
template class DeterministicSchedsObjectiveHelper<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class DeterministicSchedsObjectiveHelper<storm::models::sparse::MarkovAutomaton<double>>;
template class DeterministicSchedsObjectiveHelper<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;
}  // namespace storm::modelchecker::multiobjective