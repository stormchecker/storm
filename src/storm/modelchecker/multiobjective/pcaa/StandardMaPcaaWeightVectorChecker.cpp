#include "storm/modelchecker/multiobjective/pcaa/StandardMaPcaaWeightVectorChecker.h"

#include <cmath>

#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/UncheckedRequirementException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/solver/SolverSelectionOptions.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

template<class SparseMaModelType>
StandardMaPcaaWeightVectorChecker<SparseMaModelType>::StandardMaPcaaWeightVectorChecker(
    preprocessing::SparseMultiObjectivePreprocessorResult<SparseMaModelType> const& preprocessorResult)
    : StandardPcaaWeightVectorChecker<SparseMaModelType>(preprocessorResult) {
    this->initialize(preprocessorResult);
}

template<class SparseMaModelType>
void StandardMaPcaaWeightVectorChecker<SparseMaModelType>::initializeModelTypeSpecificData(SparseMaModelType const& model) {
    markovianStates = model.getMarkovianStates();
    exitRates = model.getExitRates();

    // Set the (discretized) state action rewards.
    this->actionRewards.assign(this->objectives.size(), {});
    this->stateRewards.assign(this->objectives.size(), {});
    for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
        auto const& formula = *this->objectives[objIndex].formula;
        STORM_LOG_THROW(formula.isRewardOperatorFormula() && formula.asRewardOperatorFormula().hasRewardModelName(), storm::exceptions::UnexpectedException,
                        "Unexpected type of operator formula: " << formula << ".");
        typename SparseMaModelType::RewardModelType const& rewModel = model.getRewardModel(formula.asRewardOperatorFormula().getRewardModelName());
        STORM_LOG_ASSERT(!rewModel.hasTransitionRewards(), "Preprocessed Reward model has transition rewards which is not expected.");
        this->actionRewards[objIndex] = rewModel.hasStateActionRewards()
                                            ? rewModel.getStateActionRewardVector()
                                            : std::vector<ValueType>(model.getTransitionMatrix().getRowCount(), storm::utility::zero<ValueType>());
        if (formula.getSubformula().isTotalRewardFormula()) {
            if (rewModel.hasStateRewards()) {
                // Note that state rewards are earned over time and thus play no role for probabilistic states
                for (auto markovianState : markovianStates) {
                    this->actionRewards[objIndex][model.getTransitionMatrix().getRowGroupIndices()[markovianState]] +=
                        rewModel.getStateReward(markovianState) / exitRates[markovianState];
                }
            }
        } else if (formula.getSubformula().isLongRunAverageRewardFormula()) {
            // The LRA methods for MA require keeping track of state- and action rewards separately
            if (rewModel.hasStateRewards()) {
                this->stateRewards[objIndex] = rewModel.getStateRewardVector();
            }
        } else {
            STORM_LOG_THROW(formula.getSubformula().isCumulativeRewardFormula() &&
                                formula.getSubformula().asCumulativeRewardFormula().getTimeBoundReference().isTimeBound(),
                            storm::exceptions::UnexpectedException, "Unexpected type of sub-formula: " << formula.getSubformula() << ".");
            STORM_LOG_THROW(!rewModel.hasStateRewards(), storm::exceptions::InvalidPropertyException,
                            "Found state rewards for time bounded objective " << this->objectives[objIndex].originalFormula << ". This is not supported.");
            STORM_LOG_WARN_COND(
                this->objectives[objIndex].originalFormula->isProbabilityOperatorFormula() &&
                    this->objectives[objIndex].originalFormula->asProbabilityOperatorFormula().getSubformula().isBoundedUntilFormula(),
                "Objective " << this->objectives[objIndex].originalFormula
                             << " was simplified to a cumulative reward formula. Correctness of the algorithm is unknown for this type of property.");
        }
    }
    STORM_LOG_STATISTICS("Final preprocessed model has " << markovianStates.getNumberOfSetBits() << " Markovian states.\n");
}

template<class SparseMdpModelType>
storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<typename SparseMdpModelType::ValueType>
StandardMaPcaaWeightVectorChecker<SparseMdpModelType>::createNondetInfiniteHorizonHelper(storm::storage::SparseMatrix<ValueType> const& transitions) const {
    STORM_LOG_ASSERT(transitions.getRowGroupCount() == this->transitionMatrix.getRowGroupCount(), "Unexpected size of given matrix.");
    return storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType>(transitions, this->markovianStates, this->exitRates);
}

template<class SparseMdpModelType>
storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<typename SparseMdpModelType::ValueType>
StandardMaPcaaWeightVectorChecker<SparseMdpModelType>::createDetInfiniteHorizonHelper(storm::storage::SparseMatrix<ValueType> const& transitions) const {
    STORM_LOG_ASSERT(transitions.getRowGroupCount() == this->transitionMatrix.getRowGroupCount(), "Unexpected size of given matrix.");
    // TODO: Right now, there is no dedicated support for "deterministic" Markov automata so we have to pick the nondeterministic one.
    auto result = storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType>(transitions, this->markovianStates, this->exitRates);
    result.setOptimizationDirection(storm::solver::OptimizationDirection::Maximize);
    return result;
}

template<class SparseMaModelType>
typename SparseMaModelType::ValueType StandardMaPcaaWeightVectorChecker<SparseMaModelType>::getWeightedPrecisionUnboundedPhase() const {
    return this->getWeightedPrecision() - getWeightedPrecisionBoundedPhase();
}

template<class SparseMaModelType>
typename SparseMaModelType::ValueType StandardMaPcaaWeightVectorChecker<SparseMaModelType>::getWeightedPrecisionBoundedPhase() const {
    if (!this->objectivesWithNoUpperTimeBound.full()) {
        // If there are time-bounded objectives, we allow most of the approximation error in the bounded phase.
        // This is because computing time-bounded objectives accurately is likely the largest bottle neck of the computation.
        return storm::utility::convertNumber<ValueType>(0.99) * this->getWeightedPrecision();
    }
    return storm::utility::zero<ValueType>();
}

template<class SparseMaModelType>
bool StandardMaPcaaWeightVectorChecker<SparseMaModelType>::smallPrecisionsAreChallenging() const {
    return !this->objectivesWithNoUpperTimeBound.full();
}

template<class SparseMaModelType>
void StandardMaPcaaWeightVectorChecker<SparseMaModelType>::boundedPhase(Environment const& env, std::vector<ValueType> const& weightVector,
                                                                        std::vector<ValueType>& weightedRewardVector) {
    // Split the preprocessed model into transitions from/to probabilistic/Markovian states.
    SubModel MS = createSubModel(true, weightedRewardVector);
    SubModel PS = createSubModel(false, weightedRewardVector);

    // Apply digitization to Markovian transitions
    ValueType digitizationConstant = getDigitizationConstant(weightVector);
    digitize(MS, digitizationConstant);

    // Get for each occurring (digitized) timeBound the indices of the objectives with that bound.
    TimeBoundMap upperTimeBounds;
    digitizeTimeBounds(upperTimeBounds, digitizationConstant, weightVector);

    // Check whether there is a cycle in of probabilistic states
    bool acyclic = !storm::utility::graph::hasCycle(PS.toPS);

    // Initialize a minMaxSolver to compute an optimal scheduler (w.r.t. PS) for each epoch
    // No EC elimination is necessary as we assume non-zenoness
    std::unique_ptr<MinMaxSolverData> minMax = initMinMaxSolver(env, PS, acyclic, weightVector);

    // create a linear equation solver for the model induced by the optimal choice vector.
    // the solver will be updated whenever the optimal choice vector has changed.
    std::unique_ptr<LinEqSolverData> linEq = initLinEqSolver(env, PS, acyclic);

    // Store the optimal choices of PS as computed by the minMax solver.
    std::vector<uint_fast64_t> optimalChoicesAtCurrentEpoch(PS.getNumberOfStates(), std::numeric_limits<uint_fast64_t>::max());

    // Stores the objectives for which we need to compute values in the current time epoch.
    storm::storage::BitVector consideredObjectives = this->objectivesWithNoUpperTimeBound & ~this->lraObjectives;

    auto upperTimeBoundIt = upperTimeBounds.begin();
    uint_fast64_t currentEpoch = upperTimeBounds.empty() ? 0 : upperTimeBoundIt->first;
    while (!storm::utility::resources::isTerminate()) {
        // Update the objectives that are considered at the current time epoch as well as the (weighted) reward vectors.
        updateDataToCurrentEpoch(MS, PS, *minMax, consideredObjectives, currentEpoch, weightVector, upperTimeBoundIt, upperTimeBounds);

        // Compute the values that can be obtained at probabilistic states in the current time epoch
        performPSStep(env, PS, MS, *minMax, *linEq, optimalChoicesAtCurrentEpoch, consideredObjectives, weightVector);

        // Compute values that can be obtained at Markovian states after letting one (digitized) time unit pass.
        // Only perform such a step if there is time left.
        if (currentEpoch > 0) {
            performMSStep(env, MS, PS, consideredObjectives, weightVector);
            --currentEpoch;
        } else {
            break;
        }
    }
    STORM_LOG_WARN_COND(!storm::utility::resources::isTerminate(), "Time-bounded reachability computation aborted.");

    // compose the results from MS and PS
    storm::utility::vector::setVectorValues(this->weightedResult, MS.states, MS.weightedSolutionVector);
    storm::utility::vector::setVectorValues(this->weightedResult, PS.states, PS.weightedSolutionVector);
    for (uint_fast64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
        storm::utility::vector::setVectorValues(this->objectiveResults[objIndex], MS.states, MS.objectiveSolutionVectors[objIndex]);
        storm::utility::vector::setVectorValues(this->objectiveResults[objIndex], PS.states, PS.objectiveSolutionVectors[objIndex]);
    }
}

template<class SparseMaModelType>
typename StandardMaPcaaWeightVectorChecker<SparseMaModelType>::SubModel StandardMaPcaaWeightVectorChecker<SparseMaModelType>::createSubModel(
    bool createMS, std::vector<ValueType> const& weightedRewardVector) const {
    SubModel result;

    storm::storage::BitVector probabilisticStates = ~markovianStates;
    result.states = createMS ? markovianStates : probabilisticStates;
    result.choices = this->transitionMatrix.getRowFilter(result.states);
    STORM_LOG_ASSERT(!createMS || result.states.getNumberOfSetBits() == result.choices.getNumberOfSetBits(),
                     "row groups for Markovian states should consist of exactly one row");

    // We need to add diagonal entries for selfloops on Markovian states.
    result.toMS = this->transitionMatrix.getSubmatrix(true, result.states, markovianStates, createMS);
    result.toPS = this->transitionMatrix.getSubmatrix(true, result.states, probabilisticStates, false);
    STORM_LOG_ASSERT(result.getNumberOfStates() == result.states.getNumberOfSetBits() && result.getNumberOfStates() == result.toMS.getRowGroupCount() &&
                         result.getNumberOfStates() == result.toPS.getRowGroupCount(),
                     "Invalid state count for subsystem");
    STORM_LOG_ASSERT(result.getNumberOfChoices() == result.choices.getNumberOfSetBits() && result.getNumberOfChoices() == result.toMS.getRowCount() &&
                         result.getNumberOfChoices() == result.toPS.getRowCount(),
                     "Invalid choice count for subsystem");

    result.weightedRewardVector.resize(result.getNumberOfChoices());
    storm::utility::vector::selectVectorValues(result.weightedRewardVector, result.choices, weightedRewardVector);
    for (uint_fast64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
        std::vector<ValueType> const& objRewards = this->actionRewards[objIndex];
        std::vector<ValueType> subModelObjRewards;
        subModelObjRewards.reserve(result.getNumberOfChoices());
        for (auto choice : result.choices) {
            subModelObjRewards.push_back(objRewards[choice]);
        }
        result.objectiveRewardVectors.push_back(std::move(subModelObjRewards));
    }

    result.weightedSolutionVector.resize(result.getNumberOfStates());
    storm::utility::vector::selectVectorValues(result.weightedSolutionVector, result.states, this->weightedResult);
    result.objectiveSolutionVectors.resize(this->objectives.size());
    for (uint_fast64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
        result.objectiveSolutionVectors[objIndex].resize(result.weightedSolutionVector.size());
        storm::utility::vector::selectVectorValues(result.objectiveSolutionVectors[objIndex], result.states, this->objectiveResults[objIndex]);
    }

    result.auxChoiceValues.resize(result.getNumberOfChoices());

    return result;
}

template<class SparseMaModelType>
template<typename VT, typename std::enable_if<storm::NumberTraits<VT>::SupportsExponential, int>::type>
VT StandardMaPcaaWeightVectorChecker<SparseMaModelType>::getDigitizationConstant(std::vector<ValueType> const& weightVector) const {
    STORM_LOG_DEBUG("Retrieving digitization constant");
    // We need to find a delta such that for each objective it holds that lowerbound/delta , upperbound/delta are natural numbers and
    // sum_{obj_i} (
    //   If obj_i has a lower and an upper bound:
    //     weightVector_i * (1 - e^(-maxRate lowerbound) * (1 + maxRate delta) ^ (lowerbound / delta) + 1-e^(-maxRate upperbound) * (1 + maxRate delta) ^
    //     (upperbound / delta) + (1-e^(-maxRate delta)))
    //   If there is only an upper bound:
    //     weightVector_i * ( 1-e^(-maxRate upperbound) * (1 + maxRate delta) ^ (upperbound / delta))
    // ) <= this->maximumLowerUpperDistance

    // Initialize some data for fast and easy access
    VT const maxRate = storm::utility::vector::max_if(exitRates, markovianStates);
    std::vector<VT> timeBounds;
    std::vector<VT> eToPowerOfMinusMaxRateTimesBound;
    VT smallestNonZeroBound = storm::utility::zero<VT>();
    for (auto const& obj : this->objectives) {
        if (obj.formula->getSubformula().isCumulativeRewardFormula()) {
            timeBounds.push_back(obj.formula->getSubformula().asCumulativeRewardFormula().template getBound<VT>());
            STORM_LOG_THROW(!storm::utility::isZero(timeBounds.back()), storm::exceptions::InvalidPropertyException,
                            "Got zero-valued upper time bound. This is not suppoted.");
            eToPowerOfMinusMaxRateTimesBound.push_back(std::exp(-maxRate * timeBounds.back()));
            smallestNonZeroBound = storm::utility::isZero(smallestNonZeroBound) ? timeBounds.back() : std::min(smallestNonZeroBound, timeBounds.back());
        } else {
            timeBounds.push_back(storm::utility::zero<VT>());
            eToPowerOfMinusMaxRateTimesBound.push_back(storm::utility::zero<VT>());
        }
    }
    if (storm::utility::isZero(smallestNonZeroBound)) {
        // There are no time bounds. In this case, one is a valid digitization constant.
        return storm::utility::one<VT>();
    }
    VT weightedGoalPrecision = this->getWeightedPrecisionBoundedPhase() * storm::utility::sqrt(storm::utility::vector::dotProduct(weightVector, weightVector));

    // We brute-force a delta, since a direct computation is apparently not easy.
    // Also note that the number of times this loop runs is a lower bound for the number of minMaxSolver invocations.
    // Hence, this brute-force approach will most likely not be a bottleneck.
    storm::storage::BitVector objectivesWithTimeBound = ~this->objectivesWithNoUpperTimeBound;
    uint_fast64_t smallestStepBound = 1;
    VT delta = smallestNonZeroBound / smallestStepBound;
    while (true) {
        bool deltaValid = true;
        for (auto objIndex : objectivesWithTimeBound) {
            auto const& timeBound = timeBounds[objIndex];
            if (timeBound / delta != std::floor(timeBound / delta)) {
                deltaValid = false;
                break;
            }
        }
        if (deltaValid) {
            VT weightedPrecisionForCurrentDelta = storm::utility::zero<VT>();
            for (uint_fast64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
                VT precisionOfObj = storm::utility::zero<VT>();
                if (objectivesWithTimeBound.get(objIndex)) {
                    precisionOfObj +=
                        storm::utility::one<VT>() - (eToPowerOfMinusMaxRateTimesBound[objIndex] *
                                                     storm::utility::pow(storm::utility::one<VT>() + maxRate * delta, timeBounds[objIndex] / delta));
                }
                weightedPrecisionForCurrentDelta += weightVector[objIndex] * precisionOfObj;
            }
            deltaValid &= weightedPrecisionForCurrentDelta <= weightedGoalPrecision;
        }
        if (deltaValid) {
            break;
        }
        ++smallestStepBound;
        STORM_LOG_ASSERT(delta > smallestNonZeroBound / smallestStepBound, "Digitization constant is expected to become smaller in every iteration");
        delta = smallestNonZeroBound / smallestStepBound;
    }
    STORM_LOG_DEBUG("Found digitization constant: " << delta << ". At least " << smallestStepBound << " digitization steps will be necessarry");
    return delta;
}

template<class SparseMaModelType>
template<typename VT, typename std::enable_if<!storm::NumberTraits<VT>::SupportsExponential, int>::type>
VT StandardMaPcaaWeightVectorChecker<SparseMaModelType>::getDigitizationConstant(std::vector<ValueType> const& /*weightVector*/) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Computing bounded probabilities of MAs is unsupported for this value type.");
}

template<class SparseMaModelType>
template<typename VT, typename std::enable_if<storm::NumberTraits<VT>::SupportsExponential, int>::type>
void StandardMaPcaaWeightVectorChecker<SparseMaModelType>::digitize(SubModel& MS, VT const& digitizationConstant) const {
    std::vector<VT> rateVector(MS.getNumberOfChoices());
    storm::utility::vector::selectVectorValues(rateVector, MS.states, exitRates);
    for (uint_fast64_t row = 0; row < rateVector.size(); ++row) {
        VT const eToMinusRateTimesDelta = std::exp(-rateVector[row] * digitizationConstant);
        for (auto& entry : MS.toMS.getRow(row)) {
            entry.setValue((storm::utility::one<VT>() - eToMinusRateTimesDelta) * entry.getValue());
            if (entry.getColumn() == row) {
                entry.setValue(entry.getValue() + eToMinusRateTimesDelta);
            }
        }
        for (auto& entry : MS.toPS.getRow(row)) {
            entry.setValue((storm::utility::one<VT>() - eToMinusRateTimesDelta) * entry.getValue());
        }
        MS.weightedRewardVector[row] *= storm::utility::one<VT>() - eToMinusRateTimesDelta;
        for (auto& objVector : MS.objectiveRewardVectors) {
            objVector[row] *= storm::utility::one<VT>() - eToMinusRateTimesDelta;
        }
    }
}

template<class SparseMaModelType>
template<typename VT, typename std::enable_if<!storm::NumberTraits<VT>::SupportsExponential, int>::type>
void StandardMaPcaaWeightVectorChecker<SparseMaModelType>::digitize(SubModel& /*subModel*/, VT const& /*digitizationConstant*/) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Computing bounded probabilities of MAs is unsupported for this value type.");
}

template<class SparseMaModelType>
void StandardMaPcaaWeightVectorChecker<SparseMaModelType>::digitizeTimeBounds(TimeBoundMap& upperTimeBounds, ValueType const& digitizationConstant,
                                                                              std::vector<ValueType> const& weightVector) {
    if constexpr (storm::NumberTraits<ValueType>::SupportsExponential) {
        ValueType const maxRate = storm::utility::vector::max_if(exitRates, markovianStates);
        for (auto objIndex : ~this->objectivesWithNoUpperTimeBound) {
            auto const& obj = this->objectives[objIndex];
            ValueType errorTowardsZero = storm::utility::zero<ValueType>();
            ValueType errorAwayFromZero = storm::utility::zero<ValueType>();
            if (obj.formula->getSubformula().isCumulativeRewardFormula()) {
                ValueType timeBound = obj.formula->getSubformula().asCumulativeRewardFormula().template getBound<ValueType>();
                uint_fast64_t digitizedBound = storm::utility::convertNumber<uint_fast64_t>(timeBound / digitizationConstant);
                auto timeBoundIt = upperTimeBounds.insert(std::make_pair(digitizedBound, storm::storage::BitVector(this->objectives.size(), false))).first;
                timeBoundIt->second.set(objIndex);
                ValueType digitizationError = storm::utility::one<ValueType>();
                digitizationError -=
                    std::exp(-maxRate * timeBound) * storm::utility::pow(storm::utility::one<ValueType>() + maxRate * digitizationConstant, digitizedBound);
                errorAwayFromZero += digitizationError;
            }
            if (storm::solver::maximize(obj.formula->getOptimalityType())) {
                this->offsetsToAchievablePoint[objIndex] = -errorTowardsZero;
                this->offsetToWeightedSum += weightVector[objIndex] * errorAwayFromZero;
            } else {
                this->offsetsToAchievablePoint[objIndex] = errorAwayFromZero;
                this->offsetToWeightedSum += weightVector[objIndex] * errorTowardsZero;
            }
        }
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Computing bounded probabilities of MAs is unsupported for this value type.");
    }
}

template<class SparseMaModelType>
std::unique_ptr<typename StandardMaPcaaWeightVectorChecker<SparseMaModelType>::MinMaxSolverData>
StandardMaPcaaWeightVectorChecker<SparseMaModelType>::initMinMaxSolver(Environment const& env, SubModel const& PS, bool acyclic,
                                                                       std::vector<ValueType> const& weightVector) const {
    std::unique_ptr<MinMaxSolverData> result(new MinMaxSolverData());
    result->env = std::make_unique<storm::Environment>(env);
    // For acyclic models we switch to the more efficient acyclic solver (Unless the solver / method was explicitly specified)
    if (acyclic) {
        result->env->solver().minMax().setMethod(storm::solver::MinMaxMethod::Acyclic);
    }
    storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType> minMaxSolverFactory;
    result->solver = minMaxSolverFactory.create(*result->env, PS.toPS);
    result->solver->setHasUniqueSolution(true);
    result->solver->setHasNoEndComponents(true);  // Non-zeno MA
    result->solver->setTrackScheduler(true);
    result->solver->setCachingEnabled(true);
    auto req = result->solver->getRequirements(*result->env, storm::solver::OptimizationDirection::Maximize, false);
    boost::optional<ValueType> lowerBound = this->computeWeightedResultBound(true, weightVector, storm::storage::BitVector(weightVector.size(), true));
    if (lowerBound) {
        result->solver->setLowerBound(lowerBound.get());
        req.clearLowerBounds();
    }
    boost::optional<ValueType> upperBound = this->computeWeightedResultBound(false, weightVector, storm::storage::BitVector(weightVector.size(), true));
    if (upperBound) {
        result->solver->setUpperBound(upperBound.get());
        req.clearUpperBounds();
    }
    if (acyclic) {
        req.clearAcyclic();
    }
    STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                    "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
    result->solver->setRequirementsChecked(true);
    result->solver->setOptimizationDirection(storm::solver::OptimizationDirection::Maximize);

    result->b.resize(PS.getNumberOfChoices());

    return result;
}

template<class SparseMaModelType>
template<typename VT, typename std::enable_if<storm::NumberTraits<VT>::SupportsExponential, int>::type>
std::unique_ptr<typename StandardMaPcaaWeightVectorChecker<SparseMaModelType>::LinEqSolverData>
StandardMaPcaaWeightVectorChecker<SparseMaModelType>::initLinEqSolver(Environment const& env, SubModel const& PS, bool acyclic) const {
    std::unique_ptr<LinEqSolverData> result(new LinEqSolverData());
    result->env = std::make_unique<Environment>(env);
    result->acyclic = acyclic;
    // For acyclic models we switch to the more efficient acyclic solver (Unless the solver / method was explicitly specified)
    if (acyclic) {
        result->env->solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Acyclic);
    }
    result->factory = std::make_unique<storm::solver::GeneralLinearEquationSolverFactory<ValueType>>();
    result->b.resize(PS.getNumberOfStates());
    return result;
}

template<class SparseMaModelType>
template<typename VT, typename std::enable_if<!storm::NumberTraits<VT>::SupportsExponential, int>::type>
std::unique_ptr<typename StandardMaPcaaWeightVectorChecker<SparseMaModelType>::LinEqSolverData>
StandardMaPcaaWeightVectorChecker<SparseMaModelType>::initLinEqSolver(Environment const& /*env*/, SubModel const& /*PS*/, bool /*acyclic*/) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Computing bounded probabilities of MAs is unsupported for this value type.");
}

template<class SparseMaModelType>
void StandardMaPcaaWeightVectorChecker<SparseMaModelType>::updateDataToCurrentEpoch(
    SubModel& MS, SubModel& PS, MinMaxSolverData& minMax, storm::storage::BitVector& consideredObjectives, uint_fast64_t const& currentEpoch,
    std::vector<ValueType> const& weightVector, TimeBoundMap::iterator& upperTimeBoundIt, TimeBoundMap const& upperTimeBounds) {
    if (upperTimeBoundIt != upperTimeBounds.end() && currentEpoch == upperTimeBoundIt->first) {
        consideredObjectives |= upperTimeBoundIt->second;
        for (auto objIndex : upperTimeBoundIt->second) {
            // This objective now plays a role in the weighted sum
            ValueType factor =
                storm::solver::minimize(this->objectives[objIndex].formula->getOptimalityType()) ? -weightVector[objIndex] : weightVector[objIndex];
            storm::utility::vector::addScaledVector(MS.weightedRewardVector, MS.objectiveRewardVectors[objIndex], factor);
            storm::utility::vector::addScaledVector(PS.weightedRewardVector, PS.objectiveRewardVectors[objIndex], factor);
        }
        ++upperTimeBoundIt;
    }

    // Update the solver data
    PS.toMS.multiplyWithVector(MS.weightedSolutionVector, minMax.b);
    storm::utility::vector::addVectors(minMax.b, PS.weightedRewardVector, minMax.b);
}

template<class SparseMaModelType>
void StandardMaPcaaWeightVectorChecker<SparseMaModelType>::performPSStep(Environment const& env, SubModel& PS, SubModel const& MS, MinMaxSolverData& minMax,
                                                                         LinEqSolverData& linEq, std::vector<uint_fast64_t>& optimalChoicesAtCurrentEpoch,
                                                                         storm::storage::BitVector const& consideredObjectives,
                                                                         std::vector<ValueType> const& weightVector) const {
    // compute a choice vector for the probabilistic states that is optimal w.r.t. the weighted reward vector
    minMax.solver->solveEquations(*minMax.env, PS.weightedSolutionVector, minMax.b);
    auto const& newChoices = minMax.solver->getSchedulerChoices();
    if (consideredObjectives.getNumberOfSetBits() == 1 && storm::utility::isOne(weightVector[*consideredObjectives.begin()])) {
        // In this case there is no need to perform the computation on the individual objectives
        optimalChoicesAtCurrentEpoch = newChoices;
        PS.objectiveSolutionVectors[*consideredObjectives.begin()] = PS.weightedSolutionVector;
        if (storm::solver::minimize(this->objectives[*consideredObjectives.begin()].formula->getOptimalityType())) {
            storm::utility::vector::scaleVectorInPlace(PS.objectiveSolutionVectors[*consideredObjectives.begin()], -storm::utility::one<ValueType>());
        }
    } else {
        // check whether the linEqSolver needs to be updated, i.e., whether the scheduler has changed
        if (linEq.solver == nullptr || newChoices != optimalChoicesAtCurrentEpoch) {
            optimalChoicesAtCurrentEpoch = newChoices;
            linEq.solver = nullptr;
            bool needEquationSystem = linEq.factory->getEquationProblemFormat(*linEq.env) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem;
            storm::storage::SparseMatrix<ValueType> linEqMatrix = PS.toPS.selectRowsFromRowGroups(optimalChoicesAtCurrentEpoch, needEquationSystem);
            if (needEquationSystem) {
                linEqMatrix.convertToEquationSystem();
            }
            linEq.solver = linEq.factory->create(*linEq.env, std::move(linEqMatrix));
            linEq.solver->setCachingEnabled(true);
            auto req = linEq.solver->getRequirements(*linEq.env);
            if (linEq.acyclic) {
                req.clearAcyclic();
            }
            STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                            "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
        }

        // Get the results for the individual objectives.
        // Note that we do not consider an estimate for each objective (as done in the unbounded phase) since the results from the previous epoch are already
        // pretty close
        for (auto objIndex : consideredObjectives) {
            auto const& objectiveRewardVectorPS = PS.objectiveRewardVectors[objIndex];
            auto const& objectiveSolutionVectorMS = MS.objectiveSolutionVectors[objIndex];
            // compute rhs of equation system, i.e., PS.toMS * x + Rewards
            // To safe some time, only do this for the obtained optimal choices
            auto itGroupIndex = PS.toPS.getRowGroupIndices().begin();
            auto itChoiceOffset = optimalChoicesAtCurrentEpoch.begin();
            for (auto& bValue : linEq.b) {
                uint_fast64_t row = (*itGroupIndex) + (*itChoiceOffset);
                bValue = objectiveRewardVectorPS[row];
                for (auto const& entry : PS.toMS.getRow(row)) {
                    bValue += entry.getValue() * objectiveSolutionVectorMS[entry.getColumn()];
                }
                ++itGroupIndex;
                ++itChoiceOffset;
            }
            linEq.solver->solveEquations(*linEq.env, PS.objectiveSolutionVectors[objIndex], linEq.b);
        }
    }
}

template<class SparseMaModelType>
void StandardMaPcaaWeightVectorChecker<SparseMaModelType>::performMSStep(Environment const& env, SubModel& MS, SubModel const& PS,
                                                                         storm::storage::BitVector const& consideredObjectives,
                                                                         std::vector<ValueType> const& weightVector) const {
    MS.toMS.multiplyWithVector(MS.weightedSolutionVector, MS.auxChoiceValues);
    storm::utility::vector::addVectors(MS.weightedRewardVector, MS.auxChoiceValues, MS.weightedSolutionVector);
    MS.toPS.multiplyWithVector(PS.weightedSolutionVector, MS.auxChoiceValues);
    storm::utility::vector::addVectors(MS.weightedSolutionVector, MS.auxChoiceValues, MS.weightedSolutionVector);
    if (consideredObjectives.getNumberOfSetBits() == 1 && storm::utility::isOne(weightVector[*consideredObjectives.begin()])) {
        // In this case there is no need to perform the computation on the individual objectives
        MS.objectiveSolutionVectors[*consideredObjectives.begin()] = MS.weightedSolutionVector;
        if (storm::solver::minimize(this->objectives[*consideredObjectives.begin()].formula->getOptimalityType())) {
            storm::utility::vector::scaleVectorInPlace(MS.objectiveSolutionVectors[*consideredObjectives.begin()], -storm::utility::one<ValueType>());
        }
    } else {
        for (auto objIndex : consideredObjectives) {
            MS.toMS.multiplyWithVector(MS.objectiveSolutionVectors[objIndex], MS.auxChoiceValues);
            storm::utility::vector::addVectors(MS.objectiveRewardVectors[objIndex], MS.auxChoiceValues, MS.objectiveSolutionVectors[objIndex]);
            MS.toPS.multiplyWithVector(PS.objectiveSolutionVectors[objIndex], MS.auxChoiceValues);
            storm::utility::vector::addVectors(MS.objectiveSolutionVectors[objIndex], MS.auxChoiceValues, MS.objectiveSolutionVectors[objIndex]);
        }
    }
}

template class StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<double>>;
template double StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<double>>::getDigitizationConstant<double>(
    std::vector<double> const& direction) const;
template void StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<double>>::digitize<double>(
    StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<double>>::SubModel& subModel, double const& digitizationConstant) const;
template std::unique_ptr<typename StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<double>>::LinEqSolverData>
StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<double>>::initLinEqSolver<double>(
    Environment const& env, StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<double>>::SubModel const& PS, bool acyclic) const;

template class StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;
template storm::RationalNumber StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>::getDigitizationConstant<
    storm::RationalNumber>(std::vector<storm::RationalNumber> const& direction) const;
template void StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>::digitize<storm::RationalNumber>(
    StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>::SubModel& subModel,
    storm::RationalNumber const& digitizationConstant) const;
template std::unique_ptr<typename StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>::LinEqSolverData>
StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>::initLinEqSolver<storm::RationalNumber>(
    Environment const& env, StandardMaPcaaWeightVectorChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>::SubModel const& PS,
    bool acyclic) const;

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
