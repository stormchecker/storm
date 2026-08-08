#include "storm-pars/modelchecker/region/SparseMdpParameterLiftingModelChecker.h"

#include "storm-pars/modelchecker/region/AnnotatedRegion.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityBackend.h"
#include "storm-pars/transformer/SparseParametricMdpSimplifier.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/solver/StandardGameSolver.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm::modelchecker {

template<typename SparseModelType, typename ConstantType>
SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::SparseMdpParameterLiftingModelChecker()
    : SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>(std::make_unique<storm::solver::GameSolverFactory<ConstantType>>()) {
    // Intentionally left empty
}

template<typename SparseModelType, typename ConstantType>
SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::SparseMdpParameterLiftingModelChecker(
    std::unique_ptr<storm::solver::GameSolverFactory<ConstantType>>&& solverFactory)
    : solverFactory(std::move(solverFactory)) {
    // Intentionally left empty
}

template<typename SparseModelType, typename ConstantType>
bool SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::canHandle(std::shared_ptr<storm::models::ModelBase> parametricModel,
                                                                                     CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const {
    bool result = parametricModel->isOfType(storm::models::ModelType::Mdp);
    result &= parametricModel->isSparseModel();
    result &= parametricModel->supportsParameters();
    auto mdp = parametricModel->template as<SparseModelType>();
    result &= static_cast<bool>(mdp);
    result &= checkTask.getFormula().isInFragment(storm::logic::reachability()
                                                      .setRewardOperatorsAllowed(true)
                                                      .setReachabilityRewardFormulasAllowed(true)
                                                      .setBoundedUntilFormulasAllowed(true)
                                                      .setCumulativeRewardFormulasAllowed(true)
                                                      .setStepBoundedUntilFormulasAllowed(true)
                                                      .setTimeBoundedCumulativeRewardFormulasAllowed(true)
                                                      .setStepBoundedCumulativeRewardFormulasAllowed(true)
                                                      .setTimeBoundedUntilFormulasAllowed(true));
    return result;
}

template<typename SparseModelType, typename ConstantType>
void SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::specify(Environment const& env,
                                                                                   std::shared_ptr<storm::models::ModelBase> parametricModel,
                                                                                   CheckTask<storm::logic::Formula, ParametricType> const& checkTask,
                                                                                   std::optional<RegionSplitEstimateKind> generateRegionSplitEstimates,
                                                                                   std::shared_ptr<MonotonicityBackend<ParametricType>> monotonicityBackend,
                                                                                   bool allowModelSimplifications, bool graphPreserving) {
    STORM_LOG_THROW(this->canHandle(parametricModel, checkTask), storm::exceptions::NotSupportedException,
                    "Combination of model " << parametricModel->getType() << " and formula '" << checkTask.getFormula() << "' is not supported.");
    STORM_LOG_THROW(graphPreserving, storm::exceptions::NotImplementedException, "Non-graph-preserving regions not implemented for MDPs.");
    this->specifySplitEstimates(generateRegionSplitEstimates, checkTask);
    this->specifyMonotonicity(monotonicityBackend, checkTask);
    auto mdp = parametricModel->template as<SparseModelType>();
    reset();

    if (allowModelSimplifications) {
        auto simplifier = storm::transformer::SparseParametricMdpSimplifier<SparseModelType>(*mdp);
        STORM_LOG_THROW(simplifier.simplify(checkTask.getFormula()), storm::exceptions::UnexpectedException, "Simplifying the model was not successfull.");
        this->parametricModel = simplifier.getSimplifiedModel();
        this->specifyFormula(env, checkTask.substituteFormula(*simplifier.getSimplifiedFormula()));
    } else {
        this->parametricModel = mdp;
        this->specifyFormula(env, checkTask);
    }

    std::shared_ptr<storm::logic::Formula> formulaWithoutBounds = this->currentCheckTask->getFormula().clone();
    formulaWithoutBounds->asOperatorFormula().removeBound();
    this->currentFormulaNoBound = formulaWithoutBounds->asSharedPointer();
    this->currentCheckTaskNoBound = std::make_unique<storm::modelchecker::CheckTask<storm::logic::Formula, ParametricType>>(*this->currentFormulaNoBound);
}

template<typename SparseModelType, typename ConstantType>
void SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::specifyBoundedUntilFormula(
    const CheckTask<logic::BoundedUntilFormula, ConstantType>& checkTask) {
    // get the step bound
    STORM_LOG_THROW(!checkTask.getFormula().hasLowerBound(), storm::exceptions::NotSupportedException, "Lower step bounds are not supported.");
    STORM_LOG_THROW(checkTask.getFormula().hasUpperBound(), storm::exceptions::NotSupportedException, "Expected a bounded until formula with an upper bound.");
    STORM_LOG_THROW(checkTask.getFormula().getTimeBoundReference().isStepBound(), storm::exceptions::NotSupportedException,
                    "Expected a bounded until formula with step bounds.");
    stepBound = checkTask.getFormula().getUpperBound().evaluateAsInt();
    STORM_LOG_THROW(*stepBound > 0, storm::exceptions::NotSupportedException,
                    "Can not apply parameter lifting on step bounded formula: The step bound has to be positive.");
    if (checkTask.getFormula().isUpperBoundStrict()) {
        STORM_LOG_THROW(*stepBound > 0, storm::exceptions::NotSupportedException, "Expected a strict upper step bound that is greater than zero.");
        --(*stepBound);
    }
    STORM_LOG_THROW(*stepBound > 0, storm::exceptions::NotSupportedException,
                    "Can not apply parameter lifting on step bounded formula: The step bound has to be positive.");

    // get the results for the subformulas
    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> propositionalChecker(*this->parametricModel);
    STORM_LOG_THROW(propositionalChecker.canHandle(checkTask.getFormula().getLeftSubformula()) &&
                        propositionalChecker.canHandle(checkTask.getFormula().getRightSubformula()),
                    storm::exceptions::NotSupportedException, "Parameter lifting with non-propositional subformulas is not supported.");
    storm::storage::BitVector phiStates = std::move(propositionalChecker.check(checkTask.getFormula().getLeftSubformula())
                                                        ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                        .getTruthValuesVector());
    storm::storage::BitVector psiStates = std::move(propositionalChecker.check(checkTask.getFormula().getRightSubformula())
                                                        ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                        .getTruthValuesVector());

    // get the maybeStates
    maybeStates = storm::solver::minimize(checkTask.getOptimizationDirection())
                      ? storm::utility::graph::performProbGreater0A(this->parametricModel->getTransitionMatrix(),
                                                                    this->parametricModel->getTransitionMatrix().getRowGroupIndices(),
                                                                    this->parametricModel->getBackwardTransitions(), phiStates, psiStates, true, *stepBound)
                      : storm::utility::graph::performProbGreater0E(this->parametricModel->getBackwardTransitions(), phiStates, psiStates, true, *stepBound);
    maybeStates &= ~psiStates;

    // set the result for all non-maybe states
    resultsForNonMaybeStates = std::vector<ConstantType>(this->parametricModel->getNumberOfStates(), storm::utility::zero<ConstantType>());
    storm::utility::vector::setVectorValues(resultsForNonMaybeStates, psiStates, storm::utility::one<ConstantType>());

    // if there are maybestates, create the parameterLifter
    if (!maybeStates.empty()) {
        // Create the vector of one-step probabilities to go to target states.
        std::vector<ParametricType> b = this->parametricModel->getTransitionMatrix().getConstrainedRowSumVector(
            storm::storage::BitVector(this->parametricModel->getTransitionMatrix().getRowCount(), true), psiStates);

        parameterLifter = std::make_unique<storm::transformer::ParameterLifter<ParametricType, ConstantType>>(
            this->parametricModel->getTransitionMatrix(), b, this->parametricModel->getTransitionMatrix().getRowFilter(maybeStates), maybeStates);
        computePlayer1Matrix();

        applyPreviousResultAsHint = false;
    }

    // We know some bounds for the results
    lowerResultBound = storm::utility::zero<ConstantType>();
    upperResultBound = storm::utility::one<ConstantType>();
}

template<typename SparseModelType, typename ConstantType>
void SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::specifyUntilFormula(
    Environment const& env, CheckTask<storm::logic::UntilFormula, ConstantType> const& checkTask) {
    // get the results for the subformulas
    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> propositionalChecker(*this->parametricModel);
    STORM_LOG_THROW(propositionalChecker.canHandle(checkTask.getFormula().getLeftSubformula()) &&
                        propositionalChecker.canHandle(checkTask.getFormula().getRightSubformula()),
                    storm::exceptions::NotSupportedException, "Parameter lifting with non-propositional subformulas is not supported.");
    storm::storage::BitVector phiStates = std::move(propositionalChecker.check(checkTask.getFormula().getLeftSubformula())
                                                        ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                        .getTruthValuesVector());
    storm::storage::BitVector psiStates = std::move(propositionalChecker.check(checkTask.getFormula().getRightSubformula())
                                                        ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                        .getTruthValuesVector());

    // get the maybeStates
    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 =
        storm::solver::minimize(checkTask.getOptimizationDirection())
            ? storm::utility::graph::performProb01Min(this->parametricModel->getTransitionMatrix(),
                                                      this->parametricModel->getTransitionMatrix().getRowGroupIndices(),
                                                      this->parametricModel->getBackwardTransitions(), phiStates, psiStates)
            : storm::utility::graph::performProb01Max(this->parametricModel->getTransitionMatrix(),
                                                      this->parametricModel->getTransitionMatrix().getRowGroupIndices(),
                                                      this->parametricModel->getBackwardTransitions(), phiStates, psiStates);
    maybeStates = ~(statesWithProbability01.first | statesWithProbability01.second);

    // set the result for all non-maybe states
    resultsForNonMaybeStates = std::vector<ConstantType>(this->parametricModel->getNumberOfStates(), storm::utility::zero<ConstantType>());
    storm::utility::vector::setVectorValues(resultsForNonMaybeStates, statesWithProbability01.second, storm::utility::one<ConstantType>());

    // if there are maybestates, create the parameterLifter
    if (!maybeStates.empty()) {
        // Create the vector of one-step probabilities to go to target states.
        std::vector<ParametricType> b = this->parametricModel->getTransitionMatrix().getConstrainedRowSumVector(
            storm::storage::BitVector(this->parametricModel->getTransitionMatrix().getRowCount(), true), statesWithProbability01.second);

        parameterLifter = std::make_unique<storm::transformer::ParameterLifter<ParametricType, ConstantType>>(
            this->parametricModel->getTransitionMatrix(), b, this->parametricModel->getTransitionMatrix().getRowFilter(maybeStates), maybeStates);
        computePlayer1Matrix();

        // Check whether there is an EC consisting of maybestates
        applyPreviousResultAsHint =
            storm::solver::minimize(checkTask.getOptimizationDirection()) ||  // when minimizing, there can not be an EC within the maybestates
            storm::utility::graph::performProb1A(this->parametricModel->getTransitionMatrix(),
                                                 this->parametricModel->getTransitionMatrix().getRowGroupIndices(),
                                                 this->parametricModel->getBackwardTransitions(), maybeStates, ~maybeStates)
                .full();
    }

    // We know some bounds for the results
    lowerResultBound = storm::utility::zero<ConstantType>();
    upperResultBound = storm::utility::one<ConstantType>();
}

template<typename SparseModelType, typename ConstantType>
void SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::specifyReachabilityRewardFormula(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ConstantType> const& checkTask) {
    // get the results for the subformula
    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> propositionalChecker(*this->parametricModel);
    STORM_LOG_THROW(propositionalChecker.canHandle(checkTask.getFormula().getSubformula()), storm::exceptions::NotSupportedException,
                    "Parameter lifting with non-propositional subformulas is not supported.");
    storm::storage::BitVector targetStates = std::move(propositionalChecker.check(checkTask.getFormula().getSubformula())
                                                           ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                           .getTruthValuesVector());

    // get the maybeStates
    storm::storage::BitVector infinityStates =
        storm::solver::minimize(checkTask.getOptimizationDirection())
            ? storm::utility::graph::performProb1E(
                  this->parametricModel->getTransitionMatrix(), this->parametricModel->getTransitionMatrix().getRowGroupIndices(),
                  this->parametricModel->getBackwardTransitions(), storm::storage::BitVector(this->parametricModel->getNumberOfStates(), true), targetStates)
            : storm::utility::graph::performProb1A(
                  this->parametricModel->getTransitionMatrix(), this->parametricModel->getTransitionMatrix().getRowGroupIndices(),
                  this->parametricModel->getBackwardTransitions(), storm::storage::BitVector(this->parametricModel->getNumberOfStates(), true), targetStates);
    infinityStates.complement();
    maybeStates = ~(targetStates | infinityStates);

    // set the result for all the non-maybe states
    resultsForNonMaybeStates = std::vector<ConstantType>(this->parametricModel->getNumberOfStates(), storm::utility::zero<ConstantType>());
    storm::utility::vector::setVectorValues(resultsForNonMaybeStates, infinityStates, storm::utility::infinity<ConstantType>());

    // if there are maybestates, create the parameterLifter
    if (!maybeStates.empty()) {
        // Create the reward vector
        STORM_LOG_THROW((checkTask.isRewardModelSet() && this->parametricModel->hasRewardModel(checkTask.getRewardModel())) ||
                            (!checkTask.isRewardModelSet() && this->parametricModel->hasUniqueRewardModel()),
                        storm::exceptions::InvalidPropertyException, "The reward model specified by the CheckTask is not available in the given model.");

        typename SparseModelType::RewardModelType const& rewardModel =
            checkTask.isRewardModelSet() ? this->parametricModel->getRewardModel(checkTask.getRewardModel()) : this->parametricModel->getUniqueRewardModel();

        std::vector<ParametricType> b = rewardModel.getTotalRewardVector(this->parametricModel->getTransitionMatrix());

        // We need to handle choices that lead to an infinity state.
        // As a maybeState does not have reward infinity, a choice leading to an infinity state will never be picked. Hence, we can unselect the
        // corresponding rows
        storm::storage::BitVector selectedRows = this->parametricModel->getTransitionMatrix().getRowFilter(maybeStates, ~infinityStates);

        parameterLifter = std::make_unique<storm::transformer::ParameterLifter<ParametricType, ConstantType>>(this->parametricModel->getTransitionMatrix(), b,
                                                                                                              selectedRows, maybeStates);
        computePlayer1Matrix(selectedRows);

        // Check whether there is an EC consisting of maybestates
        applyPreviousResultAsHint =
            !storm::solver::minimize(checkTask.getOptimizationDirection()) ||  // when maximizing, there can not be an EC within the maybestates
            storm::utility::graph::performProb1A(this->parametricModel->getTransitionMatrix(),
                                                 this->parametricModel->getTransitionMatrix().getRowGroupIndices(),
                                                 this->parametricModel->getBackwardTransitions(), maybeStates, ~maybeStates)
                .full();
    }

    // We only know a lower bound for the result
    lowerResultBound = storm::utility::zero<ConstantType>();
}

template<typename SparseModelType, typename ConstantType>
void SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::specifyCumulativeRewardFormula(
    const CheckTask<logic::CumulativeRewardFormula, ConstantType>& checkTask) {
    // Obtain the stepBound
    stepBound = checkTask.getFormula().getBound().evaluateAsInt();
    if (checkTask.getFormula().isBoundStrict()) {
        STORM_LOG_THROW(*stepBound > 0, storm::exceptions::NotSupportedException, "Expected a strict upper step bound that is greater than zero.");
        --(*stepBound);
    }
    STORM_LOG_THROW(*stepBound > 0, storm::exceptions::NotSupportedException,
                    "Can not apply parameter lifting on step bounded formula: The step bound has to be positive.");

    // Every state is a maybeState
    maybeStates = storm::storage::BitVector(this->parametricModel->getTransitionMatrix().getColumnCount(), true);
    resultsForNonMaybeStates = std::vector<ConstantType>(this->parametricModel->getNumberOfStates());

    // Create the reward vector
    STORM_LOG_THROW((checkTask.isRewardModelSet() && this->parametricModel->hasRewardModel(checkTask.getRewardModel())) ||
                        (!checkTask.isRewardModelSet() && this->parametricModel->hasUniqueRewardModel()),
                    storm::exceptions::InvalidPropertyException, "The reward model specified by the CheckTask is not available in the given model.");
    typename SparseModelType::RewardModelType const& rewardModel =
        checkTask.isRewardModelSet() ? this->parametricModel->getRewardModel(checkTask.getRewardModel()) : this->parametricModel->getUniqueRewardModel();
    std::vector<ParametricType> b = rewardModel.getTotalRewardVector(this->parametricModel->getTransitionMatrix());

    parameterLifter = std::make_unique<storm::transformer::ParameterLifter<ParametricType, ConstantType>>(
        this->parametricModel->getTransitionMatrix(), b, storm::storage::BitVector(this->parametricModel->getTransitionMatrix().getRowCount(), true),
        maybeStates);
    computePlayer1Matrix();

    applyPreviousResultAsHint = false;

    // We only know a lower bound for the result
    lowerResultBound = storm::utility::zero<ConstantType>();
}

template<typename SparseModelType, typename ConstantType>
storm::modelchecker::SparseInstantiationModelChecker<SparseModelType, ConstantType>&
SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::getInstantiationChecker(bool quantitative) {
    if (!instantiationChecker) {
        instantiationChecker = std::make_unique<storm::modelchecker::SparseMdpInstantiationModelChecker<SparseModelType, ConstantType>>(*this->parametricModel);
        instantiationChecker->specifyFormula(quantitative ? *this->currentCheckTaskNoBound
                                                          : this->currentCheckTask->template convertValueType<ParametricType>());
        instantiationChecker->setInstantiationsAreGraphPreserving(true);
    }
    return *instantiationChecker;
}

template<typename SparseModelType, typename ConstantType>
bool SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::isMonotonicitySupported(
    MonotonicityBackend<ParametricType> const& backend, CheckTask<storm::logic::Formula, ParametricType> const&) const {
    // Currently, we do not support any interaction with the monotonicity backend
    return !backend.requiresInteractionWithRegionModelChecker();
}

template<typename SparseModelType, typename ConstantType>
std::vector<ConstantType> SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::computeQuantitativeValues(
    Environment const& env, AnnotatedRegion<ParametricType>& region, storm::solver::OptimizationDirection const& dirForParameters) {
    if (maybeStates.empty()) {
        this->updateKnownValueBoundInRegion(region, dirForParameters, resultsForNonMaybeStates);
        return resultsForNonMaybeStates;
    }

    parameterLifter->specifyRegion(region.region, dirForParameters);

    // Set up the solver
    auto solver = solverFactory->create(env, player1Matrix, parameterLifter->getMatrix());
    if (lowerResultBound)
        solver->setLowerBound(lowerResultBound.value());
    if (upperResultBound)
        solver->setUpperBound(upperResultBound.value());
    if (applyPreviousResultAsHint) {
        solver->setTrackSchedulers(true);
        x.resize(maybeStates.getNumberOfSetBits(), storm::utility::zero<ConstantType>());
        if (storm::solver::minimize(dirForParameters) && minSchedChoices && player1SchedChoices)
            solver->setSchedulerHints(std::move(player1SchedChoices.value()), std::move(minSchedChoices.value()));
        if (storm::solver::maximize(dirForParameters) && maxSchedChoices && player1SchedChoices)
            solver->setSchedulerHints(std::move(player1SchedChoices.value()), std::move(maxSchedChoices.value()));
    } else {
        x.assign(maybeStates.getNumberOfSetBits(), storm::utility::zero<ConstantType>());
    }
    if (this->currentCheckTask->isBoundSet() && this->currentCheckTask->getOptimizationDirection() == dirForParameters && solver->hasSchedulerHints()) {
        // If we reach this point, we know that after applying the hints, the x-values can only become larger (if we maximize) or smaller (if we minimize).
        std::unique_ptr<storm::solver::TerminationCondition<ConstantType>> termCond;
        storm::storage::BitVector relevantStatesInSubsystem = this->currentCheckTask->isOnlyInitialStatesRelevantSet()
                                                                  ? this->parametricModel->getInitialStates() % maybeStates
                                                                  : storm::storage::BitVector(maybeStates.getNumberOfSetBits(), true);
        if (storm::solver::minimize(dirForParameters)) {
            // Terminate if the value for ALL relevant states is already below the threshold
            termCond = std::make_unique<storm::solver::TerminateIfFilteredExtremumBelowThreshold<ConstantType>>(
                relevantStatesInSubsystem, true, this->currentCheckTask->getBoundThreshold(), false);
        } else {
            // Terminate if the value for ALL relevant states is already above the threshold
            termCond = std::make_unique<storm::solver::TerminateIfFilteredExtremumExceedsThreshold<ConstantType>>(
                relevantStatesInSubsystem, true, this->currentCheckTask->getBoundThreshold(), true);
        }
        solver->setTerminationCondition(std::move(termCond));
    }

    // Invoke the solver
    if (stepBound) {
        STORM_LOG_ASSERT(*stepBound > 0, "Expected positive step bound.");
        solver->repeatedMultiply(env, this->currentCheckTask->getOptimizationDirection(), dirForParameters, x, &parameterLifter->getVector(), *stepBound);
    } else {
        solver->solveGame(env, this->currentCheckTask->getOptimizationDirection(), dirForParameters, x, parameterLifter->getVector());
        if (applyPreviousResultAsHint) {
            if (storm::solver::minimize(dirForParameters)) {
                minSchedChoices = solver->getPlayer2SchedulerChoices();
            } else {
                maxSchedChoices = solver->getPlayer2SchedulerChoices();
            }
            player1SchedChoices = solver->getPlayer1SchedulerChoices();
        }
    }

    // Get the result for the complete model (including maybestates)
    std::vector<ConstantType> result = resultsForNonMaybeStates;
    auto maybeStateResIt = x.begin();
    for (uint64_t maybeState : maybeStates) {
        result[maybeState] = *maybeStateResIt;
        ++maybeStateResIt;
    }
    this->updateKnownValueBoundInRegion(region, dirForParameters, result);
    return result;
}

template<typename SparseModelType, typename ConstantType>
void SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::computePlayer1Matrix(std::optional<storm::storage::BitVector> const& selectedRows) {
    uint64_t n = 0;
    if (selectedRows) {
        // only count selected rows
        n = selectedRows->getNumberOfSetBits();
    } else {
        for (uint64_t maybeState : maybeStates) {
            n += this->parametricModel->getTransitionMatrix().getRowGroupSize(maybeState);
        }
    }

    // The player 1 matrix is the identity matrix of size n with the row groups as given by the original matrix (potentially without unselected rows)
    storm::storage::SparseMatrixBuilder<storm::storage::sparse::state_type> matrixBuilder(n, n, n, true, true, maybeStates.getNumberOfSetBits());
    uint64_t p1MatrixRow = 0;
    for (uint64_t maybeState : maybeStates) {
        matrixBuilder.newRowGroup(p1MatrixRow);
        if (selectedRows) {
            for (uint64_t row = selectedRows->getNextSetIndex(this->parametricModel->getTransitionMatrix().getRowGroupIndices()[maybeState]);
                 row < this->parametricModel->getTransitionMatrix().getRowGroupIndices()[maybeState + 1]; row = selectedRows->getNextSetIndex(row + 1)) {
                matrixBuilder.addNextValue(p1MatrixRow, p1MatrixRow, storm::utility::one<storm::storage::sparse::state_type>());
                ++p1MatrixRow;
            }
        } else {
            for (uint64_t endOfGroup = p1MatrixRow + this->parametricModel->getTransitionMatrix().getRowGroupSize(maybeState); p1MatrixRow < endOfGroup;
                 ++p1MatrixRow) {
                matrixBuilder.addNextValue(p1MatrixRow, p1MatrixRow, storm::utility::one<storm::storage::sparse::state_type>());
            }
        }
    }
    player1Matrix = matrixBuilder.build();
}

template<typename SparseModelType, typename ConstantType>
void SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::reset() {
    maybeStates.resize(0);
    resultsForNonMaybeStates.clear();
    stepBound = std::nullopt;
    instantiationChecker = nullptr;
    player1Matrix = storm::storage::SparseMatrix<storm::storage::sparse::state_type>();
    parameterLifter = nullptr;
    minSchedChoices = std::nullopt;
    maxSchedChoices = std::nullopt;
    x.clear();
    lowerResultBound = std::nullopt;
    upperResultBound = std::nullopt;
    applyPreviousResultAsHint = false;
}

template<typename ConstantType>
std::optional<storm::storage::Scheduler<ConstantType>> getSchedulerHelper(std::optional<std::vector<uint64_t>> const& choices) {
    std::optional<storm::storage::Scheduler<ConstantType>> result;
    if (choices) {
        result.emplace(choices->size());
        uint64_t state = 0;
        for (auto const& choice : choices.value()) {
            result->setChoice(choice, state);
            ++state;
        }
    }
    return result;
}

template<typename SparseModelType, typename ConstantType>
std::optional<storm::storage::Scheduler<ConstantType>> SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::getCurrentMinScheduler() {
    return getSchedulerHelper<ConstantType>(minSchedChoices);
}

template<typename SparseModelType, typename ConstantType>
std::optional<storm::storage::Scheduler<ConstantType>> SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::getCurrentMaxScheduler() {
    return getSchedulerHelper<ConstantType>(maxSchedChoices);
}

template<typename SparseModelType, typename ConstantType>
std::optional<storm::storage::Scheduler<ConstantType>> SparseMdpParameterLiftingModelChecker<SparseModelType, ConstantType>::getCurrentPlayer1Scheduler() {
    return getSchedulerHelper<ConstantType>(player1SchedChoices);
}

template class SparseMdpParameterLiftingModelChecker<storm::models::sparse::Mdp<storm::RationalFunction>, double>;
template class SparseMdpParameterLiftingModelChecker<storm::models::sparse::Mdp<storm::RationalFunction>, storm::RationalNumber>;
}  // namespace storm::modelchecker
