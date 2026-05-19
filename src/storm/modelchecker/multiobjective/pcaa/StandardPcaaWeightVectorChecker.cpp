#include "storm/modelchecker/multiobjective/pcaa/StandardPcaaWeightVectorChecker.h"

#include <map>
#include <set>

#include "storm/environment/solver/LongRunAverageSolverEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UncheckedRequirementException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/multiobjective/preprocessing/SparseMultiObjectiveRewardAnalysis.h"
#include "storm/modelchecker/prctl/helper/BaierUpperRewardBoundsComputer.h"
#include "storm/modelchecker/prctl/helper/DsMpiUpperRewardBoundsComputer.h"
#include "storm/modelchecker/prctl/helper/SparseDtmcPrctlHelper.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/transformer/GoalStateMerger.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

template<class SparseModelType>
StandardPcaaWeightVectorChecker<SparseModelType>::StandardPcaaWeightVectorChecker(
    preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType> const& preprocessorResult)
    : PcaaWeightVectorChecker<SparseModelType>(preprocessorResult.objectives) {
    // Intantionally left empty
}

template<class SparseModelType>
void StandardPcaaWeightVectorChecker<SparseModelType>::initialize(
    preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType> const& preprocessorResult) {
    auto rewardAnalysis = preprocessing::SparseMultiObjectiveRewardAnalysis<SparseModelType>::analyze(preprocessorResult);
    STORM_LOG_THROW(rewardAnalysis.rewardFinitenessType != preprocessing::RewardFinitenessType::Infinite, storm::exceptions::NotSupportedException,
                    "There is no Pareto optimal scheduler that yields finite reward for all objectives. This is not supported.");
    STORM_LOG_WARN_COND(rewardAnalysis.rewardFinitenessType == preprocessing::RewardFinitenessType::AllFinite,
                        "There might be infinite reward for some scheduler. Multi-objective model checking restricts to schedulers that yield finite reward "
                        "for all objectives. Be aware that solutions yielding infinite reward are discarded.");
    STORM_LOG_THROW(rewardAnalysis.totalRewardLessInfinityEStates, storm::exceptions::UnexpectedException,
                    "The set of states with reward < infinity for some scheduler has not been computed during preprocessing.");
    STORM_LOG_THROW(!preprocessorResult.containsRewardBoundedObjective(), storm::exceptions::NotSupportedException,
                    "At least one objective was not reduced to an expected (long run, total or cumulative) reward objective during preprocessing. This is not "
                    "supported by the considered weight vector checker.");
    STORM_LOG_THROW(preprocessorResult.preprocessedModel->getInitialStates().getNumberOfSetBits() == 1, storm::exceptions::NotSupportedException,
                    "The model has multiple initial states.");

    // Build a subsystem of the preprocessor result model that discards states that yield infinite reward for all schedulers.
    // We can also merge the states that will have reward zero anyway.
    storm::storage::BitVector maybeStates = rewardAnalysis.totalRewardLessInfinityEStates.get() & ~rewardAnalysis.reward0AStates;
    storm::storage::BitVector finiteTotalRewardChoices = preprocessorResult.preprocessedModel->getTransitionMatrix().getRowFilter(
        rewardAnalysis.totalRewardLessInfinityEStates.get(), rewardAnalysis.totalRewardLessInfinityEStates.get());
    std::set<std::string> relevantRewardModels;
    for (auto const& obj : this->objectives) {
        obj.formula->gatherReferencedRewardModels(relevantRewardModels);
    }
    storm::transformer::GoalStateMerger<SparseModelType> merger(*preprocessorResult.preprocessedModel);
    auto mergerResult =
        merger.mergeTargetAndSinkStates(maybeStates, rewardAnalysis.reward0AStates, storm::storage::BitVector(maybeStates.size(), false),
                                        std::vector<std::string>(relevantRewardModels.begin(), relevantRewardModels.end()), finiteTotalRewardChoices);
    goalStateMergerInputToReducedStateIndexMapping = std::move(mergerResult.oldToNewStateIndexMapping);
    goalStateMergerReducedToInputChoiceMapping = mergerResult.keptChoices.getNumberOfSetBitsBeforeIndices();
    // Initialize data specific for the considered model type
    initializeModelTypeSpecificData(*mergerResult.model);

    // Initilize general data of the model
    transitionMatrix = std::move(mergerResult.model->getTransitionMatrix());
    initialState = *mergerResult.model->getInitialStates().begin();
    totalReward0EStates = rewardAnalysis.totalReward0EStates % maybeStates;
    if (mergerResult.targetState) {
        // There is an additional state in the result
        totalReward0EStates.resize(totalReward0EStates.size() + 1, true);

        // The overapproximation for the possible ec choices consists of the states that can reach the target states with prob. 0 and the target state itself.
        storm::storage::BitVector targetStateAsVector(transitionMatrix.getRowGroupCount(), false);
        targetStateAsVector.set(*mergerResult.targetState, true);
        ecChoicesHint = transitionMatrix.getRowFilter(
            storm::utility::graph::performProb0E(transitionMatrix, transitionMatrix.getRowGroupIndices(), transitionMatrix.transpose(true),
                                                 storm::storage::BitVector(targetStateAsVector.size(), true), targetStateAsVector));
        ecChoicesHint.set(transitionMatrix.getRowGroupIndices()[*mergerResult.targetState], true);
    } else {
        ecChoicesHint = storm::storage::BitVector(transitionMatrix.getRowCount(), true);
    }

    // set data for unbounded objectives
    lraObjectives = storm::storage::BitVector(this->objectives.size(), false);
    objectivesWithNoUpperTimeBound = storm::storage::BitVector(this->objectives.size(), false);
    actionsWithoutRewardInUnboundedPhase = storm::storage::BitVector(transitionMatrix.getRowCount(), true);
    for (uint_fast64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
        auto const& formula = *this->objectives[objIndex].formula;
        if (formula.getSubformula().isTotalRewardFormula()) {
            objectivesWithNoUpperTimeBound.set(objIndex, true);
            actionsWithoutRewardInUnboundedPhase &= storm::utility::vector::filterZero(actionRewards[objIndex]);
        }
        if (formula.getSubformula().isLongRunAverageRewardFormula()) {
            lraObjectives.set(objIndex, true);
            objectivesWithNoUpperTimeBound.set(objIndex, true);
        }
    }

    // Set data for LRA objectives (if available)
    if (!lraObjectives.empty()) {
        lraMecDecomposition = LraMecDecomposition();
        lraMecDecomposition->mecs = storm::storage::MaximalEndComponentDecomposition<ValueType>(
            transitionMatrix, transitionMatrix.transpose(true), storm::storage::BitVector(transitionMatrix.getRowGroupCount(), true),
            actionsWithoutRewardInUnboundedPhase);
        lraMecDecomposition->auxMecValues.resize(lraMecDecomposition->mecs.size());
    }

    // initialize data for the results
    checkHasBeenCalled = false;
    objectiveResults.resize(this->objectives.size());
    offsetsToAchievablePoint.resize(this->objectives.size(), storm::utility::zero<ValueType>());
    offsetToWeightedSum = storm::utility::zero<ValueType>();
    optimalChoices.resize(transitionMatrix.getRowGroupCount(), 0);

    // Print some statistics (if requested)
    if (storm::settings::getModule<storm::settings::modules::CoreSettings>().isShowStatisticsSet()) {
        STORM_PRINT_AND_LOG("Weight Vector Checker Statistics:\n");
        STORM_PRINT_AND_LOG("Final preprocessed model has " << transitionMatrix.getRowGroupCount() << " states.\n");
        STORM_PRINT_AND_LOG("Final preprocessed model has " << transitionMatrix.getRowCount() << " actions.\n");
        if (lraMecDecomposition) {
            STORM_PRINT_AND_LOG("Found " << lraMecDecomposition->mecs.size() << " end components that are relevant for LRA-analysis.\n");
            uint64_t numLraMecStates = 0;
            for (auto const& mec : this->lraMecDecomposition->mecs) {
                numLraMecStates += mec.size();
            }
            STORM_PRINT_AND_LOG(numLraMecStates << " states lie on such an end component.\n");
        }
        STORM_PRINT_AND_LOG('\n');
    }
}

template<class SparseModelType>
void StandardPcaaWeightVectorChecker<SparseModelType>::check(Environment const& env, std::vector<ValueType> weightVector) {
    // See https://doi.org/10.18154/RWTH-2023-09669 Algorithm 4.2
    STORM_LOG_INFO("Invoked WeightVectorChecker with weights \n"
                   << "\t" << storm::utility::vector::toString(storm::utility::vector::convertNumericVector<double>(weightVector)));
    STORM_LOG_THROW(std::any_of(weightVector.begin(), weightVector.end(), [](auto const& w_i) { return !storm::utility::isZero(w_i); }),
                    storm::exceptions::InvalidOperationException, "Weight vector must not be the zero vector.");
    checkHasBeenCalled = true;
    // Normalize weights so the vector has length 1
    // This is necessary for ensuring the required accuracy, i.e. distance between halfspace induced by weightedSum and weightvector and achievable point.
    ValueType const inputWeightVectorLength = storm::utility::sqrt(storm::utility::vector::dotProduct(weightVector, weightVector));
    storm::utility::vector::scaleVectorInPlace<ValueType, ValueType>(weightVector, storm::utility::one<ValueType>() / (inputWeightVectorLength));

    // Prepare and invoke weighted infinite horizon (long run average) phase
    std::vector<ValueType> weightedRewardVector(transitionMatrix.getRowCount(), storm::utility::zero<ValueType>());
    if (!lraObjectives.empty()) {
        boost::optional<std::vector<ValueType>> weightedStateRewardVector;
        for (auto objIndex : lraObjectives) {
            ValueType weight =
                storm::solver::minimize(this->objectives[objIndex].formula->getOptimalityType()) ? -weightVector[objIndex] : weightVector[objIndex];
            storm::utility::vector::addScaledVector(weightedRewardVector, actionRewards[objIndex], weight);
            if (!stateRewards.empty() && !stateRewards[objIndex].empty()) {
                if (!weightedStateRewardVector) {
                    weightedStateRewardVector = std::vector<ValueType>(transitionMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
                }
                storm::utility::vector::addScaledVector(weightedStateRewardVector.get(), stateRewards[objIndex], weight);
            }
        }
        infiniteHorizonWeightedPhase(env, weightedRewardVector, weightedStateRewardVector, weightVector);
        // Clear all values of the weighted reward vector
        weightedRewardVector.assign(weightedRewardVector.size(), storm::utility::zero<ValueType>());
    }

    // Prepare and invoke weighted indefinite horizon (unbounded total reward) phase
    auto totalRewardObjectives = objectivesWithNoUpperTimeBound & ~lraObjectives;
    for (auto objIndex : totalRewardObjectives) {
        if (storm::solver::minimize(this->objectives[objIndex].formula->getOptimalityType())) {
            storm::utility::vector::addScaledVector(weightedRewardVector, actionRewards[objIndex], -weightVector[objIndex]);
        } else {
            storm::utility::vector::addScaledVector(weightedRewardVector, actionRewards[objIndex], weightVector[objIndex]);
        }
    }
    unboundedWeightedPhase(env, weightedRewardVector, weightVector);

    unboundedIndividualPhase(env, weightVector);
    // Only invoke boundedPhase if necessarry, i.e., if there is at least one objective with a time bound
    for (auto const& obj : this->objectives) {
        if (!obj.formula->getSubformula().isTotalRewardFormula() && !obj.formula->getSubformula().isLongRunAverageRewardFormula()) {
            boundedPhase(env, weightVector, weightedRewardVector);
            break;
        }
    }
    STORM_LOG_INFO("Weight vector check done. Lower bounds for results in initial state: "
                   << storm::utility::vector::toString(storm::utility::vector::convertNumericVector<double>(getAchievablePoint())));
    // Validate that the results are sufficiently precise
    ValueType weightedSum = storm::utility::zero<ValueType>();
    for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
        weightedSum += (storm::solver::minimize(this->objectives[objIndex].formula->getOptimalityType()) ? -weightVector[objIndex] : weightVector[objIndex]) *
                       getAchievablePoint()[objIndex];
    }
    ValueType resultingWeightedPrecision = storm::utility::abs<ValueType>(getOptimalWeightedSum() - weightedSum);
    // Since the weight vector is normalized (has length 1), the resultingWeightedPrecision coincides with the distance between over- and under-approximaiton
    STORM_LOG_WARN_COND(resultingWeightedPrecision <= this->getWeightedPrecision() + storm::utility::convertNumber<ValueType>(1e-10),
                        "The desired precision was not reached: resulting precision "
                            << resultingWeightedPrecision << " exceeds specified value " << this->getWeightedPrecision() << " by approx. "
                            << (storm::utility::convertNumber<double, ValueType>(resultingWeightedPrecision - this->getWeightedPrecision()))
                            << ". Weight vector is" << storm::utility::vector::toString(storm::utility::vector::convertNumericVector<double>(weightVector))
                            << ".");
    if (!storm::utility::isOne(inputWeightVectorLength)) {
        // reverse the normalization of the weight vector for the returned optimal weighted sum.
        storm::utility::vector::scaleVectorInPlace<ValueType, ValueType>(weightedResult, inputWeightVectorLength);
        offsetToWeightedSum *= inputWeightVectorLength;
    }
}

template<class SparseModelType>
std::vector<typename StandardPcaaWeightVectorChecker<SparseModelType>::ValueType> StandardPcaaWeightVectorChecker<SparseModelType>::getAchievablePoint() const {
    STORM_LOG_THROW(checkHasBeenCalled, storm::exceptions::InvalidOperationException, "Tried to retrieve results but check(..) has not been called before.");
    std::vector<ValueType> res;
    res.reserve(this->objectives.size());
    for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
        res.push_back(this->objectives[objIndex].clipResult(this->objectiveResults[objIndex][initialState] + this->offsetsToAchievablePoint[objIndex]));
    }
    return res;
}

template<class SparseModelType>
typename StandardPcaaWeightVectorChecker<SparseModelType>::ValueType StandardPcaaWeightVectorChecker<SparseModelType>::getOptimalWeightedSum() const {
    STORM_LOG_THROW(checkHasBeenCalled, storm::exceptions::InvalidOperationException, "Tried to retrieve results but check(..) has not been called before.");
    return this->weightedResult[initialState] + this->offsetToWeightedSum;
}

template<class SparseModelType>
storm::storage::Scheduler<typename StandardPcaaWeightVectorChecker<SparseModelType>::ValueType>
StandardPcaaWeightVectorChecker<SparseModelType>::computeScheduler() const {
    STORM_LOG_THROW(this->checkHasBeenCalled, storm::exceptions::InvalidOperationException,
                    "Tried to retrieve results but check(..) has not been called before.");
    for (auto const& obj : this->objectives) {
        STORM_LOG_THROW(obj.formula->getSubformula().isTotalRewardFormula() || obj.formula->getSubformula().isLongRunAverageRewardFormula(),
                        storm::exceptions::NotImplementedException, "Scheduler retrival is only implemented for objectives without time-bound.");
    }
    auto const numStatesOfInputModel = goalStateMergerInputToReducedStateIndexMapping.size();
    storm::storage::Scheduler<ValueType> result(numStatesOfInputModel);
    for (uint64_t inputModelState = 0; inputModelState < numStatesOfInputModel; ++inputModelState) {
        auto const reducedModelState = goalStateMergerInputToReducedStateIndexMapping[inputModelState];
        if (reducedModelState >= optimalChoices.size()) {
            // This state is a "reward0AState", i.e., it has no reward for any scheduler. We can set an arbitrary choice here.
            result.setChoice(0, inputModelState);
        } else {
            auto const reducedModelChoice = optimalChoices[reducedModelState];
            auto const inputModelChoice = goalStateMergerReducedToInputChoiceMapping[reducedModelChoice];
            result.setChoice(inputModelChoice, inputModelState);
        }
    }
    return result;
}

template<typename ValueType>
void computeSchedulerProb1(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                           storm::storage::BitVector const& consideredStates, storm::storage::BitVector const& statesToReach, std::vector<uint64_t>& choices,
                           storm::storage::BitVector const* allowedChoices = nullptr) {
    std::vector<uint64_t> stack;
    storm::storage::BitVector processedStates = statesToReach;
    stack.insert(stack.end(), processedStates.begin(), processedStates.end());
    uint64_t currentState = 0;

    while (!stack.empty()) {
        currentState = stack.back();
        stack.pop_back();

        for (auto const& predecessorEntry : backwardTransitions.getRow(currentState)) {
            auto predecessor = predecessorEntry.getColumn();
            if (consideredStates.get(predecessor) && !processedStates.get(predecessor)) {
                // Find a choice leading to an already processed state (such a choice has to exist since this is a predecessor of the currentState)
                auto const& groupStart = transitionMatrix.getRowGroupIndices()[predecessor];
                auto const& groupEnd = transitionMatrix.getRowGroupIndices()[predecessor + 1];
                uint64_t row = allowedChoices ? allowedChoices->getNextSetIndex(groupStart) : groupStart;
                for (; row < groupEnd; row = allowedChoices ? allowedChoices->getNextSetIndex(row + 1) : row + 1) {
                    bool hasSuccessorInProcessedStates = false;
                    for (auto const& successorOfPredecessor : transitionMatrix.getRow(row)) {
                        if (processedStates.get(successorOfPredecessor.getColumn())) {
                            hasSuccessorInProcessedStates = true;
                            break;
                        }
                    }
                    if (hasSuccessorInProcessedStates) {
                        choices[predecessor] = row - groupStart;
                        processedStates.set(predecessor, true);
                        stack.push_back(predecessor);
                        break;
                    }
                }
                STORM_LOG_ASSERT(allowedChoices || row < groupEnd,
                                 "Unable to find choice at a predecessor of a processed state that leads to a processed state.");
            }
        }
    }
    STORM_LOG_ASSERT(consideredStates.isSubsetOf(processedStates), "Not all states have been processed.");
}

template<typename ValueType>
void computeSchedulerProb0(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                           storm::storage::BitVector const& consideredStates, storm::storage::BitVector const& statesToAvoid,
                           storm::storage::BitVector const& allowedChoices, std::vector<uint64_t>& choices) {
    for (auto state : consideredStates) {
        auto const& groupStart = transitionMatrix.getRowGroupIndices()[state];
        auto const& groupEnd = transitionMatrix.getRowGroupIndices()[state + 1];
        bool choiceFound = false;
        for (uint64_t row = allowedChoices.getNextSetIndex(groupStart); row < groupEnd; row = allowedChoices.getNextSetIndex(row + 1)) {
            choiceFound = true;
            for (auto const& element : transitionMatrix.getRow(row)) {
                if (statesToAvoid.get(element.getColumn())) {
                    choiceFound = false;
                    break;
                }
            }
            if (choiceFound) {
                choices[state] = row - groupStart;
                break;
            }
        }
        STORM_LOG_ASSERT(choiceFound, "Unable to find choice for a state.");
    }
}

template<typename ValueType>
std::vector<uint64_t> computeValidInitialScheduler(storm::storage::SparseMatrix<ValueType> const& matrix, storm::storage::BitVector const& rowsWithSumLessOne) {
    std::vector<uint64_t> result(matrix.getRowGroupCount());
    auto const& groups = matrix.getRowGroupIndices();
    auto backwardsTransitions = matrix.transpose(true);
    storm::storage::BitVector processedStates(result.size(), false);
    for (uint64_t state = 0; state < result.size(); ++state) {
        if (rowsWithSumLessOne.getNextSetIndex(groups[state]) < groups[state + 1]) {
            result[state] = rowsWithSumLessOne.getNextSetIndex(groups[state]) - groups[state];
            processedStates.set(state, true);
        }
    }

    computeSchedulerProb1(matrix, backwardsTransitions, ~processedStates, processedStates, result);
    return result;
}

/*!
 * Computes a scheduler taking the choices from the given set only finitely often
 * @param safeStates it is assumed that reaching such a state is unproblematic. The choice for these states is not set.
 * @pre such a scheduler exists
 */
template<typename ValueType>
void computeSchedulerFinitelyOften(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                   storm::storage::SparseMatrix<ValueType> const& backwardTransitions, storm::storage::BitVector const& finitelyOftenChoices,
                                   storm::storage::BitVector safeStates, std::vector<uint64_t>& choices) {
    auto badStates = transitionMatrix.getRowGroupFilter(finitelyOftenChoices, true) & ~safeStates;
    // badStates shall only be reached finitely often

    auto reachBadWithProbGreater0AStates = storm::utility::graph::performProbGreater0A(
        transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardTransitions, ~safeStates, badStates, false, 0, ~finitelyOftenChoices);
    // States in ~reachBadWithProbGreater0AStates can avoid bad states forever by only taking ~finitelyOftenChoices.
    // We compute a scheduler for these states achieving exactly this (but we exclude the safe states)
    auto avoidBadStates = ~reachBadWithProbGreater0AStates & ~safeStates;
    computeSchedulerProb0(transitionMatrix, backwardTransitions, avoidBadStates, reachBadWithProbGreater0AStates, ~finitelyOftenChoices, choices);

    // We need to take care of states that will reach a bad state with prob greater 0 (including the bad states themselves).
    // due to the precondition, we know that it has to be possible to eventually avoid the bad states for ever.
    // Perform a backwards search from the avoid states and store choices with prob. 1
    computeSchedulerProb1(transitionMatrix, backwardTransitions, reachBadWithProbGreater0AStates, avoidBadStates | safeStates, choices);
}

template<class SparseModelType>
void StandardPcaaWeightVectorChecker<SparseModelType>::infiniteHorizonWeightedPhase(Environment const& inputEnv,
                                                                                    std::vector<ValueType> const& weightedActionRewardVector,
                                                                                    boost::optional<std::vector<ValueType>> const& weightedStateRewardVector,
                                                                                    std::vector<ValueType> const& weightVector) {
    auto solverEnv = inputEnv;
    // see epsilon in https://doi.org/10.18154/RWTH-2023-09669 Algorithm 5.2
    ValueType epsilon = this->getWeightedPrecisionUnboundedPhase() * storm::utility::sqrt(storm::utility::vector::dotProduct(weightVector, weightVector)) /
                        storm::utility::convertNumber<ValueType>(2.0);
    // We want to compute a value v_C for each MEC C that upper bounds the true MEC value and is also epsilon/2 close to it.
    // We therefore compute a value that is epsilon/4 close to it and then add epsilon/4 as offset below.
    ValueType const offset = epsilon / storm::utility::convertNumber<ValueType>(4.0);
    solverEnv.solver().lra().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(offset));
    solverEnv.solver().lra().setRelativeTerminationCriterion(false);
    // Compute the optimal (weighted) lra value for each mec, keeping track of the optimal choices
    STORM_LOG_ASSERT(lraMecDecomposition, "Mec decomposition for lra computations not initialized.");
    storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType> helper = createNondetInfiniteHorizonHelper(this->transitionMatrix);
    helper.provideLongRunComponentDecomposition(lraMecDecomposition->mecs);
    helper.setOptimizationDirection(storm::solver::OptimizationDirection::Maximize);
    helper.setProduceScheduler(true);
    for (uint64_t mecIndex = 0; mecIndex < lraMecDecomposition->mecs.size(); ++mecIndex) {
        auto const& mec = lraMecDecomposition->mecs[mecIndex];
        auto actionValueGetter = [&weightedActionRewardVector](uint64_t const& a) { return weightedActionRewardVector[a]; };
        typename storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType>::ValueGetter stateValueGetter;
        if (weightedStateRewardVector) {
            stateValueGetter = [&weightedStateRewardVector](uint64_t const& s) { return weightedStateRewardVector.get()[s]; };
        } else {
            stateValueGetter = [](uint64_t const&) { return storm::utility::zero<ValueType>(); };
        }
        lraMecDecomposition->auxMecValues[mecIndex] = helper.computeLraForComponent(solverEnv, stateValueGetter, actionValueGetter, mec) + offset;
    }
    // Extract the produced optimal choices for the MECs
    this->optimalChoices = std::move(helper.getProducedOptimalChoices());
}

template<class SparseModelType>
void StandardPcaaWeightVectorChecker<SparseModelType>::unboundedWeightedPhase(Environment const& inputEnv, std::vector<ValueType> const& weightedRewardVector,
                                                                              std::vector<ValueType> const& weightVector) {
    storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType> solverFactory;
    auto solverEnv = inputEnv;
    solverEnv.solver().minMax().setRelativeTerminationCriterion(false);
    solverEnv.solver().lra().setRelativeTerminationCriterion(false);
    bool const requireSoundApproximation = !solverEnv.solver().isForceExact() && solverEnv.solver().isForceSoundness();
    ValueType const two = storm::utility::convertNumber<ValueType>(2.0);
    // see epsilon in https://doi.org/10.18154/RWTH-2023-09669 Algorithm 4.2
    ValueType adjustedPrecision =
        this->getWeightedPrecisionUnboundedPhase() * storm::utility::sqrt(storm::utility::vector::dotProduct(weightVector, weightVector)) / two;
    if (solverEnv.solver().isForceExact()) {
        // If we are already using an exact solver, we consider the precision to be zero
        adjustedPrecision = storm::utility::zero<ValueType>();
    } else if (requireSoundApproximation) {
        adjustedPrecision /= two;  // need to be more precise to get a correct and sufficiently tight upper bound on the weighted sum
    }
    if (lraObjectives.empty()) {
        solverEnv.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(adjustedPrecision));
    } else {
        // need to be more precise to distribute the approximation error between lra and total reward phase
        solverEnv.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber, ValueType>(adjustedPrecision / two));
        solverEnv.solver().lra().setPrecision(storm::utility::convertNumber<storm::RationalNumber, ValueType>(adjustedPrecision / two));
    }

    // Catch the case where all values on the RHS of the MinMax equation system are zero.
    if (this->objectivesWithNoUpperTimeBound.empty() ||
        ((this->lraObjectives.empty() || !storm::utility::vector::hasNonZeroEntry(lraMecDecomposition->auxMecValues)) &&
         !storm::utility::vector::hasNonZeroEntry(weightedRewardVector))) {
        this->weightedResult.assign(transitionMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
        storm::storage::BitVector statesInLraMec(transitionMatrix.getRowGroupCount(), false);
        if (this->lraMecDecomposition) {
            for (auto const& mec : this->lraMecDecomposition->mecs) {
                for (auto const& sc : mec) {
                    statesInLraMec.set(sc.first, true);
                }
            }
        }
        // Get an arbitrary scheduler that yields finite reward for all objectives
        computeSchedulerFinitelyOften(transitionMatrix, transitionMatrix.transpose(true), ~actionsWithoutRewardInUnboundedPhase, statesInLraMec,
                                      this->optimalChoices);
        return;
    }

    updateEcQuotient(weightedRewardVector);

    // Set up the choice values
    storm::utility::vector::selectVectorValues(ecQuotient->auxChoiceValues, ecQuotient->ecqToOriginalChoiceMapping, weightedRewardVector);
    std::map<uint64_t, uint64_t> ecqStateToOptimalMecMap;
    if (!lraObjectives.empty()) {
        // We also need to assign a value for each ecQuotientChoice that corresponds to "staying" in the eliminated EC. (at this point these choices should all
        // have a value of zero). Since each of the eliminated ECs has to contain *at least* one LRA EC, we need to find the largest value among the contained
        // LRA ECs
        storm::storage::BitVector foundEcqChoices(ecQuotient->matrix.getRowCount(), false);  // keeps track of choices we have already seen before
        for (uint64_t mecIndex = 0; mecIndex < lraMecDecomposition->mecs.size(); ++mecIndex) {
            auto const& mec = lraMecDecomposition->mecs[mecIndex];
            auto const& mecValue = lraMecDecomposition->auxMecValues[mecIndex];
            uint64_t ecqState = ecQuotient->originalToEcqStateMapping[mec.begin()->first];
            if (ecqState >= ecQuotient->matrix.getRowGroupCount()) {
                // The mec was not part of the ecquotient. This means that it must have value 0.
                // No further processing is needed.
                continue;
            }
            uint64_t ecqChoice = ecQuotient->ecqStayInEcChoices.getNextSetIndex(ecQuotient->matrix.getRowGroupIndices()[ecqState]);
            STORM_LOG_ASSERT(ecqChoice < ecQuotient->matrix.getRowGroupIndices()[ecqState + 1],
                             "Unable to find choice that represents staying inside the (eliminated) ec.");
            auto& ecqChoiceValue = ecQuotient->auxChoiceValues[ecqChoice];
            auto insertionRes = ecqStateToOptimalMecMap.emplace(ecqState, mecIndex);
            if (insertionRes.second) {
                // We have seen this ecqState for the first time.
                STORM_LOG_ASSERT(storm::utility::isZero(ecqChoiceValue),
                                 "Expected a total reward of zero for choices that represent staying in an EC for ever.");
                ecqChoiceValue = mecValue;
            } else {
                if (mecValue > ecqChoiceValue) {  // found a larger value
                    ecqChoiceValue = mecValue;
                    insertionRes.first->second = mecIndex;
                }
            }
        }
    }

    std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> solver = solverFactory.create(solverEnv, ecQuotient->matrix);
    solver->setTrackScheduler(true);
    solver->setHasUniqueSolution(true);
    solver->setOptimizationDirection(storm::solver::OptimizationDirection::Maximize);
    auto req = solver->getRequirements(solverEnv, storm::solver::OptimizationDirection::Maximize);
    setBoundsToSolver(*solver, req.lowerBounds(), req.upperBounds(), weightVector, objectivesWithNoUpperTimeBound, ecQuotient->matrix,
                      ecQuotient->rowsWithSumLessOne, ecQuotient->auxChoiceValues);
    if (solver->hasLowerBound()) {
        req.clearLowerBounds();
    }
    if (solver->hasUpperBound()) {
        req.clearUpperBounds();
    }
    if (req.validInitialScheduler()) {
        solver->setInitialScheduler(computeValidInitialScheduler(ecQuotient->matrix, ecQuotient->rowsWithSumLessOne));
        req.clearValidInitialScheduler();
    }
    STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                    "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
    solver->setRequirementsChecked(true);

    // Use the (0...0) vector as initial guess for the solution.
    std::fill(ecQuotient->auxStateValues.begin(), ecQuotient->auxStateValues.end(), storm::utility::zero<ValueType>());

    solver->solveEquations(solverEnv, ecQuotient->auxStateValues, ecQuotient->auxChoiceValues);
    this->weightedResult = std::vector<ValueType>(transitionMatrix.getRowGroupCount());

    transformEcqSolutionToOriginalModel(ecQuotient->auxStateValues, solver->getSchedulerChoices(), ecqStateToOptimalMecMap, this->weightedResult,
                                        this->optimalChoices);

    offsetToWeightedSum = storm::utility::zero<ValueType>();
    if (requireSoundApproximation) {
        // Add offset to ensure that we have an upper bound on the true optimal value
        offsetToWeightedSum += adjustedPrecision;
    }
}

template<class SparseModelType>
void StandardPcaaWeightVectorChecker<SparseModelType>::unboundedIndividualPhase(Environment const& inputEnv, std::vector<ValueType> const& weightVector) {
    auto solverEnv = inputEnv;
    storm::storage::SparseMatrix<ValueType> deterministicMatrix = transitionMatrix.selectRowsFromRowGroups(this->optimalChoices, false);
    storm::storage::SparseMatrix<ValueType> deterministicBackwardTransitions = deterministicMatrix.transpose();
    std::vector<ValueType> deterministicStateRewards(deterministicMatrix.getRowCount());  // allocate here
    storm::solver::GeneralLinearEquationSolverFactory<ValueType> linearEquationSolverFactory;
    bool const requireSoundApproximation = !solverEnv.solver().isForceExact() && solverEnv.solver().isForceSoundness();
    // see epsilon and epsilon_j in https://doi.org/10.18154/RWTH-2023-09669 Algorithm 4.2
    ValueType const two = storm::utility::convertNumber<ValueType>(2.0);
    ValueType epsilon = this->getWeightedPrecisionUnboundedPhase() * storm::utility::sqrt(storm::utility::vector::dotProduct(weightVector, weightVector)) / two;
    if (solverEnv.solver().isForceExact()) {
        // If we are already using an exact solver, we consider the precision to be zero
        epsilon = storm::utility::zero<ValueType>();
    } else if (requireSoundApproximation) {
        epsilon /= two;  // need to be more precise to get a correct and sufficiently tight achievable value
    }

    auto infiniteHorizonHelper = createDetInfiniteHorizonHelper(deterministicMatrix);
    infiniteHorizonHelper.provideBackwardTransitions(deterministicBackwardTransitions);

    // We compute an estimate for the results of the individual objectives which is obtained from the weighted result and the result of the objectives
    // computed so far. Note that weightedResult = Sum_{i=1}^{n} w_i * objectiveResult_i.
    std::vector<ValueType> weightedSumOfUncheckedObjectives = weightedResult;
    ValueType sumOfWeightsOfUncheckedObjectives = storm::utility::vector::sum_if(weightVector, objectivesWithNoUpperTimeBound);

    for (uint_fast64_t const& objIndex : storm::utility::vector::getSortedIndices(weightVector)) {
        auto const& obj = this->objectives[objIndex];
        if (objectivesWithNoUpperTimeBound.get(objIndex)) {
            ValueType epsilon_j = epsilon / storm::utility::convertNumber<ValueType, uint64_t>(objectivesWithNoUpperTimeBound.getNumberOfSetBits());
            if (!storm::utility::isZero(weightVector[objIndex])) {
                epsilon_j /= storm::utility::abs(weightVector[objIndex]);
            }
            solverEnv.solver().setLinearEquationSolverPrecision(storm::utility::convertNumber<RationalNumber>(epsilon_j), false);
            solverEnv.solver().lra().setPrecision(storm::utility::convertNumber<RationalNumber>(epsilon_j));
            solverEnv.solver().lra().setRelativeTerminationCriterion(false);

            if (lraObjectives.get(objIndex)) {
                auto actionValueGetter = [&](uint64_t const& a) {
                    return actionRewards[objIndex][transitionMatrix.getRowGroupIndices()[a] + this->optimalChoices[a]];
                };
                typename storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType>::ValueGetter stateValueGetter;
                if (stateRewards.empty() || stateRewards[objIndex].empty()) {
                    stateValueGetter = [](uint64_t const&) { return storm::utility::zero<ValueType>(); };
                } else {
                    stateValueGetter = [&](uint64_t const& s) { return stateRewards[objIndex][s]; };
                }
                objectiveResults[objIndex] = infiniteHorizonHelper.computeLongRunAverageValues(solverEnv, stateValueGetter, actionValueGetter);
            } else {  // i.e. a total reward objective
                storm::utility::vector::selectVectorValues(deterministicStateRewards, this->optimalChoices, transitionMatrix.getRowGroupIndices(),
                                                           actionRewards[objIndex]);
                storm::storage::BitVector statesWithRewards = ~storm::utility::vector::filterZero(deterministicStateRewards);
                // As maybestates we pick the states from which a state with reward is reachable
                storm::storage::BitVector maybeStates = storm::utility::graph::performProbGreater0(
                    deterministicBackwardTransitions, storm::storage::BitVector(deterministicMatrix.getRowCount(), true), statesWithRewards);

                // Compute the estimate for this objective
                if (!storm::utility::isZero(weightVector[objIndex]) && !storm::utility::isZero(sumOfWeightsOfUncheckedObjectives)) {
                    objectiveResults[objIndex] = weightedSumOfUncheckedObjectives;
                    ValueType scalingFactor = storm::utility::one<ValueType>() / sumOfWeightsOfUncheckedObjectives;
                    if (storm::solver::minimize(obj.formula->getOptimalityType())) {
                        scalingFactor *= -storm::utility::one<ValueType>();
                    }
                    storm::utility::vector::scaleVectorInPlace(objectiveResults[objIndex], scalingFactor);
                    storm::utility::vector::clip(objectiveResults[objIndex], obj.lowerResultBound, obj.upperResultBound);
                }
                // Make sure that the objectiveResult is initialized correctly
                objectiveResults[objIndex].resize(transitionMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());

                if (!maybeStates.empty()) {
                    bool needEquationSystem =
                        linearEquationSolverFactory.getEquationProblemFormat(solverEnv) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem;
                    storm::storage::SparseMatrix<ValueType> submatrix = deterministicMatrix.getSubmatrix(true, maybeStates, maybeStates, needEquationSystem);
                    if (needEquationSystem) {
                        // Converting the matrix from the fixpoint notation to the form needed for the equation
                        // system. That is, we go from x = A*x + b to (I-A)x = b.
                        submatrix.convertToEquationSystem();
                    }

                    // Prepare solution vector and rhs of the equation system.
                    std::vector<ValueType> x = storm::utility::vector::filterVector(objectiveResults[objIndex], maybeStates);
                    std::vector<ValueType> b = storm::utility::vector::filterVector(deterministicStateRewards, maybeStates);

                    // Now solve the resulting equation system.
                    std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> solver = linearEquationSolverFactory.create(solverEnv, submatrix);
                    auto req = solver->getRequirements(solverEnv);
                    solver->clearBounds();
                    storm::storage::BitVector submatrixRowsWithSumLessOne = deterministicMatrix.getRowFilter(maybeStates, maybeStates) % maybeStates;
                    submatrixRowsWithSumLessOne.complement();
                    this->setBoundsToSolver(*solver, req.lowerBounds(), req.upperBounds(), objIndex, submatrix, submatrixRowsWithSumLessOne, b);
                    if (solver->hasLowerBound()) {
                        req.clearLowerBounds();
                    }
                    if (solver->hasUpperBound()) {
                        req.clearUpperBounds();
                    }
                    STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                                    "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
                    solver->solveEquations(solverEnv, x, b);
                    if (requireSoundApproximation) {
                        // add offsets to ensure that we have an upper/lower bound on the true optimal value
                        offsetsToAchievablePoint[objIndex] =
                            storm::solver::maximize(this->objectives[objIndex].formula->getOptimalityType()) ? -epsilon_j : epsilon_j;
                    }
                    // Set the result for this objective accordingly
                    storm::utility::vector::setVectorValues<ValueType>(objectiveResults[objIndex], maybeStates, x);
                }
                storm::utility::vector::setVectorValues<ValueType>(objectiveResults[objIndex], ~maybeStates, storm::utility::zero<ValueType>());
            }
            // Update the estimate for the next objectives.
            if (!storm::utility::isZero(weightVector[objIndex])) {
                storm::utility::vector::addScaledVector(weightedSumOfUncheckedObjectives, objectiveResults[objIndex], -weightVector[objIndex]);
                sumOfWeightsOfUncheckedObjectives -= weightVector[objIndex];
            }
        } else {
            // Other objectives will be computed in bounded phase.
            objectiveResults[objIndex] = std::vector<ValueType>(transitionMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
        }
    }
}

template<class SparseModelType>
void StandardPcaaWeightVectorChecker<SparseModelType>::updateEcQuotient(std::vector<ValueType> const& weightedRewardVector) {
    // Check whether we need to update the currently cached ecElimResult
    storm::storage::BitVector newTotalReward0Choices = storm::utility::vector::filterZero(weightedRewardVector);
    storm::storage::BitVector zeroLraRewardChoices(weightedRewardVector.size(), true);
    if (lraMecDecomposition) {
        for (uint64_t mecIndex = 0; mecIndex < lraMecDecomposition->mecs.size(); ++mecIndex) {
            if (!storm::utility::isZero(lraMecDecomposition->auxMecValues[mecIndex])) {
                // The mec has a non-zero value, so flag all its choices as non-zero
                auto const& mec = lraMecDecomposition->mecs[mecIndex];
                for (auto const& stateChoices : mec) {
                    for (auto const& choice : stateChoices.second) {
                        zeroLraRewardChoices.set(choice, false);
                    }
                }
            }
        }
    }
    storm::storage::BitVector newReward0Choices = newTotalReward0Choices & zeroLraRewardChoices;
    if (!ecQuotient || ecQuotient->origReward0Choices != newReward0Choices) {
        // It is sufficient to consider the states from which a transition with non-zero reward is reachable. (The remaining states always have reward zero).
        auto nonZeroRewardStates = transitionMatrix.getRowGroupFilter(newReward0Choices, true);
        nonZeroRewardStates.complement();
        storm::storage::BitVector subsystemStates = storm::utility::graph::performProbGreater0E(
            transitionMatrix.transpose(true), storm::storage::BitVector(transitionMatrix.getRowGroupCount(), true), nonZeroRewardStates);

        // Remove neutral end components, i.e., ECs in which no total reward is earned.
        // Note that such ECs contain one (or maybe more) LRA ECs.
        auto ecElimResult = storm::transformer::EndComponentEliminator<ValueType>::transform(transitionMatrix, subsystemStates,
                                                                                             ecChoicesHint & newTotalReward0Choices, totalReward0EStates);

        storm::storage::BitVector rowsWithSumLessOne(ecElimResult.matrix.getRowCount(), false);
        for (uint64_t row = 0; row < rowsWithSumLessOne.size(); ++row) {
            if (ecElimResult.matrix.getRow(row).getNumberOfEntries() == 0) {
                rowsWithSumLessOne.set(row, true);
            } else {
                for (auto const& entry : transitionMatrix.getRow(ecElimResult.newToOldRowMapping[row])) {
                    if (!subsystemStates.get(entry.getColumn())) {
                        rowsWithSumLessOne.set(row, true);
                        break;
                    }
                }
            }
        }

        ecQuotient = EcQuotient();
        ecQuotient->matrix = std::move(ecElimResult.matrix);
        ecQuotient->ecqToOriginalChoiceMapping = std::move(ecElimResult.newToOldRowMapping);
        ecQuotient->originalToEcqStateMapping = std::move(ecElimResult.oldToNewStateMapping);
        ecQuotient->ecqToOriginalStateMapping.resize(ecQuotient->matrix.getRowGroupCount());
        for (uint64_t state = 0; state < ecQuotient->originalToEcqStateMapping.size(); ++state) {
            uint64_t ecqState = ecQuotient->originalToEcqStateMapping[state];
            if (ecqState < ecQuotient->matrix.getRowGroupCount()) {
                ecQuotient->ecqToOriginalStateMapping[ecqState].insert(state);
            }
        }
        ecQuotient->ecqStayInEcChoices = std::move(ecElimResult.sinkRows);
        ecQuotient->origReward0Choices = std::move(newReward0Choices);
        ecQuotient->origTotalReward0Choices = std::move(newTotalReward0Choices);
        ecQuotient->rowsWithSumLessOne = std::move(rowsWithSumLessOne);
        ecQuotient->auxStateValues.resize(ecQuotient->matrix.getRowGroupCount());
        ecQuotient->auxChoiceValues.resize(ecQuotient->matrix.getRowCount());
    }
}

template<class SparseModelType>
void StandardPcaaWeightVectorChecker<SparseModelType>::setBoundsToSolver(storm::solver::AbstractEquationSolver<ValueType>& solver, bool requiresLower,
                                                                         bool requiresUpper, uint64_t objIndex,
                                                                         storm::storage::SparseMatrix<ValueType> const& transitions,
                                                                         storm::storage::BitVector const& rowsWithSumLessOne,
                                                                         std::vector<ValueType> const& rewards) const {
    // Check whether bounds are already available
    if (this->objectives[objIndex].lowerResultBound) {
        solver.setLowerBound(this->objectives[objIndex].lowerResultBound.get());
    }
    if (this->objectives[objIndex].upperResultBound) {
        solver.setUpperBound(this->objectives[objIndex].upperResultBound.get());
    }

    if ((requiresLower && !solver.hasLowerBound()) || (requiresUpper && !solver.hasUpperBound())) {
        computeAndSetBoundsToSolver(solver, requiresLower, requiresUpper, transitions, rowsWithSumLessOne, rewards);
    }
}

template<class SparseModelType>
void StandardPcaaWeightVectorChecker<SparseModelType>::setBoundsToSolver(storm::solver::AbstractEquationSolver<ValueType>& solver, bool requiresLower,
                                                                         bool requiresUpper, std::vector<ValueType> const& weightVector,
                                                                         storm::storage::BitVector const& objectiveFilter,
                                                                         storm::storage::SparseMatrix<ValueType> const& transitions,
                                                                         storm::storage::BitVector const& rowsWithSumLessOne,
                                                                         std::vector<ValueType> const& rewards) const {
    // Check whether bounds are already available
    boost::optional<ValueType> lowerBound = this->computeWeightedResultBound(true, weightVector, objectiveFilter & ~lraObjectives);
    if (lowerBound) {
        if (!lraObjectives.empty()) {
            auto min = std::min_element(lraMecDecomposition->auxMecValues.begin(), lraMecDecomposition->auxMecValues.end());
            if (min != lraMecDecomposition->auxMecValues.end()) {
                lowerBound.get() += *min;
            }
        }
        solver.setLowerBound(lowerBound.get());
    }
    boost::optional<ValueType> upperBound = this->computeWeightedResultBound(false, weightVector, objectiveFilter);
    if (upperBound) {
        if (!lraObjectives.empty()) {
            auto max = std::max_element(lraMecDecomposition->auxMecValues.begin(), lraMecDecomposition->auxMecValues.end());
            if (max != lraMecDecomposition->auxMecValues.end()) {
                upperBound.get() += *max;
            }
        }
        solver.setUpperBound(upperBound.get());
    }

    if ((requiresLower && !solver.hasLowerBound()) || (requiresUpper && !solver.hasUpperBound())) {
        computeAndSetBoundsToSolver(solver, requiresLower, requiresUpper, transitions, rowsWithSumLessOne, rewards);
    }
}

template<class SparseModelType>
void StandardPcaaWeightVectorChecker<SparseModelType>::computeAndSetBoundsToSolver(storm::solver::AbstractEquationSolver<ValueType>& solver, bool requiresLower,
                                                                                   bool requiresUpper,
                                                                                   storm::storage::SparseMatrix<ValueType> const& transitions,
                                                                                   storm::storage::BitVector const& rowsWithSumLessOne,
                                                                                   std::vector<ValueType> const& rewards) const {
    // Compute the one step target probs
    std::vector<ValueType> oneStepTargetProbs(transitions.getRowCount(), storm::utility::zero<ValueType>());
    for (auto row : rowsWithSumLessOne) {
        oneStepTargetProbs[row] = storm::utility::one<ValueType>() - transitions.getRowSum(row);
    }

    bool hasNegativeReward = false;
    bool hasPositiveReward = false;
    for (auto const& rew : rewards) {
        if (rew < storm::utility::zero<ValueType>()) {
            hasNegativeReward = true;
        } else if (rew > storm::utility::zero<ValueType>()) {
            hasPositiveReward = true;
        }
        if (hasNegativeReward && hasPositiveReward) {
            break;
        }
    }
    if (requiresLower && !solver.hasLowerBound()) {
        // Compute lower bounds
        if (hasNegativeReward) {
            // For lower bounds we actually compute upper bounds for the negated rewards because DsMpi is not implemented for negative rewards.
            std::vector<ValueType> tmpRewards(rewards.size());
            storm::utility::vector::applyPointwise(rewards, tmpRewards,
                                                   [](ValueType const& v) { return std::max<ValueType>(storm::utility::zero<ValueType>(), -v); });
            std::vector<ValueType> lowerBounds =
                storm::modelchecker::helper::DsMpiMdpUpperRewardBoundsComputer<ValueType>(transitions, tmpRewards, oneStepTargetProbs).computeUpperBounds();
            storm::utility::vector::scaleVectorInPlace(lowerBounds, -storm::utility::one<ValueType>());
            solver.setLowerBounds(std::move(lowerBounds));
        } else {
            solver.setLowerBound(storm::utility::zero<ValueType>());
        }
    }

    // Compute upper bounds
    if (requiresUpper && !solver.hasUpperBound()) {
        if (hasPositiveReward) {
            solver.setUpperBound(storm::modelchecker::helper::BaierUpperRewardBoundsComputer<ValueType>(transitions, oneStepTargetProbs)
                                     .computeTotalRewardBounds(rewards)
                                     .upper);
        } else {
            solver.setUpperBound(storm::utility::zero<ValueType>());
        }
    }
}

template<class SparseModelType>
void StandardPcaaWeightVectorChecker<SparseModelType>::transformEcqSolutionToOriginalModel(std::vector<ValueType> const& ecqSolution,
                                                                                           std::vector<uint_fast64_t> const& ecqOptimalChoices,
                                                                                           std::map<uint64_t, uint64_t> const& ecqStateToOptimalMecMap,
                                                                                           std::vector<ValueType>& originalSolution,
                                                                                           std::vector<uint_fast64_t>& originalOptimalChoices) const {
    auto backwardsTransitions = transitionMatrix.transpose(true);

    // Keep track of states for which no choice has been set yet.
    storm::storage::BitVector unprocessedStates(transitionMatrix.getRowGroupCount(), true);

    // For each eliminated ec, keep track of the states (within the ec) that we want to reach and the states for which a choice needs to be set
    // (Declared already at this point to avoid expensive allocations in each loop iteration)
    storm::storage::BitVector ecStatesToReach(transitionMatrix.getRowGroupCount(), false);
    storm::storage::BitVector ecStatesToProcess(transitionMatrix.getRowGroupCount(), false);

    // Run through each state of the ec quotient as well as the associated state(s) of the original model
    for (uint64_t ecqState = 0; ecqState < ecqSolution.size(); ++ecqState) {
        uint64_t ecqChoice = ecQuotient->matrix.getRowGroupIndices()[ecqState] + ecqOptimalChoices[ecqState];
        uint_fast64_t origChoice = ecQuotient->ecqToOriginalChoiceMapping[ecqChoice];
        auto const& origStates = ecQuotient->ecqToOriginalStateMapping[ecqState];
        STORM_LOG_ASSERT(!origStates.empty(), "Unexpected empty set of original states.");
        if (ecQuotient->ecqStayInEcChoices.get(ecqChoice)) {
            // We stay in the current state(s) forever (End component)
            // We need to set choices in a way that (i) the optimal LRA Mec is reached (if there is any) and (ii) 0 total reward is collected.
            if (!ecqStateToOptimalMecMap.empty()) {
                // The current ecqState represents an elimnated EC and we need to stay in this EC and we need to make sure that optimal MEC decisions are
                // performed within this EC.
                STORM_LOG_ASSERT(ecqStateToOptimalMecMap.count(ecqState) > 0, "No Lra Mec associated to given eliminated EC");
                auto const& lraMec = lraMecDecomposition->mecs[ecqStateToOptimalMecMap.at(ecqState)];
                if (lraMec.size() == origStates.size()) {
                    // LRA mec and eliminated EC coincide
                    for (auto const& state : origStates) {
                        STORM_LOG_ASSERT(lraMec.containsState(state), "Expected state to be contained in the lra mec.");
                        // Note that the optimal choice for this state has already been set in the infinite horizon phase.
                        unprocessedStates.set(state, false);
                        originalSolution[state] = ecqSolution[ecqState];
                    }
                } else {
                    // LRA mec is proper subset of eliminated ec. There are also other states for which we have to set choices leading to the LRA MEC inside.
                    STORM_LOG_ASSERT(lraMec.size() < origStates.size(), "Lra Mec (" << lraMec.size()
                                                                                    << " states) should be a proper subset of the eliminated ec ("
                                                                                    << origStates.size() << " states).");
                    for (auto const& state : origStates) {
                        if (lraMec.containsState(state)) {
                            ecStatesToReach.set(state, true);
                            // Note that the optimal choice for this state has already been set in the infinite horizon phase.
                        } else {
                            ecStatesToProcess.set(state, true);
                        }
                        unprocessedStates.set(state, false);
                        originalSolution[state] = ecqSolution[ecqState];
                    }
                    computeSchedulerProb1(transitionMatrix, backwardsTransitions, ecStatesToProcess, ecStatesToReach, originalOptimalChoices,
                                          &ecQuotient->origTotalReward0Choices);
                    // Clear bitvectors for next ecqState.
                    ecStatesToProcess.clear();
                    ecStatesToReach.clear();
                }
            } else {
                // If there is no LRA Mec to reach, we just need to make sure that finite total reward is collected for all objectives
                // In this branch our BitVectors have a slightly different meaning, so we create more readable aliases
                storm::storage::BitVector& ecStatesToAvoid = ecStatesToReach;
                bool needSchedulerComputation = false;
                STORM_LOG_ASSERT(storm::utility::isZero(ecqSolution[ecqState]),
                                 "Solution for state that stays inside EC must be zero. Got " << ecqSolution[ecqState] << " instead.");
                for (auto const& state : origStates) {
                    originalSolution[state] = storm::utility::zero<ValueType>();  // i.e. ecqSolution[ecqState];
                    ecStatesToProcess.set(state, true);
                }
                auto validChoices = transitionMatrix.getRowFilter(ecStatesToProcess, ecStatesToProcess);
                auto valid0RewardChoices = validChoices & actionsWithoutRewardInUnboundedPhase;
                for (auto const& state : origStates) {
                    auto groupStart = transitionMatrix.getRowGroupIndices()[state];
                    auto groupEnd = transitionMatrix.getRowGroupIndices()[state + 1];
                    auto nextValidChoice = valid0RewardChoices.getNextSetIndex(groupStart);
                    if (nextValidChoice < groupEnd) {
                        originalOptimalChoices[state] = nextValidChoice - groupStart;
                    } else {
                        // this state should not be reached infinitely often
                        ecStatesToAvoid.set(state, true);
                        needSchedulerComputation = true;
                    }
                }
                if (needSchedulerComputation) {
                    // There are ec states which we should not visit infinitely often
                    auto ecStatesThatCanAvoid =
                        storm::utility::graph::performProbGreater0A(transitionMatrix, transitionMatrix.getRowGroupIndices(), backwardsTransitions,
                                                                    ecStatesToProcess, ecStatesToAvoid, false, 0, valid0RewardChoices);
                    ecStatesThatCanAvoid.complement();
                    // Set the choice for all states that can achieve value 0
                    computeSchedulerProb0(transitionMatrix, backwardsTransitions, ecStatesThatCanAvoid, ecStatesToAvoid, valid0RewardChoices,
                                          originalOptimalChoices);
                    // Set the choice for all remaining states
                    computeSchedulerProb1(transitionMatrix, backwardsTransitions, ecStatesToProcess & ~ecStatesToAvoid, ecStatesToAvoid, originalOptimalChoices,
                                          &validChoices);
                }
                ecStatesToAvoid.clear();
                ecStatesToProcess.clear();
            }
        } else {
            // We eventually leave the current state(s)
            // In this case, we can safely take the origChoice at the corresponding state (say 's').
            // For all other origStates associated with ecqState (if there are any), we make sure that the state 's' is reached almost surely.
            if (origStates.size() > 1) {
                for (auto const& state : origStates) {
                    // Check if the orig choice originates from this state
                    auto groupStart = transitionMatrix.getRowGroupIndices()[state];
                    auto groupEnd = transitionMatrix.getRowGroupIndices()[state + 1];
                    if (origChoice >= groupStart && origChoice < groupEnd) {
                        originalOptimalChoices[state] = origChoice - groupStart;
                        ecStatesToReach.set(state, true);
                    } else {
                        STORM_LOG_ASSERT(origStates.size() > 1, "Multiple original states expected.");
                        ecStatesToProcess.set(state, true);
                    }
                    unprocessedStates.set(state, false);
                    originalSolution[state] = ecqSolution[ecqState];
                }
                auto validChoices = transitionMatrix.getRowFilter(ecStatesToProcess, ecStatesToProcess | ecStatesToReach);
                computeSchedulerProb1(transitionMatrix, backwardsTransitions, ecStatesToProcess, ecStatesToReach, originalOptimalChoices, &validChoices);
                // Clear bitvectors for next ecqState.
                ecStatesToProcess.clear();
                ecStatesToReach.clear();
            } else {
                // There is just one state so we take the associated choice.
                auto state = *origStates.begin();
                auto groupStart = transitionMatrix.getRowGroupIndices()[state];
                STORM_LOG_ASSERT(
                    origChoice >= groupStart && origChoice < transitionMatrix.getRowGroupIndices()[state + 1],
                    "Invalid choice: " << originalOptimalChoices[state] << " at a state with " << transitionMatrix.getRowGroupSize(state) << " choices.");
                originalOptimalChoices[state] = origChoice - groupStart;
                originalSolution[state] = ecqSolution[ecqState];
                unprocessedStates.set(state, false);
            }
        }
    }

    // The states that still not have been processed, there is no associated state of the ec quotient.
    // This is because the value for these states will be 0 under all (lra optimal-) schedulers.
    storm::utility::vector::setVectorValues(originalSolution, unprocessedStates, storm::utility::zero<ValueType>());
    // Get a set of states for which we know that no reward (for all objectives) will be collected
    if (this->lraMecDecomposition) {
        // In this case, all unprocessed non-lra mec states should reach an (unprocessed) lra mec
        for (auto const& mec : this->lraMecDecomposition->mecs) {
            for (auto const& sc : mec) {
                if (unprocessedStates.get(sc.first)) {
                    ecStatesToReach.set(sc.first, true);
                }
            }
        }
    } else {
        ecStatesToReach = unprocessedStates & totalReward0EStates;
        // Set a scheduler for the ecStates that we want to reach
        computeSchedulerProb0(transitionMatrix, backwardsTransitions, ecStatesToReach, ~unprocessedStates | ~totalReward0EStates,
                              actionsWithoutRewardInUnboundedPhase, originalOptimalChoices);
    }
    unprocessedStates &= ~ecStatesToReach;
    // Set a scheduler for the remaining states
    computeSchedulerProb1(transitionMatrix, backwardsTransitions, unprocessedStates, ecStatesToReach, originalOptimalChoices);
}

template class StandardPcaaWeightVectorChecker<storm::models::sparse::Mdp<double>>;
template class StandardPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<double>>;

template class StandardPcaaWeightVectorChecker<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class StandardPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
