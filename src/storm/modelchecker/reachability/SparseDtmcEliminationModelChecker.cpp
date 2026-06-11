#include "storm/modelchecker/reachability/SparseDtmcEliminationModelChecker.h"

#include <algorithm>
#include <chrono>
#include <random>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/EliminationSettings.h"
#include "storm/solver/stateelimination/ConditionalStateEliminator.h"
#include "storm/solver/stateelimination/DynamicStatePriorityQueue.h"
#include "storm/solver/stateelimination/MultiValueStateEliminator.h"
#include "storm/solver/stateelimination/PrioritizedStateEliminator.h"
#include "storm/solver/stateelimination/StaticStatePriorityQueue.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/stateelimination.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {

using namespace storm::utility::stateelimination;

template<typename SparseDtmcModelType>
SparseDtmcEliminationModelChecker<SparseDtmcModelType>::SparseDtmcEliminationModelChecker(storm::models::sparse::Dtmc<ValueType> const& model)
    : SparsePropositionalModelChecker<SparseDtmcModelType>(model) {
    // Intentionally left empty.
}

template<typename SparseDtmcModelType>
bool SparseDtmcEliminationModelChecker<SparseDtmcModelType>::canHandle(CheckTask<storm::logic::Formula, SolutionType> const& checkTask) const {
    storm::logic::Formula const& formula = checkTask.getFormula();
    storm::logic::FragmentSpecification fragment = storm::logic::prctl().setCumulativeRewardFormulasAllowed(false).setInstantaneousFormulasAllowed(false);
    fragment.setNestedOperatorsAllowed(false)
        .setLongRunAverageProbabilitiesAllowed(true)
        .setConditionalProbabilityFormulasAllowed(true)
        .setOnlyEventuallyFormuluasInConditionalFormulasAllowed(true);
    return formula.isInFragment(fragment);
}

template<typename SparseDtmcModelType>
std::unique_ptr<CheckResult> SparseDtmcEliminationModelChecker<SparseDtmcModelType>::computeLongRunAverageProbabilities(
    Environment const& env, CheckTask<storm::logic::StateFormula, SolutionType> const& checkTask) {
    storm::logic::StateFormula const& stateFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(stateFormula);
    storm::storage::BitVector const& psiStates = subResultPointer->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();

    storm::storage::SparseMatrix<ValueType> const& transitionMatrix = this->getModel().getTransitionMatrix();
    uint_fast64_t numberOfStates = transitionMatrix.getRowCount();
    if (psiStates.empty()) {
        return std::unique_ptr<CheckResult>(
            new ExplicitQuantitativeCheckResult<ValueType>(std::vector<ValueType>(numberOfStates, storm::utility::zero<ValueType>())));
    }
    if (psiStates.full()) {
        return std::unique_ptr<CheckResult>(
            new ExplicitQuantitativeCheckResult<ValueType>(std::vector<ValueType>(numberOfStates, storm::utility::one<ValueType>())));
    }

    storm::storage::BitVector const& initialStates = this->getModel().getInitialStates();
    STORM_LOG_THROW(initialStates.getNumberOfSetBits() == 1, storm::exceptions::IllegalArgumentException,
                    "Input model is required to have exactly one initial state.");
    STORM_LOG_THROW(checkTask.isOnlyInitialStatesRelevantSet(), storm::exceptions::IllegalArgumentException,
                    "Cannot compute long-run probabilities for all states.");

    storm::storage::SparseMatrix<ValueType> backwardTransitions = this->getModel().getBackwardTransitions();
    storm::storage::BitVector maybeStates =
        storm::utility::graph::performProbGreater0(backwardTransitions, storm::storage::BitVector(transitionMatrix.getRowCount(), true), psiStates);

    std::vector<ValueType> result(transitionMatrix.getRowCount(), storm::utility::zero<ValueType>());

    // Determine whether we need to perform some further computation.
    bool furtherComputationNeeded = true;
    if (checkTask.isOnlyInitialStatesRelevantSet() && initialStates.isDisjointFrom(maybeStates)) {
        STORM_LOG_DEBUG("The long-run probability for all initial states was found in a preprocessing step.");
        furtherComputationNeeded = false;
    }
    if (maybeStates.empty()) {
        STORM_LOG_DEBUG("The long-run probability for all states was found in a preprocessing step.");
        furtherComputationNeeded = false;
    }

    if (furtherComputationNeeded) {
        if (checkTask.isOnlyInitialStatesRelevantSet()) {
            // Determine the set of states that is reachable from the initial state without jumping over a target state.
            storm::storage::BitVector reachableStates = storm::utility::graph::getReachableStates(
                transitionMatrix, initialStates, storm::storage::BitVector(numberOfStates, true), storm::storage::BitVector(numberOfStates, false));

            // Subtract from the maybe states the set of states that is not reachable (on a path from the initial to a target state).
            maybeStates &= reachableStates;
        }

        std::vector<ValueType> stateValues(maybeStates.size(), storm::utility::zero<ValueType>());
        storm::utility::vector::setVectorValues(stateValues, psiStates, storm::utility::one<ValueType>());
        result =
            computeLongRunValues(transitionMatrix, backwardTransitions, initialStates, maybeStates, checkTask.isOnlyInitialStatesRelevantSet(), stateValues);
    }

    // Construct check result based on whether we have computed values for all states or just the initial states.
    std::unique_ptr<CheckResult> checkResult(new ExplicitQuantitativeCheckResult<ValueType>(result));
    if (checkTask.isOnlyInitialStatesRelevantSet()) {
        // If we computed the results for the initial states only, we need to filter the result to only
        // communicate these results.
        checkResult->filter(ExplicitQualitativeCheckResult<ValueType>(initialStates));
    }
    return checkResult;
}

template<typename SparseDtmcModelType>
std::unique_ptr<CheckResult> SparseDtmcEliminationModelChecker<SparseDtmcModelType>::computeLongRunAverageRewards(
    Environment const& env, CheckTask<storm::logic::LongRunAverageRewardFormula, SolutionType> const& checkTask) {
    // Do some sanity checks to establish some required properties.
    RewardModelType const& rewardModel = this->getModel().getRewardModel(checkTask.isRewardModelSet() ? checkTask.getRewardModel() : "");
    STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::IllegalArgumentException, "Input model does not have a reward model.");

    storm::storage::BitVector const& initialStates = this->getModel().getInitialStates();
    STORM_LOG_THROW(initialStates.getNumberOfSetBits() == 1, storm::exceptions::IllegalArgumentException,
                    "Input model is required to have exactly one initial state.");
    STORM_LOG_THROW(checkTask.isOnlyInitialStatesRelevantSet(), storm::exceptions::IllegalArgumentException,
                    "Cannot compute long-run probabilities for all states.");

    storm::storage::SparseMatrix<ValueType> const& transitionMatrix = this->getModel().getTransitionMatrix();
    uint_fast64_t numberOfStates = transitionMatrix.getRowCount();

    // Get the state-reward values from the reward model.
    std::vector<ValueType> stateRewardValues = rewardModel.getTotalRewardVector(this->getModel().getTransitionMatrix());

    storm::storage::BitVector maybeStates(stateRewardValues.size());
    uint_fast64_t index = 0;
    for (auto const& value : stateRewardValues) {
        if (value != storm::utility::zero<ValueType>()) {
            maybeStates.set(index, true);
        }
        ++index;
    }

    storm::storage::SparseMatrix<ValueType> backwardTransitions = this->getModel().getBackwardTransitions();

    storm::storage::BitVector allStates(numberOfStates, true);
    maybeStates = storm::utility::graph::performProbGreater0(backwardTransitions, allStates, maybeStates);

    std::vector<ValueType> result(numberOfStates, storm::utility::zero<ValueType>());

    // Determine whether we need to perform some further computation.
    bool furtherComputationNeeded = true;
    if (checkTask.isOnlyInitialStatesRelevantSet() && initialStates.isDisjointFrom(maybeStates)) {
        furtherComputationNeeded = false;
    }

    if (furtherComputationNeeded) {
        if (checkTask.isOnlyInitialStatesRelevantSet()) {
            // Determine the set of states that is reachable from the initial state without jumping over a target state.
            storm::storage::BitVector reachableStates = storm::utility::graph::getReachableStates(
                transitionMatrix, initialStates, storm::storage::BitVector(numberOfStates, true), storm::storage::BitVector(numberOfStates, false));

            // Subtract from the maybe states the set of states that is not reachable (on a path from the initial to a target state).
            maybeStates &= reachableStates;
        }

        result = computeLongRunValues(transitionMatrix, backwardTransitions, initialStates, maybeStates, checkTask.isOnlyInitialStatesRelevantSet(),
                                      stateRewardValues);
    }

    // Construct check result based on whether we have computed values for all states or just the initial states.
    std::unique_ptr<CheckResult> checkResult(new ExplicitQuantitativeCheckResult<ValueType>(result));
    if (checkTask.isOnlyInitialStatesRelevantSet()) {
        // If we computed the results for the initial states only, we need to filter the result to only
        // communicate these results.
        checkResult->filter(ExplicitQualitativeCheckResult<ValueType>(initialStates));
    }
    return checkResult;
}

template<typename SparseDtmcModelType>
std::vector<typename SparseDtmcEliminationModelChecker<SparseDtmcModelType>::SolutionType>
SparseDtmcEliminationModelChecker<SparseDtmcModelType>::computeLongRunValues(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                             storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                             storm::storage::BitVector const& initialStates,
                                                                             storm::storage::BitVector const& maybeStates,
                                                                             bool computeResultsForInitialStatesOnly, std::vector<ValueType>& stateValues) {
    std::chrono::high_resolution_clock::time_point totalTimeStart = std::chrono::high_resolution_clock::now();

    // Start by decomposing the DTMC into its BSCCs.
    std::chrono::high_resolution_clock::time_point sccDecompositionStart = std::chrono::high_resolution_clock::now();
    storm::storage::StronglyConnectedComponentDecomposition<ValueType> bsccDecomposition(
        transitionMatrix, storm::storage::StronglyConnectedComponentDecompositionOptions().onlyBottomSccs());
    auto sccDecompositionEnd = std::chrono::high_resolution_clock::now();

    std::chrono::high_resolution_clock::time_point conversionStart = std::chrono::high_resolution_clock::now();

    // Then, we convert the reduced matrix to a more flexible format to be able to perform state elimination more easily.
    storm::storage::FlexibleSparseMatrix<ValueType> flexibleMatrix(transitionMatrix);
    flexibleMatrix.filterEntries(maybeStates, maybeStates);
    storm::storage::FlexibleSparseMatrix<ValueType> flexibleBackwardTransitions(backwardTransitions);
    flexibleBackwardTransitions.filterEntries(maybeStates, maybeStates);
    auto conversionEnd = std::chrono::high_resolution_clock::now();

    std::chrono::high_resolution_clock::time_point modelCheckingStart = std::chrono::high_resolution_clock::now();

    storm::settings::modules::EliminationSettings::EliminationOrder order =
        storm::settings::getModule<storm::settings::modules::EliminationSettings>().getEliminationOrder();
    boost::optional<std::vector<uint_fast64_t>> distanceBasedPriorities;
    if (eliminationOrderNeedsDistances(order)) {
        distanceBasedPriorities = getDistanceBasedPriorities(transitionMatrix, backwardTransitions, initialStates, stateValues,
                                                             eliminationOrderNeedsForwardDistances(order), eliminationOrderNeedsReversedDistances(order));
    }

    uint_fast64_t numberOfStates = transitionMatrix.getRowCount();
    storm::storage::BitVector regularStatesInBsccs(numberOfStates);
    storm::storage::BitVector relevantBsccs(bsccDecomposition.size());
    storm::storage::BitVector bsccRepresentativesAsBitVector(numberOfStates);
    std::vector<storm::storage::sparse::state_type> bsccRepresentatives;
    uint_fast64_t currentIndex = 0;
    for (auto const& bscc : bsccDecomposition) {
        // Since all states in an SCC can reach all other states, we only need to check whether an arbitrary
        // state is a maybe state.
        if (maybeStates.get(*bscc.cbegin())) {
            relevantBsccs.set(currentIndex);
            bsccRepresentatives.push_back(*bscc.cbegin());
            bsccRepresentativesAsBitVector.set(*bscc.cbegin(), true);
            for (auto const& state : bscc) {
                regularStatesInBsccs.set(state, true);
            }
        }
        ++currentIndex;
    }
    regularStatesInBsccs &= ~bsccRepresentativesAsBitVector;

    // Compute the average time to stay in each state for all states in BSCCs.
    std::vector<ValueType> averageTimeInStates(stateValues.size(), storm::utility::one<ValueType>());

    // First, we eliminate all states in BSCCs (except for the representative states).
    std::shared_ptr<StatePriorityQueue> priorityQueue =
        createStatePriorityQueue(distanceBasedPriorities, flexibleMatrix, flexibleBackwardTransitions, stateValues, regularStatesInBsccs);
    storm::solver::stateelimination::MultiValueStateEliminator<ValueType> stateEliminator(flexibleMatrix, flexibleBackwardTransitions, priorityQueue,
                                                                                          stateValues, averageTimeInStates);

    while (priorityQueue->hasNext()) {
        storm::storage::sparse::state_type state = priorityQueue->pop();
        stateEliminator.eliminateState(state, true);
#ifdef STORM_DEV
        STORM_LOG_ASSERT(checkConsistent(flexibleMatrix, flexibleBackwardTransitions), "The forward and backward transition matrices became inconsistent.");
#endif
    }

    // Now, we set the values of all states in BSCCs to that of the representative value (and clear the
    // transitions of the representative states while doing so).
    auto representativeIt = bsccRepresentatives.begin();
    for (auto sccIndex : relevantBsccs) {
        // We only need to set the values for all states of the BSCC if we are not computing the values for the
        // initial states only.
        ValueType bsccValue = stateValues[*representativeIt] / averageTimeInStates[*representativeIt];
        auto const& bscc = bsccDecomposition[sccIndex];
        if (!computeResultsForInitialStatesOnly) {
            for (auto const& state : bscc) {
                stateValues[state] = bsccValue;
            }
        } else {
            for (auto const& state : bscc) {
                stateValues[state] = storm::utility::zero<ValueType>();
            }
            stateValues[*representativeIt] = bsccValue;
        }

        FlexibleRowType& representativeForwardRow = flexibleMatrix.getRow(*representativeIt);
        representativeForwardRow.clear();
        representativeForwardRow.shrink_to_fit();

        FlexibleRowType& representativeBackwardRow = flexibleBackwardTransitions.getRow(*representativeIt);
        auto it = representativeBackwardRow.begin(), ite = representativeBackwardRow.end();
        for (; it != ite; ++it) {
            if (it->getColumn() == *representativeIt) {
                break;
            }
        }
        representativeBackwardRow.erase(it);

        ++representativeIt;
    }

    // If there are states remaining that are not in BSCCs, we need to eliminate them now.
    storm::storage::BitVector remainingStates = maybeStates & ~regularStatesInBsccs;

    // Set the value initial value of all states not in a BSCC to zero, because a) any previous value would
    // incorrectly influence the result and b) the value have been erroneously changed for the predecessors of
    // BSCCs by the previous state elimination.
    for (auto state : remainingStates) {
        if (!bsccRepresentativesAsBitVector.get(state)) {
            stateValues[state] = storm::utility::zero<ValueType>();
        }
    }

    // We only need to eliminate the remaining states if there was some BSCC that has a non-zero value, i.e.
    // that consists of maybe states.
    if (!relevantBsccs.empty()) {
        performOrdinaryStateElimination(flexibleMatrix, flexibleBackwardTransitions, remainingStates, initialStates, computeResultsForInitialStatesOnly,
                                        stateValues, distanceBasedPriorities);
    }

    std::chrono::high_resolution_clock::time_point modelCheckingEnd = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point totalTimeEnd = std::chrono::high_resolution_clock::now();

    if (storm::settings::getModule<storm::settings::modules::CoreSettings>().isShowStatisticsSet()) {
        std::chrono::high_resolution_clock::duration sccDecompositionTime = sccDecompositionEnd - sccDecompositionStart;
        std::chrono::milliseconds sccDecompositionTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(sccDecompositionTime);
        std::chrono::high_resolution_clock::duration conversionTime = conversionEnd - conversionStart;
        std::chrono::milliseconds conversionTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(conversionTime);
        std::chrono::high_resolution_clock::duration modelCheckingTime = modelCheckingEnd - modelCheckingStart;
        std::chrono::milliseconds modelCheckingTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(modelCheckingTime);
        std::chrono::high_resolution_clock::duration totalTime = totalTimeEnd - totalTimeStart;
        std::chrono::milliseconds totalTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(totalTime);

        STORM_PRINT_AND_LOG('\n');
        STORM_PRINT_AND_LOG("Time breakdown:\n");
        STORM_PRINT_AND_LOG("    * time for SCC decomposition: " << sccDecompositionTimeInMilliseconds.count() << "ms\n");
        STORM_PRINT_AND_LOG("    * time for conversion: " << conversionTimeInMilliseconds.count() << "ms\n");
        STORM_PRINT_AND_LOG("    * time for checking: " << modelCheckingTimeInMilliseconds.count() << "ms\n");
        STORM_PRINT_AND_LOG("------------------------------------------\n");
        STORM_PRINT_AND_LOG("    * total time: " << totalTimeInMilliseconds.count() << "ms\n");
    }

    // Now, we return the value for the only initial state.
    STORM_LOG_DEBUG("Simplifying and returning result.");
    for (auto& value : stateValues) {
        value = storm::utility::simplify(value);
    }
    return stateValues;
}

template<typename SparseDtmcModelType>
std::unique_ptr<CheckResult> SparseDtmcEliminationModelChecker<SparseDtmcModelType>::computeBoundedUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::BoundedUntilFormula, SolutionType> const& checkTask) {
    storm::logic::BoundedUntilFormula const& pathFormula = checkTask.getFormula();

    STORM_LOG_THROW(!pathFormula.hasLowerBound() && pathFormula.hasUpperBound(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to have single upper time bound.");
    STORM_LOG_THROW(pathFormula.hasIntegerUpperBound(), storm::exceptions::InvalidPropertyException, "Formula needs to have discrete upper time bound.");

    // Retrieve the appropriate bitvectors by model checking the subformulas.
    std::unique_ptr<CheckResult> leftResultPointer = this->check(pathFormula.getLeftSubformula());
    std::unique_ptr<CheckResult> rightResultPointer = this->check(pathFormula.getRightSubformula());
    storm::storage::BitVector const& phiStates = leftResultPointer->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
    storm::storage::BitVector const& psiStates = rightResultPointer->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();

    // Start by determining the states that have a non-zero probability of reaching the target states within the
    // time bound.
    storm::storage::BitVector statesWithProbabilityGreater0 = storm::utility::graph::performProbGreater0(
        this->getModel().getBackwardTransitions(), phiStates, psiStates, true, pathFormula.getUpperBound<uint64_t>());
    statesWithProbabilityGreater0 &= ~psiStates;

    // Determine whether we need to perform some further computation.
    bool furtherComputationNeeded = true;
    if (checkTask.isOnlyInitialStatesRelevantSet() && this->getModel().getInitialStates().isDisjointFrom(statesWithProbabilityGreater0)) {
        STORM_LOG_DEBUG("The probability for all initial states was found in a preprocessing step.");
        furtherComputationNeeded = false;
    } else if (statesWithProbabilityGreater0.empty()) {
        STORM_LOG_DEBUG("The probability for all states was found in a preprocessing step.");
        furtherComputationNeeded = false;
    }

    storm::storage::SparseMatrix<ValueType> const& transitionMatrix = this->getModel().getTransitionMatrix();
    storm::storage::BitVector const& initialStates = this->getModel().getInitialStates();

    std::vector<ValueType> result(transitionMatrix.getRowCount(), storm::utility::zero<ValueType>());

    if (furtherComputationNeeded) {
        uint_fast64_t timeBound = pathFormula.getUpperBound<uint64_t>();

        if (checkTask.isOnlyInitialStatesRelevantSet()) {
            // Determine the set of states that is reachable from the initial state without jumping over a target state.
            storm::storage::BitVector reachableStates =
                storm::utility::graph::getReachableStates(transitionMatrix, initialStates, phiStates, psiStates, true, timeBound);

            // Subtract from the maybe states the set of states that is not reachable (on a path from the initial to a target state).
            statesWithProbabilityGreater0 &= reachableStates;
        }

        // We then build the submatrix that only has the transitions of the maybe states.
        storm::storage::SparseMatrix<ValueType> submatrix =
            transitionMatrix.getSubmatrix(true, statesWithProbabilityGreater0, statesWithProbabilityGreater0, true);

        std::vector<uint_fast64_t> distancesFromInitialStates;
        storm::storage::BitVector relevantStates;
        if (checkTask.isOnlyInitialStatesRelevantSet()) {
            // Determine the set of initial states of the sub-model.
            storm::storage::BitVector subInitialStates = this->getModel().getInitialStates() % statesWithProbabilityGreater0;

            // Precompute the distances of the relevant states to the initial states.
            distancesFromInitialStates = storm::utility::graph::getDistances(submatrix, subInitialStates, statesWithProbabilityGreater0);

            // Set all states to be relevant for later use.
            relevantStates = storm::storage::BitVector(statesWithProbabilityGreater0.getNumberOfSetBits(), true);
        }

        // Create the vector of one-step probabilities to go to target states.
        std::vector<ValueType> b = transitionMatrix.getConstrainedRowSumVector(statesWithProbabilityGreater0, psiStates);

        // Create the vector with which to multiply.
        std::vector<ValueType> subresult(b);
        std::vector<ValueType> tmp(subresult.size());

        // Subtract one from the time bound because initializing the sub-result to b already accounts for one step.
        --timeBound;

        // Perform matrix-vector multiplications until the time-bound is met.
        for (uint_fast64_t timeStep = 0; timeStep < timeBound; ++timeStep) {
            submatrix.multiplyWithVector(subresult, tmp);
            storm::utility::vector::addVectors(tmp, b, subresult);

            // If we are computing the results for the initial states only, we can use the minimal distance from
            // each state to the initial states to determine whether we still need to consider the values for
            // these states. If not, we can null-out all their probabilities.
            if (checkTask.isOnlyInitialStatesRelevantSet()) {
                for (auto state : relevantStates) {
                    if (distancesFromInitialStates[state] > (timeBound - timeStep)) {
                        for (auto& element : submatrix.getRow(state)) {
                            element.setValue(storm::utility::zero<ValueType>());
                        }
                        b[state] = storm::utility::zero<ValueType>();
                        relevantStates.set(state, false);
                    }
                }
            }
        }

        // Set the values of the resulting vector accordingly.
        storm::utility::vector::setVectorValues(result, statesWithProbabilityGreater0, subresult);
    }
    storm::utility::vector::setVectorValues<ValueType>(result, psiStates, storm::utility::one<ValueType>());

    // Construct check result based on whether we have computed values for all states or just the initial states.
    std::unique_ptr<CheckResult> checkResult(new ExplicitQuantitativeCheckResult<ValueType>(result));
    if (checkTask.isOnlyInitialStatesRelevantSet()) {
        // If we computed the results for the initial (and prob 0 and prob1) states only, we need to filter the
        // result to only communicate these results.
        checkResult->filter(ExplicitQualitativeCheckResult<ValueType>(this->getModel().getInitialStates() | psiStates));
    }
    return checkResult;
}

template<typename SparseDtmcModelType>
std::unique_ptr<CheckResult> SparseDtmcEliminationModelChecker<SparseDtmcModelType>::computeUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::UntilFormula, SolutionType> const& checkTask) {
    storm::logic::UntilFormula const& pathFormula = checkTask.getFormula();

    // Retrieve the appropriate bitvectors by model checking the subformulas.
    std::unique_ptr<CheckResult> leftResultPointer = this->check(pathFormula.getLeftSubformula());
    std::unique_ptr<CheckResult> rightResultPointer = this->check(pathFormula.getRightSubformula());
    storm::storage::BitVector const& phiStates = leftResultPointer->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
    storm::storage::BitVector const& psiStates = rightResultPointer->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();

    return computeUntilProbabilities(this->getModel().getTransitionMatrix(), this->getModel().getBackwardTransitions(), this->getModel().getInitialStates(),
                                     phiStates, psiStates, checkTask.isOnlyInitialStatesRelevantSet());
}

template<typename SparseDtmcModelType>
std::unique_ptr<CheckResult> SparseDtmcEliminationModelChecker<SparseDtmcModelType>::computeUntilProbabilities(
    storm::storage::SparseMatrix<ValueType> const& probabilityMatrix, storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
    storm::storage::BitVector const& initialStates, storm::storage::BitVector const& phiStates, storm::storage::BitVector const& psiStates,
    bool computeForInitialStatesOnly) {
    // Then, compute the subset of states that has a probability of 0 or 1, respectively.
    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 =
        storm::utility::graph::performProb01(backwardTransitions, phiStates, psiStates);
    storm::storage::BitVector statesWithProbability0 = statesWithProbability01.first;
    storm::storage::BitVector statesWithProbability1 = statesWithProbability01.second;
    storm::storage::BitVector maybeStates = ~(statesWithProbability0 | statesWithProbability1);

    // Determine whether we need to perform some further computation.
    bool furtherComputationNeeded = true;
    if (computeForInitialStatesOnly && initialStates.isDisjointFrom(maybeStates)) {
        STORM_LOG_DEBUG("The probability for all initial states was found in a preprocessing step.");
        furtherComputationNeeded = false;
    } else if (maybeStates.empty()) {
        STORM_LOG_DEBUG("The probability for all states was found in a preprocessing step.");
        furtherComputationNeeded = false;
    }

    std::vector<ValueType> result(maybeStates.size());
    if (furtherComputationNeeded) {
        // If we compute the results for the initial states only, we can cut off all maybe state that are not
        // reachable from them.
        if (computeForInitialStatesOnly) {
            // Determine the set of states that is reachable from the initial state without jumping over a target state.
            storm::storage::BitVector reachableStates =
                storm::utility::graph::getReachableStates(probabilityMatrix, initialStates, maybeStates, statesWithProbability1);

            // Subtract from the maybe states the set of states that is not reachable (on a path from the initial to a target state).
            maybeStates &= reachableStates;
        }

        // Create a vector for the probabilities to go to a state with probability 1 in one step.
        std::vector<ValueType> oneStepProbabilities = probabilityMatrix.getConstrainedRowSumVector(maybeStates, statesWithProbability1);

        // Determine the set of initial states of the sub-model.
        storm::storage::BitVector newInitialStates = initialStates % maybeStates;

        // We then build the submatrix that only has the transitions of the maybe states.
        storm::storage::SparseMatrix<ValueType> submatrix = probabilityMatrix.getSubmatrix(false, maybeStates, maybeStates);
        storm::storage::SparseMatrix<ValueType> submatrixTransposed = submatrix.transpose();

        std::vector<ValueType> subresult = computeReachabilityValues(submatrix, oneStepProbabilities, submatrixTransposed, newInitialStates,
                                                                     computeForInitialStatesOnly, oneStepProbabilities);
        storm::utility::vector::setVectorValues<ValueType>(result, maybeStates, subresult);
    }

    // Construct full result.
    storm::utility::vector::setVectorValues<ValueType>(result, statesWithProbability0, storm::utility::zero<ValueType>());
    storm::utility::vector::setVectorValues<ValueType>(result, statesWithProbability1, storm::utility::one<ValueType>());

    if (computeForInitialStatesOnly) {
        // If we computed the results for the initial (and prob 0 and prob1) states only, we need to filter the
        // result to only communicate these results.
        std::unique_ptr<ExplicitQuantitativeCheckResult<ValueType>> checkResult = std::make_unique<ExplicitQuantitativeCheckResult<ValueType>>();
        for (auto state : ~maybeStates | initialStates) {
            (*checkResult)[state] = result[state];
        }
        return std::move(checkResult);  // move() required by, e.g., clang 3.8
    }
    return std::make_unique<ExplicitQuantitativeCheckResult<ValueType>>(result);
}

template<typename SparseDtmcModelType>
std::unique_ptr<CheckResult> SparseDtmcEliminationModelChecker<SparseDtmcModelType>::computeReachabilityRewards(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, SolutionType> const& checkTask) {
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();

    // Retrieve the appropriate bitvectors by model checking the subformulas.
    std::unique_ptr<CheckResult> subResultPointer = this->check(eventuallyFormula.getSubformula());
    storm::storage::BitVector trueStates(this->getModel().getNumberOfStates(), true);
    storm::storage::BitVector const& targetStates = subResultPointer->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();

    // Do some sanity checks to establish some required properties.
    RewardModelType const& rewardModel = this->getModel().getRewardModel(checkTask.isRewardModelSet() ? checkTask.getRewardModel() : "");

    STORM_LOG_THROW(!rewardModel.empty(), storm::exceptions::IllegalArgumentException, "Input model does not have a reward model.");
    return computeReachabilityRewards(
        this->getModel().getTransitionMatrix(), this->getModel().getBackwardTransitions(), this->getModel().getInitialStates(), targetStates,
        [&](uint_fast64_t numberOfRows, storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& maybeStates) {
            return rewardModel.getTotalRewardVector(numberOfRows, transitionMatrix, maybeStates);
        },
        checkTask.isOnlyInitialStatesRelevantSet());
}

template<typename SparseDtmcModelType>
std::unique_ptr<CheckResult> SparseDtmcEliminationModelChecker<SparseDtmcModelType>::computeReachabilityRewards(
    storm::storage::SparseMatrix<ValueType> const& probabilityMatrix, storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
    storm::storage::BitVector const& initialStates, storm::storage::BitVector const& targetStates, std::vector<ValueType>& stateRewardValues,
    bool computeForInitialStatesOnly) {
    return computeReachabilityRewards(
        probabilityMatrix, backwardTransitions, initialStates, targetStates,
        [&](uint_fast64_t numberOfRows, storm::storage::SparseMatrix<ValueType> const&, storm::storage::BitVector const& maybeStates) {
            std::vector<ValueType> result(numberOfRows);
            storm::utility::vector::selectVectorValues(result, maybeStates, stateRewardValues);
            return result;
        },
        computeForInitialStatesOnly);
}

template<typename SparseDtmcModelType>
std::unique_ptr<CheckResult> SparseDtmcEliminationModelChecker<SparseDtmcModelType>::computeReachabilityRewards(
    storm::storage::SparseMatrix<ValueType> const& probabilityMatrix, storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
    storm::storage::BitVector const& initialStates, storm::storage::BitVector const& targetStates,
    std::function<std::vector<ValueType>(uint_fast64_t, storm::storage::SparseMatrix<ValueType> const&, storm::storage::BitVector const&)> const&
        totalStateRewardVectorGetter,
    bool computeForInitialStatesOnly) {
    uint_fast64_t numberOfStates = probabilityMatrix.getRowCount();

    // Compute the subset of states that has a reachability reward less than infinity.
    storm::storage::BitVector trueStates(numberOfStates, true);
    storm::storage::BitVector infinityStates = storm::utility::graph::performProb1(backwardTransitions, trueStates, targetStates);
    infinityStates.complement();
    storm::storage::BitVector maybeStates = ~targetStates & ~infinityStates;

    // Determine whether we need to perform some further computation.
    bool furtherComputationNeeded = true;
    if (computeForInitialStatesOnly) {
        if (initialStates.isSubsetOf(infinityStates)) {
            STORM_LOG_DEBUG("The reward of all initial states was found in a preprocessing step.");
            furtherComputationNeeded = false;
        }
        if (initialStates.isSubsetOf(targetStates)) {
            STORM_LOG_DEBUG("The reward of all initial states was found in a preprocessing step.");
            furtherComputationNeeded = false;
        }
    }

    std::vector<ValueType> result(maybeStates.size());
    if (furtherComputationNeeded) {
        // If we compute the results for the initial states only, we can cut off all maybe state that are not
        // reachable from them.
        if (computeForInitialStatesOnly) {
            // Determine the set of states that is reachable from the initial state without jumping over a target state.
            storm::storage::BitVector reachableStates = storm::utility::graph::getReachableStates(probabilityMatrix, initialStates, maybeStates, targetStates);

            // Subtract from the maybe states the set of states that is not reachable (on a path from the initial to a target state).
            maybeStates &= reachableStates;
        }

        // Determine the set of initial states of the sub-model.
        storm::storage::BitVector newInitialStates = initialStates % maybeStates;

        // We then build the submatrix that only has the transitions of the maybe states.
        storm::storage::SparseMatrix<ValueType> submatrix = probabilityMatrix.getSubmatrix(false, maybeStates, maybeStates);
        storm::storage::SparseMatrix<ValueType> submatrixTransposed = submatrix.transpose();

        // Project the state reward vector to all maybe-states.
        std::vector<ValueType> stateRewardValues = totalStateRewardVectorGetter(submatrix.getRowCount(), probabilityMatrix, maybeStates);

        std::vector<ValueType> subresult =
            computeReachabilityValues(submatrix, stateRewardValues, submatrixTransposed, newInitialStates, computeForInitialStatesOnly,
                                      probabilityMatrix.getConstrainedRowSumVector(maybeStates, targetStates));
        storm::utility::vector::setVectorValues<ValueType>(result, maybeStates, subresult);
    }

    // Construct full result.
    storm::utility::vector::setVectorValues<ValueType>(result, infinityStates, storm::utility::infinity<ValueType>());
    storm::utility::vector::setVectorValues<ValueType>(result, targetStates, storm::utility::zero<ValueType>());
    if (computeForInitialStatesOnly) {
        // If we computed the results for the initial (and inf) states only, we need to filter the result to
        // only communicate these results.
        std::unique_ptr<ExplicitQuantitativeCheckResult<ValueType>> checkResult = std::make_unique<ExplicitQuantitativeCheckResult<ValueType>>();
        for (auto state : ~maybeStates | initialStates) {
            (*checkResult)[state] = result[state];
        }
        return std::move(checkResult);  // move() required by, e.g., clang 3.8
    }
    return std::make_unique<ExplicitQuantitativeCheckResult<ValueType>>(result);
}

// GCC 15 raises a false positive -Wfree-nonheap-object when compiling SparseDtmcEliminationModelChecker
// with GMP rational functions (STORM_USE_CLN_RF=OFF) in Release mode: the inlined std::vector destructor
// for MatrixEntry<..., RationalFunction<..., GMP>> is incorrectly flagged. Not a real memory error.
// See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=108846
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
template<typename SparseDtmcModelType>
std::unique_ptr<CheckResult> SparseDtmcEliminationModelChecker<SparseDtmcModelType>::computeConditionalProbabilities(
    Environment const& env, CheckTask<storm::logic::ConditionalFormula, SolutionType> const& checkTask) {
    storm::logic::ConditionalFormula const& conditionalFormula = checkTask.getFormula();

    // Retrieve the appropriate bitvectors by model checking the subformulas.
    STORM_LOG_THROW(conditionalFormula.getSubformula().isEventuallyFormula(), storm::exceptions::InvalidPropertyException, "Expected 'eventually' formula.");
    STORM_LOG_THROW(conditionalFormula.getConditionFormula().isEventuallyFormula(), storm::exceptions::InvalidPropertyException,
                    "Expected 'eventually' formula.");

    std::unique_ptr<CheckResult> leftResultPointer = this->check(conditionalFormula.getSubformula().asEventuallyFormula().getSubformula());
    std::unique_ptr<CheckResult> rightResultPointer = this->check(conditionalFormula.getConditionFormula().asEventuallyFormula().getSubformula());
    storm::storage::BitVector phiStates = leftResultPointer->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
    storm::storage::BitVector psiStates = rightResultPointer->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
    storm::storage::BitVector trueStates(this->getModel().getNumberOfStates(), true);

    // Do some sanity checks to establish some required properties.
    // STORM_LOG_WARN_COND(storm::settings::getModule<storm::settings::modules::EliminationSettings>().getEliminationMethod() ==
    // storm::settings::modules::EliminationSettings::EliminationMethod::State, "The chosen elimination method is not available for computing conditional
    // probabilities. Falling back to regular state elimination.");
    STORM_LOG_THROW(this->getModel().getInitialStates().getNumberOfSetBits() == 1, storm::exceptions::IllegalArgumentException,
                    "Input model is required to have exactly one initial state.");
    STORM_LOG_THROW(checkTask.isOnlyInitialStatesRelevantSet(), storm::exceptions::IllegalArgumentException,
                    "Cannot compute conditional probabilities for all states.");
    storm::storage::sparse::state_type initialState = *this->getModel().getInitialStates().begin();

    storm::storage::SparseMatrix<ValueType> backwardTransitions = this->getModel().getBackwardTransitions();

    // Compute the 'true' psi states, i.e. those psi states that can be reached without passing through another psi state first.
    psiStates = storm::utility::graph::getReachableStates(this->getModel().getTransitionMatrix(), this->getModel().getInitialStates(), trueStates, psiStates) &
                psiStates;

    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 =
        storm::utility::graph::performProb01(backwardTransitions, trueStates, psiStates);
    storm::storage::BitVector statesWithProbabilityGreater0 = ~statesWithProbability01.first;
    storm::storage::BitVector statesWithProbability1 = std::move(statesWithProbability01.second);

    STORM_LOG_THROW(this->getModel().getInitialStates().isSubsetOf(statesWithProbabilityGreater0), storm::exceptions::InvalidPropertyException,
                    "The condition of the conditional probability has zero probability.");

    // If the initial state is known to have probability 1 of satisfying the condition, we can apply regular model checking.
    if (this->getModel().getInitialStates().isSubsetOf(statesWithProbability1)) {
        STORM_LOG_INFO("The condition holds with probability 1, so the regular reachability probability is computed.");
        std::shared_ptr<storm::logic::BooleanLiteralFormula> trueFormula = std::make_shared<storm::logic::BooleanLiteralFormula>(true);
        std::shared_ptr<storm::logic::UntilFormula> untilFormula =
            std::make_shared<storm::logic::UntilFormula>(trueFormula, conditionalFormula.getSubformula().asSharedPointer());
        return this->computeUntilProbabilities(env, *untilFormula);
    }

    // From now on, we know the condition does not have a trivial probability in the initial state.

    // Compute the states that can be reached on a path that has a psi state in it.
    storm::storage::BitVector statesWithPsiPredecessor =
        storm::utility::graph::performProbGreater0(this->getModel().getTransitionMatrix(), trueStates, psiStates);
    storm::storage::BitVector statesReachingPhi = storm::utility::graph::performProbGreater0(backwardTransitions, trueStates, phiStates);

    // The set of states we need to consider are those that have a non-zero probability to satisfy the condition or are on some path that has a psi state in it.
    storm::storage::BitVector maybeStates = statesWithProbabilityGreater0 | (statesWithPsiPredecessor & statesReachingPhi);

    // Determine the set of initial states of the sub-DTMC.
    storm::storage::BitVector newInitialStates = this->getModel().getInitialStates() % maybeStates;

    // Create a dummy vector for the one-step probabilities.
    std::vector<ValueType> oneStepProbabilities(maybeStates.getNumberOfSetBits(), storm::utility::zero<ValueType>());

    // We then build the submatrix that only has the transitions of the maybe states.
    storm::storage::SparseMatrix<ValueType> submatrix = this->getModel().getTransitionMatrix().getSubmatrix(false, maybeStates, maybeStates);
    storm::storage::SparseMatrix<ValueType> submatrixTransposed = submatrix.transpose();

    // The states we want to eliminate are those that are tagged with "maybe" but are not a phi or psi state.
    phiStates = phiStates % maybeStates;

    // If there are no phi states in the reduced model, the conditional probability is trivially zero.
    if (phiStates.empty()) {
        return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(initialState, storm::utility::zero<ValueType>()));
    }

    psiStates = psiStates % maybeStates;

    // Keep only the states that we do not eliminate in the maybe states.
    maybeStates = phiStates | psiStates;

    storm::storage::BitVector statesToEliminate = ~maybeStates & ~newInitialStates;

    // Before starting the model checking process, we assign priorities to states so we can use them to
    // impose ordering constraints later.
    boost::optional<std::vector<uint_fast64_t>> distanceBasedPriorities;
    storm::settings::modules::EliminationSettings::EliminationOrder order =
        storm::settings::getModule<storm::settings::modules::EliminationSettings>().getEliminationOrder();
    if (eliminationOrderNeedsDistances(order)) {
        distanceBasedPriorities = getDistanceBasedPriorities(submatrix, submatrixTransposed, newInitialStates, oneStepProbabilities,
                                                             eliminationOrderNeedsForwardDistances(order), eliminationOrderNeedsReversedDistances(order));
    }

    storm::storage::FlexibleSparseMatrix<ValueType> flexibleMatrix(submatrix);
    storm::storage::FlexibleSparseMatrix<ValueType> flexibleBackwardTransitions(submatrixTransposed, true);

    std::shared_ptr<StatePriorityQueue> statePriorities =
        createStatePriorityQueue(distanceBasedPriorities, flexibleMatrix, flexibleBackwardTransitions, oneStepProbabilities, statesToEliminate);

    STORM_LOG_INFO("Computing conditional probilities.\n");
    uint_fast64_t numberOfStatesToEliminate = statePriorities->size();
    STORM_LOG_INFO("Eliminating " << numberOfStatesToEliminate << " states using the state elimination technique.\n");
    performPrioritizedStateElimination(statePriorities, flexibleMatrix, flexibleBackwardTransitions, oneStepProbabilities, this->getModel().getInitialStates(),
                                       true);

    storm::solver::stateelimination::ConditionalStateEliminator<ValueType> stateEliminator =
        storm::solver::stateelimination::ConditionalStateEliminator<ValueType>(flexibleMatrix, flexibleBackwardTransitions, oneStepProbabilities, phiStates,
                                                                               psiStates);

    // Eliminate the transitions going into the initial state (if there are any).
    if (!flexibleBackwardTransitions.getRow(*newInitialStates.begin()).empty()) {
        stateEliminator.eliminateState(*newInitialStates.begin(), false);
    }

    // Now we need to basically eliminate all chains of not-psi states after phi states and chains of not-phi
    // states after psi states.
    for (auto const& trans1 : flexibleMatrix.getRow(*newInitialStates.begin())) {
        auto initialStateSuccessor = trans1.getColumn();

        STORM_LOG_TRACE("Exploring successor " << initialStateSuccessor << " of the initial state.");

        if (phiStates.get(initialStateSuccessor)) {
            STORM_LOG_TRACE("Is a phi state.");

            // If the state is both a phi and a psi state, we do not need to eliminate chains.
            if (psiStates.get(initialStateSuccessor)) {
                continue;
            }

            // At this point, we know that the state satisfies phi and not psi.
            // This means, we must compute the probability to reach psi states, which in turn means that we need
            // to eliminate all chains of non-psi states between the current state and psi states.
            bool hasNonPsiSuccessor = true;
            while (hasNonPsiSuccessor) {
                stateEliminator.setFilterPhi();
                hasNonPsiSuccessor = false;

                // Only treat the state if it has an outgoing transition other than a self-loop.
                auto const currentRow = flexibleMatrix.getRow(initialStateSuccessor);
                if (currentRow.size() > 1 || (!currentRow.empty() && currentRow.front().getColumn() != initialStateSuccessor)) {
                    for (auto const& element : currentRow) {
                        // If any of the successors is a phi state, we eliminate it (wrt. all its phi predecessors).
                        if (!psiStates.get(element.getColumn())) {
                            FlexibleRowType const& successorRow = flexibleMatrix.getRow(element.getColumn());
                            // Eliminate the successor only if there possibly is a psi state reachable through it.
                            if (successorRow.size() > 1 || (!successorRow.empty() && successorRow.front().getColumn() != element.getColumn())) {
                                STORM_LOG_TRACE("Found non-psi successor " << element.getColumn() << " that needs to be eliminated.");
                                stateEliminator.eliminateState(element.getColumn(), false);
                                hasNonPsiSuccessor = true;
                            }
                        }
                    }
                    STORM_LOG_ASSERT(!flexibleMatrix.getRow(initialStateSuccessor).empty(), "(1) New transitions expected to be non-empty.");
                }
            }
            stateEliminator.unsetFilter();
        } else {
            STORM_LOG_ASSERT(psiStates.get(initialStateSuccessor), "Expected psi state.");
            STORM_LOG_TRACE("Is a psi state.");

            // At this point, we know that the state satisfies psi and not phi.
            // This means, we must compute the probability to reach phi states, which in turn means that we need
            // to eliminate all chains of non-phi states between the current state and phi states.

            bool hasNonPhiSuccessor = true;
            while (hasNonPhiSuccessor) {
                stateEliminator.setFilterPsi();
                hasNonPhiSuccessor = false;

                // Only treat the state if it has an outgoing transition other than a self-loop.
                auto const currentRow = flexibleMatrix.getRow(initialStateSuccessor);
                if (currentRow.size() > 1 || (!currentRow.empty() && currentRow.front().getColumn() != initialStateSuccessor)) {
                    for (auto const& element : currentRow) {
                        // If any of the successors is a psi state, we eliminate it (wrt. all its psi predecessors).
                        if (!phiStates.get(element.getColumn())) {
                            FlexibleRowType const& successorRow = flexibleMatrix.getRow(element.getColumn());
                            if (successorRow.size() > 1 || (!successorRow.empty() && successorRow.front().getColumn() != element.getColumn())) {
                                STORM_LOG_TRACE("Found non-phi successor " << element.getColumn() << " that needs to be eliminated.");
                                stateEliminator.eliminateState(element.getColumn(), false);
                                hasNonPhiSuccessor = true;
                            }
                        }
                    }
                }
            }
            stateEliminator.unsetFilter();
        }
    }

    ValueType numerator = storm::utility::zero<ValueType>();
    ValueType denominator = storm::utility::zero<ValueType>();

    for (auto const& trans1 : flexibleMatrix.getRow(*newInitialStates.begin())) {
        auto initialStateSuccessor = trans1.getColumn();
        if (phiStates.get(initialStateSuccessor)) {
            if (psiStates.get(initialStateSuccessor)) {
                numerator += trans1.getValue();
                denominator += trans1.getValue();
            } else {
                ValueType additiveTerm = storm::utility::zero<ValueType>();
                for (auto const& trans2 : flexibleMatrix.getRow(initialStateSuccessor)) {
                    if (psiStates.get(trans2.getColumn())) {
                        additiveTerm += trans2.getValue();
                    }
                }
                additiveTerm *= trans1.getValue();
                numerator += additiveTerm;
                denominator += additiveTerm;
            }
        } else {
            STORM_LOG_ASSERT(psiStates.get(initialStateSuccessor), "Expected psi state.");
            denominator += trans1.getValue();
            ValueType additiveTerm = storm::utility::zero<ValueType>();
            for (auto const& trans2 : flexibleMatrix.getRow(initialStateSuccessor)) {
                if (phiStates.get(trans2.getColumn())) {
                    additiveTerm += trans2.getValue();
                }
            }
            numerator += trans1.getValue() * additiveTerm;
        }
    }

    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(initialState, numerator / denominator));
}

template<typename SparseDtmcModelType>
void SparseDtmcEliminationModelChecker<SparseDtmcModelType>::performPrioritizedStateElimination(
    std::shared_ptr<StatePriorityQueue>& priorityQueue, storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
    storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions, std::vector<ValueType>& values, storm::storage::BitVector const& initialStates,
    bool computeResultsForInitialStatesOnly) {
    storm::solver::stateelimination::PrioritizedStateEliminator<ValueType> stateEliminator(transitionMatrix, backwardTransitions, priorityQueue, values);

    while (priorityQueue->hasNext()) {
        storm::storage::sparse::state_type state = priorityQueue->pop();
        bool removeForwardTransitions = computeResultsForInitialStatesOnly && !initialStates.get(state);
        stateEliminator.eliminateState(state, removeForwardTransitions);
        if (removeForwardTransitions) {
            values[state] = storm::utility::zero<ValueType>();
        }
#ifdef STORM_DEV
        STORM_LOG_ASSERT(checkConsistent(transitionMatrix, backwardTransitions), "The forward and backward transition matrices became inconsistent.");
#endif
    }
}
#pragma GCC diagnostic pop

template<typename SparseDtmcModelType>
void SparseDtmcEliminationModelChecker<SparseDtmcModelType>::performOrdinaryStateElimination(
    storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix, storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions,
    storm::storage::BitVector const& subsystem, storm::storage::BitVector const& initialStates, bool computeResultsForInitialStatesOnly,
    std::vector<ValueType>& values, boost::optional<std::vector<uint_fast64_t>> const& distanceBasedPriorities) {
    std::shared_ptr<StatePriorityQueue> statePriorities =
        createStatePriorityQueue(distanceBasedPriorities, transitionMatrix, backwardTransitions, values, subsystem);

    std::size_t numberOfStatesToEliminate = statePriorities->size();
    STORM_LOG_DEBUG("Eliminating " << numberOfStatesToEliminate << " states using the state elimination technique.\n");
    performPrioritizedStateElimination(statePriorities, transitionMatrix, backwardTransitions, values, initialStates, computeResultsForInitialStatesOnly);
    STORM_LOG_DEBUG("Eliminated " << numberOfStatesToEliminate << " states.\n");
}

template<typename SparseDtmcModelType>
uint_fast64_t SparseDtmcEliminationModelChecker<SparseDtmcModelType>::performHybridStateElimination(
    storm::storage::SparseMatrix<ValueType> const& forwardTransitions, storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
    storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions, storm::storage::BitVector const& subsystem,
    storm::storage::BitVector const& initialStates, bool computeResultsForInitialStatesOnly, std::vector<ValueType>& values,
    boost::optional<std::vector<uint_fast64_t>> const& distanceBasedPriorities) {
    // When using the hybrid technique, we recursively treat the SCCs up to some size.
    std::vector<storm::storage::sparse::state_type> entryStateQueue;
    STORM_LOG_DEBUG("Eliminating " << subsystem.size() << " states using the hybrid elimination technique.\n");
    uint_fast64_t maximalDepth = treatScc(transitionMatrix, values, initialStates, subsystem, initialStates, forwardTransitions, backwardTransitions, false, 0,
                                          storm::settings::getModule<storm::settings::modules::EliminationSettings>().getMaximalSccSize(), entryStateQueue,
                                          computeResultsForInitialStatesOnly, distanceBasedPriorities);

    // If the entry states were to be eliminated last, we need to do so now.
    if (storm::settings::getModule<storm::settings::modules::EliminationSettings>().isEliminateEntryStatesLastSet()) {
        STORM_LOG_DEBUG("Eliminating " << entryStateQueue.size() << " entry states as a last step.");
        std::vector<storm::storage::sparse::state_type> sortedStates(entryStateQueue.begin(), entryStateQueue.end());
        std::shared_ptr<StatePriorityQueue> queuePriorities = std::make_shared<StaticStatePriorityQueue>(sortedStates);
        performPrioritizedStateElimination(queuePriorities, transitionMatrix, backwardTransitions, values, initialStates, computeResultsForInitialStatesOnly);
    }
    STORM_LOG_DEBUG("Eliminated " << subsystem.size() << " states.\n");
    return maximalDepth;
}

template<typename SparseDtmcModelType>
std::vector<typename SparseDtmcEliminationModelChecker<SparseDtmcModelType>::ValueType>
SparseDtmcEliminationModelChecker<SparseDtmcModelType>::computeReachabilityValues(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                  std::vector<ValueType>& values,
                                                                                  storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                  storm::storage::BitVector const& initialStates,
                                                                                  bool computeResultsForInitialStatesOnly,
                                                                                  std::vector<ValueType> const& oneStepProbabilitiesToTarget) {
    // Then, we convert the reduced matrix to a more flexible format to be able to perform state elimination more easily.
    storm::storage::FlexibleSparseMatrix<ValueType> flexibleMatrix(transitionMatrix);
    storm::storage::FlexibleSparseMatrix<ValueType> flexibleBackwardTransitions(backwardTransitions);

    storm::settings::modules::EliminationSettings::EliminationOrder order =
        storm::settings::getModule<storm::settings::modules::EliminationSettings>().getEliminationOrder();
    boost::optional<std::vector<uint_fast64_t>> distanceBasedPriorities;
    if (eliminationOrderNeedsDistances(order)) {
        distanceBasedPriorities = getDistanceBasedPriorities(transitionMatrix, backwardTransitions, initialStates, oneStepProbabilitiesToTarget,
                                                             eliminationOrderNeedsForwardDistances(order), eliminationOrderNeedsReversedDistances(order));
    }

    // Create a bit vector that represents the subsystem of states we still have to eliminate.
    storm::storage::BitVector subsystem = storm::storage::BitVector(transitionMatrix.getRowCount(), true);

    if (storm::settings::getModule<storm::settings::modules::EliminationSettings>().getEliminationMethod() ==
        storm::settings::modules::EliminationSettings::EliminationMethod::State) {
        performOrdinaryStateElimination(flexibleMatrix, flexibleBackwardTransitions, subsystem, initialStates, computeResultsForInitialStatesOnly, values,
                                        distanceBasedPriorities);
    } else if (storm::settings::getModule<storm::settings::modules::EliminationSettings>().getEliminationMethod() ==
               storm::settings::modules::EliminationSettings::EliminationMethod::Hybrid) {
        uint64_t maximalDepth = performHybridStateElimination(transitionMatrix, flexibleMatrix, flexibleBackwardTransitions, subsystem, initialStates,
                                                              computeResultsForInitialStatesOnly, values, distanceBasedPriorities);
        STORM_LOG_TRACE("Maximal depth of decomposition was " << maximalDepth << ".");
    }

    STORM_LOG_ASSERT(flexibleMatrix.empty(), "Not all transitions were eliminated.");
    STORM_LOG_ASSERT(flexibleBackwardTransitions.empty(), "Not all transitions were eliminated.");

    // Now, we return the value for the only initial state.
    STORM_LOG_DEBUG("Simplifying and returning result.");
    for (auto& value : values) {
        value = storm::utility::simplify(value);
    }
    return values;
}

template<typename SparseDtmcModelType>
uint_fast64_t SparseDtmcEliminationModelChecker<SparseDtmcModelType>::treatScc(
    storm::storage::FlexibleSparseMatrix<ValueType>& matrix, std::vector<ValueType>& values, storm::storage::BitVector const& entryStates,
    storm::storage::BitVector const& scc, storm::storage::BitVector const& initialStates, storm::storage::SparseMatrix<ValueType> const& forwardTransitions,
    storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions, bool eliminateEntryStates, uint_fast64_t level, uint_fast64_t maximalSccSize,
    std::vector<storm::storage::sparse::state_type>& entryStateQueue, bool computeResultsForInitialStatesOnly,
    boost::optional<std::vector<uint_fast64_t>> const& distanceBasedPriorities) {
    uint_fast64_t maximalDepth = level;

    // If the SCCs are large enough, we try to split them further.
    if (scc.getNumberOfSetBits() > maximalSccSize) {
        STORM_LOG_TRACE("SCC is large enough (" << scc.getNumberOfSetBits() << " states) to be decomposed further.");

        // Here, we further decompose the SCC into sub-SCCs.
        storm::storage::BitVector nonEntrySccStates = scc & ~entryStates;
        storm::storage::StronglyConnectedComponentDecomposition<ValueType> decomposition(
            forwardTransitions, storm::storage::StronglyConnectedComponentDecompositionOptions().subsystem(nonEntrySccStates));
        STORM_LOG_TRACE("Decomposed SCC into " << decomposition.size() << " sub-SCCs.");

        // Store a bit vector of remaining SCCs so we can be flexible when it comes to the order in which
        // we eliminate the SCCs.
        storm::storage::BitVector remainingSccs(decomposition.size(), true);

        // First, get rid of the trivial SCCs.
        storm::storage::BitVector statesInTrivialSccs(matrix.getRowCount());
        for (uint_fast64_t sccIndex = 0; sccIndex < decomposition.size(); ++sccIndex) {
            storm::storage::StronglyConnectedComponent const& scc = decomposition.getBlock(sccIndex);
            if (scc.isTrivial()) {
                // Put the only state of the trivial SCC into the set of states to eliminate.
                statesInTrivialSccs.set(*scc.begin(), true);
                remainingSccs.set(sccIndex, false);
            }
        }

        std::shared_ptr<StatePriorityQueue> statePriorities =
            createStatePriorityQueue(distanceBasedPriorities, matrix, backwardTransitions, values, statesInTrivialSccs);
        STORM_LOG_TRACE("Eliminating " << statePriorities->size() << " trivial SCCs.");
        performPrioritizedStateElimination(statePriorities, matrix, backwardTransitions, values, initialStates, computeResultsForInitialStatesOnly);
        STORM_LOG_TRACE("Eliminated all trivial SCCs.");

        // And then recursively treat the remaining sub-SCCs.
        STORM_LOG_TRACE("Eliminating " << remainingSccs.getNumberOfSetBits() << " remaining SCCs on level " << level << ".");
        for (auto sccIndex : remainingSccs) {
            storm::storage::StronglyConnectedComponent const& newScc = decomposition.getBlock(sccIndex);

            // Rewrite SCC into bit vector and subtract it from the remaining states.
            storm::storage::BitVector newSccAsBitVector(forwardTransitions.getRowCount(), newScc.begin(), newScc.end());

            // Determine the set of entry states of the SCC.
            storm::storage::BitVector entryStates(forwardTransitions.getRowCount());
            for (auto const& state : newScc) {
                for (auto const& predecessor : backwardTransitions.getRow(state)) {
                    if (predecessor.getValue() != storm::utility::zero<ValueType>() && !newSccAsBitVector.get(predecessor.getColumn())) {
                        entryStates.set(state);
                    }
                }
            }

            // Recursively descend in SCC-hierarchy.
            uint_fast64_t depth =
                treatScc(matrix, values, entryStates, newSccAsBitVector, initialStates, forwardTransitions, backwardTransitions,
                         eliminateEntryStates || !storm::settings::getModule<storm::settings::modules::EliminationSettings>().isEliminateEntryStatesLastSet(),
                         level + 1, maximalSccSize, entryStateQueue, computeResultsForInitialStatesOnly, distanceBasedPriorities);
            maximalDepth = std::max(maximalDepth, depth);
        }
    } else {
        // In this case, we perform simple state elimination in the current SCC.
        STORM_LOG_TRACE("SCC of size " << scc.getNumberOfSetBits() << " is small enough to be eliminated directly.");
        std::shared_ptr<StatePriorityQueue> statePriorities =
            createStatePriorityQueue(distanceBasedPriorities, matrix, backwardTransitions, values, scc & ~entryStates);
        performPrioritizedStateElimination(statePriorities, matrix, backwardTransitions, values, initialStates, computeResultsForInitialStatesOnly);
        STORM_LOG_TRACE("Eliminated all states of SCC.");
    }

    // Finally, eliminate the entry states (if we are required to do so).
    if (eliminateEntryStates) {
        STORM_LOG_TRACE("Finally, eliminating entry states.");
        std::shared_ptr<StatePriorityQueue> naivePriorities = createStatePriorityQueue(entryStates);
        performPrioritizedStateElimination(naivePriorities, matrix, backwardTransitions, values, initialStates, computeResultsForInitialStatesOnly);
        STORM_LOG_TRACE("Eliminated/added entry states.");
    } else {
        STORM_LOG_TRACE("Finally, adding entry states to queue.");
        for (auto state : entryStates) {
            entryStateQueue.push_back(state);
        }
    }

    return maximalDepth;
}

template<typename SparseDtmcModelType>
bool SparseDtmcEliminationModelChecker<SparseDtmcModelType>::checkConsistent(storm::storage::FlexibleSparseMatrix<ValueType>& transitionMatrix,
                                                                             storm::storage::FlexibleSparseMatrix<ValueType>& backwardTransitions) {
    for (uint_fast64_t forwardIndex = 0; forwardIndex < transitionMatrix.getRowCount(); ++forwardIndex) {
        for (auto const& forwardEntry : transitionMatrix.getRow(forwardIndex)) {
            if (forwardEntry.getColumn() == forwardIndex) {
                continue;
            }

            bool foundCorrespondingElement = false;
            for (auto const& backwardEntry : backwardTransitions.getRow(forwardEntry.getColumn())) {
                if (backwardEntry.getColumn() == forwardIndex) {
                    foundCorrespondingElement = true;
                }
            }

            if (!foundCorrespondingElement) {
                return false;
            }
        }
    }
    return true;
}

template class SparseDtmcEliminationModelChecker<storm::models::sparse::Dtmc<double>>;

template class SparseDtmcEliminationModelChecker<storm::models::sparse::Dtmc<storm::RationalNumber>>;
template class SparseDtmcEliminationModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>>;
}  // namespace modelchecker
}  // namespace storm
