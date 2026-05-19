#include "storm-pars/modelchecker/region/SparseDtmcParameterLiftingModelChecker.h"

#include <memory>
#include <vector>

#include "storm-pars/derivative/SparseDerivativeInstantiationModelChecker.h"
#include "storm-pars/modelchecker/region/AnnotatedRegion.h"
#include "storm-pars/modelchecker/region/RegionSplitEstimateKind.h"
#include "storm-pars/modelchecker/region/monotonicity/OrderBasedMonotonicityBackend.h"
#include "storm-pars/transformer/BigStep.h"
#include "storm-pars/transformer/IntervalEndComponentPreserver.h"
#include "storm-pars/transformer/SparseParametricDtmcSimplifier.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalFunctionForward.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UncheckedRequirementException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/helper/indefinitehorizon/visitingtimes/SparseDeterministicVisitingTimesHelper.h"
#include "storm/modelchecker/prctl/helper/BaierUpperRewardBoundsComputer.h"
#include "storm/modelchecker/prctl/helper/DsMpiUpperRewardBoundsComputer.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/solver/UncertaintyResolutionMode.h"
#include "storm/solver/multiplier/Multiplier.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/logging.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {

template<typename SparseModelType, typename ConstantType, bool Robust>
SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::SparseDtmcParameterLiftingModelChecker()
    : SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>(std::make_unique<GeneralSolverFactoryType<ConstantType, Robust>>()) {
    // Intentionally left empty
}

template<typename SparseModelType, typename ConstantType, bool Robust>
SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::SparseDtmcParameterLiftingModelChecker(
    std::unique_ptr<SolverFactoryType<ConstantType, Robust>>&& solverFactory)
    : solverFactory(std::move(solverFactory)), solvingRequiresUpperRewardBounds(false) {
    // Intentionally left empty
}

template<typename SparseModelType, typename ConstantType, bool Robust>
bool SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::canHandle(
    std::shared_ptr<storm::models::ModelBase> parametricModel, CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const {
    bool result = parametricModel->isOfType(storm::models::ModelType::Dtmc);
    result &= parametricModel->isSparseModel();
    result &= parametricModel->supportsParameters();
    auto dtmc = parametricModel->template as<SparseModelType>();
    result &= static_cast<bool>(dtmc);
    result &= checkTask.getFormula().isInFragment(storm::logic::reachability()
                                                      .setRewardOperatorsAllowed(true)
                                                      .setReachabilityRewardFormulasAllowed(true)
                                                      .setBoundedUntilFormulasAllowed(true)
                                                      .setCumulativeRewardFormulasAllowed(true)
                                                      .setStepBoundedCumulativeRewardFormulasAllowed(true)
                                                      .setTimeBoundedCumulativeRewardFormulasAllowed(true)
                                                      .setTimeBoundedUntilFormulasAllowed(true)
                                                      .setStepBoundedUntilFormulasAllowed(true)
                                                      .setTimeBoundedUntilFormulasAllowed(true));
    return result;
}

template<typename SparseModelType, typename ConstantType, bool Robust>
void SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::specify(
    Environment const& env, std::shared_ptr<storm::models::ModelBase> parametricModel, CheckTask<storm::logic::Formula, ParametricType> const& checkTask,
    std::optional<RegionSplitEstimateKind> generateRegionSplitEstimates, std::shared_ptr<MonotonicityBackend<ParametricType>> monotonicityBackend,
    bool allowModelSimplifications, bool graphPreserving) {
    STORM_LOG_THROW(this->canHandle(parametricModel, checkTask), storm::exceptions::NotSupportedException,
                    "Combination of model " << parametricModel->getType() << " and formula '" << checkTask.getFormula() << "' is not supported.");
    this->specifySplitEstimates(generateRegionSplitEstimates, checkTask);
    this->specifyMonotonicity(monotonicityBackend, checkTask);
    this->graphPreserving = graphPreserving;
    auto dtmc = parametricModel->template as<SparseModelType>();
    if (isOrderBasedMonotonicityBackend()) {
        STORM_LOG_WARN_COND(!(allowModelSimplifications),
                            "Allowing model simplification when using order-based monotonicity is not useful, as for order-based monotonicity checking model "
                            "simplification is done as preprocessing");  // TODO: Find out where this preprocessing for monotonicity is done
        getOrderBasedMonotonicityBackend().initializeMonotonicityChecker(dtmc->getTransitionMatrix());
    }

    reset();

    if (allowModelSimplifications && graphPreserving) {
        auto simplifier = storm::transformer::SparseParametricDtmcSimplifier<SparseModelType>(*dtmc);
        simplifier.setPreserveParametricTransitions(true);
        if (!simplifier.simplify(checkTask.getFormula())) {
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Simplifying the model was not successfull.");
        }
        this->parametricModel = simplifier.getSimplifiedModel();
        this->specifyFormula(env, checkTask.substituteFormula(*simplifier.getSimplifiedFormula()));
    } else {
        this->parametricModel = dtmc;
        this->specifyFormula(env, checkTask);
    }
    if constexpr (!Robust) {
        if (isOrderBasedMonotonicityBackend()) {
            getOrderBasedMonotonicityBackend().registerParameterLifterReference(*parameterLifter);
            getOrderBasedMonotonicityBackend().registerPLABoundFunction(
                [this](storm::Environment const& environment, AnnotatedRegion<ParametricType>& region, storm::OptimizationDirection dir) {
                    return this->computeQuantitativeValues(environment, region, dir);  // sets known value bounds within the region
                });
        }
    }
    std::shared_ptr<storm::logic::Formula> formulaWithoutBounds = this->currentCheckTask->getFormula().clone();
    formulaWithoutBounds->asOperatorFormula().removeBound();
    this->currentFormulaNoBound = formulaWithoutBounds->asSharedPointer();
    this->currentCheckTaskNoBound = std::make_unique<storm::modelchecker::CheckTask<storm::logic::Formula, ParametricType>>(*this->currentFormulaNoBound);
    if (this->specifiedRegionSplitEstimateKind == RegionSplitEstimateKind::Derivative) {
        this->derivativeChecker =
            std::make_unique<storm::derivative::SparseDerivativeInstantiationModelChecker<ParametricType, ConstantType>>(*this->parametricModel);
        this->derivativeChecker->specifyFormula(env, *this->currentCheckTaskNoBound);
    }
}

template<typename SparseModelType, typename ConstantType, bool Robust>
void SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::specifyBoundedUntilFormula(
    const CheckTask<storm::logic::BoundedUntilFormula, ConstantType>& checkTask) {
    STORM_LOG_ERROR_COND(!Robust, "Bounded until formulas not implemented for Robust PLA");
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
                    storm::exceptions::NotSupportedException, "Parameter lifting with non-propositional subformulas is not supported");
    storm::storage::BitVector phiStates = std::move(propositionalChecker.check(checkTask.getFormula().getLeftSubformula())
                                                        ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                        .getTruthValuesVector());
    storm::storage::BitVector psiStates = std::move(propositionalChecker.check(checkTask.getFormula().getRightSubformula())
                                                        ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                        .getTruthValuesVector());

    // get the maybeStates
    maybeStates = storm::utility::graph::performProbGreater0(this->parametricModel->getBackwardTransitions(), phiStates, psiStates, true, *stepBound);
    maybeStates &= ~psiStates;

    // set the result for all non-maybe states
    resultsForNonMaybeStates = std::vector<ConstantType>(this->parametricModel->getNumberOfStates(), storm::utility::zero<ConstantType>());
    storm::utility::vector::setVectorValues(resultsForNonMaybeStates, psiStates, storm::utility::one<ConstantType>());

    // if there are maybestates, create the parameterLifter
    if (Robust || !maybeStates.empty()) {
        // Create the vector of one-step probabilities to go to target states.
        std::vector<ParametricType> b = this->parametricModel->getTransitionMatrix().getConstrainedRowSumVector(
            storm::storage::BitVector(this->parametricModel->getTransitionMatrix().getRowCount(), true), psiStates);
        parameterLifter = std::make_unique<ParameterLifterType<ParametricType, ConstantType, Robust>>(
            this->parametricModel->getTransitionMatrix(), b, maybeStates, maybeStates, false, isOrderBasedMonotonicityBackend());
    }

    // We know some bounds for the results so set them
    lowerResultBound = storm::utility::zero<ConstantType>();
    upperResultBound = storm::utility::one<ConstantType>();
    // No requirements for bounded formulas
    solverFactory->setRequirementsChecked(true);

    if (isOrderBasedMonotonicityBackend()) {
        auto [prob0, prob1] = storm::utility::graph::performProb01(this->parametricModel->getBackwardTransitions(), phiStates, psiStates);
        getOrderBasedMonotonicityBackend().initializeOrderExtender(prob1, prob0, this->parametricModel->getTransitionMatrix());
    }
}

template<typename SparseModelType, typename ConstantType, bool Robust>
void SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::specifyUntilFormula(
    Environment const& env, CheckTask<storm::logic::UntilFormula, ConstantType> const& checkTask) {
    // get the results for the subformulas
    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> propositionalChecker(*this->parametricModel);
    STORM_LOG_THROW(propositionalChecker.canHandle(checkTask.getFormula().getLeftSubformula()) &&
                        propositionalChecker.canHandle(checkTask.getFormula().getRightSubformula()),
                    storm::exceptions::NotSupportedException, "Parameter lifting with non-propositional subformulas is not supported");
    storm::storage::BitVector phiStates = std::move(propositionalChecker.check(checkTask.getFormula().getLeftSubformula())
                                                        ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                        .getTruthValuesVector());
    storm::storage::BitVector psiStates = std::move(propositionalChecker.check(checkTask.getFormula().getRightSubformula())
                                                        ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                        .getTruthValuesVector());

    // get the maybeStates
    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 =
        storm::utility::graph::performProb01(this->parametricModel->getBackwardTransitions(), phiStates, psiStates);
    maybeStates = ~(statesWithProbability01.first | statesWithProbability01.second);

    // set the result for all non-maybe states
    resultsForNonMaybeStates = std::vector<ConstantType>(this->parametricModel->getNumberOfStates(), storm::utility::zero<ConstantType>());
    storm::utility::vector::setVectorValues(resultsForNonMaybeStates, statesWithProbability01.second, storm::utility::one<ConstantType>());

    // if there are maybestates, create the parameterLifter
    if (Robust || !maybeStates.empty()) {
        if constexpr (Robust) {
            // Create the vector of one-step probabilities to go to target states.
            // Robust PLA doesn't support eliminating states because it gets complicated with the polynomials you know
            std::vector<ParametricType> target(this->parametricModel->getNumberOfStates(), storm::utility::zero<ParametricType>());
            storm::storage::BitVector allTrue(maybeStates.size(), true);

            if (!graphPreserving) {
                storm::utility::vector::setVectorValues(target, psiStates, storm::utility::one<ParametricType>());
                maybeStates = ~statesWithProbability01.first & ~psiStates;
            } else {
                storm::utility::vector::setVectorValues(target, statesWithProbability01.second, storm::utility::one<ParametricType>());
            }

            // With Robust PLA, we cannot drop the non-maybe states out of the matrix for technical reasons
            auto rowFilter = this->parametricModel->getTransitionMatrix().getRowFilter(maybeStates);
            auto filteredMatrix = this->parametricModel->getTransitionMatrix().filterEntries(rowFilter);

            maybeStates = allTrue;

            parameterLifter = std::make_unique<ParameterLifterType<ParametricType, ConstantType, Robust>>(
                filteredMatrix, target, allTrue, allTrue, isValueDeltaRegionSplitEstimates(), isOrderBasedMonotonicityBackend());
        } else {
            // Create the vector of one-step probabilities to go to target states.
            std::vector<ParametricType> b = this->parametricModel->getTransitionMatrix().getConstrainedRowSumVector(
                storm::storage::BitVector(this->parametricModel->getTransitionMatrix().getRowCount(), true), statesWithProbability01.second);
            parameterLifter = std::make_unique<ParameterLifterType<ParametricType, ConstantType, Robust>>(
                this->parametricModel->getTransitionMatrix(), b, maybeStates, maybeStates, isValueDeltaRegionSplitEstimates(),
                isOrderBasedMonotonicityBackend());
        }
    }

    // We know some bounds for the results so set them
    lowerResultBound = storm::utility::zero<ConstantType>();
    upperResultBound = storm::utility::one<ConstantType>();

    // The solution of the min-max equation system will always be unique (assuming graph-preserving instantiations, every induced DTMC has the same graph
    // structure).
    auto req = solverFactory->getRequirements(env, true, true, boost::none, !Robust);
    req.clearBounds();
    STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                    "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
    solverFactory->setRequirementsChecked(true);

    if (isOrderBasedMonotonicityBackend()) {
        getOrderBasedMonotonicityBackend().initializeOrderExtender(statesWithProbability01.second, statesWithProbability01.first,
                                                                   this->parametricModel->getTransitionMatrix());
    }
}

template<typename SparseModelType, typename ConstantType, bool Robust>
void SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::specifyReachabilityRewardFormula(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ConstantType> const& checkTask) {
    // get the results for the subformula
    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> propositionalChecker(*this->parametricModel);
    STORM_LOG_THROW(propositionalChecker.canHandle(checkTask.getFormula().getSubformula()), storm::exceptions::NotSupportedException,
                    "Parameter lifting with non-propositional subformulas is not supported");
    storm::storage::BitVector targetStates = std::move(propositionalChecker.check(checkTask.getFormula().getSubformula())
                                                           ->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>()
                                                           .getTruthValuesVector());
    // get the maybeStates
    storm::storage::BitVector infinityStates = storm::utility::graph::performProb1(
        this->parametricModel->getBackwardTransitions(), storm::storage::BitVector(this->parametricModel->getNumberOfStates(), true), targetStates);
    infinityStates.complement();
    maybeStates = ~(targetStates | infinityStates);

    // set the result for all the non-maybe states
    resultsForNonMaybeStates = std::vector<ConstantType>(this->parametricModel->getNumberOfStates(), storm::utility::zero<ConstantType>());
    storm::utility::vector::setVectorValues(resultsForNonMaybeStates, infinityStates, storm::utility::infinity<ConstantType>());

    // if there are maybestates, create the parameterLifter
    if (Robust || !maybeStates.empty()) {
        // Create the reward vector
        STORM_LOG_THROW((checkTask.isRewardModelSet() && this->parametricModel->hasRewardModel(checkTask.getRewardModel())) ||
                            (!checkTask.isRewardModelSet() && this->parametricModel->hasUniqueRewardModel()),
                        storm::exceptions::InvalidPropertyException, "The reward model specified by the CheckTask is not available in the given model.");

        typename SparseModelType::RewardModelType const& rewardModel =
            checkTask.isRewardModelSet() ? this->parametricModel->getRewardModel(checkTask.getRewardModel()) : this->parametricModel->getUniqueRewardModel();

        std::vector<ParametricType> b = rewardModel.getTotalRewardVector(this->parametricModel->getTransitionMatrix());

        if constexpr (Robust) {
            storm::storage::BitVector allTrue(maybeStates.size(), true);
            if (!graphPreserving) {
                maybeStates = ~targetStates;
            }
            auto rowFilter = this->parametricModel->getTransitionMatrix().getRowFilter(maybeStates);
            auto filteredMatrix = this->parametricModel->getTransitionMatrix().filterEntries(rowFilter);
            maybeStates = allTrue;

            parameterLifter = std::make_unique<ParameterLifterType<ParametricType, ConstantType, Robust>>(
                filteredMatrix, b, allTrue, allTrue, isValueDeltaRegionSplitEstimates(), isOrderBasedMonotonicityBackend());
        } else {
            parameterLifter = std::make_unique<ParameterLifterType<ParametricType, ConstantType, Robust>>(
                this->parametricModel->getTransitionMatrix(), b, maybeStates, maybeStates, isValueDeltaRegionSplitEstimates(),
                isOrderBasedMonotonicityBackend());
        }
    }

    // We only know a lower bound for the result
    lowerResultBound = storm::utility::zero<ConstantType>();

    // The solution of the min-max equation system will always be unique (assuming graph-preserving instantiations, every induced DTMC has the same graph
    // structure).
    auto req = solverFactory->getRequirements(env, true, true, boost::none, !Robust);
    req.clearLowerBounds();
    if (req.upperBounds()) {
        solvingRequiresUpperRewardBounds = true;
        req.clearUpperBounds();
    }
    STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                    "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
    solverFactory->setRequirementsChecked(true);
    STORM_LOG_WARN_COND(!isOrderBasedMonotonicityBackend(), "Order-based monotonicity not used for reachability reward formula.");
}

template<typename SparseModelType, typename ConstantType, bool Robust>
void SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::specifyCumulativeRewardFormula(
    const CheckTask<storm::logic::CumulativeRewardFormula, ConstantType>& checkTask) {
    STORM_LOG_ERROR_COND(!Robust, "Not implemented for robust mode.");
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

    parameterLifter =
        std::make_unique<ParameterLifterType<ParametricType, ConstantType, Robust>>(this->parametricModel->getTransitionMatrix(), b, maybeStates, maybeStates);
    // We only know a lower bound for the result
    lowerResultBound = storm::utility::zero<ConstantType>();

    // No requirements for bounded reward formula
    solverFactory->setRequirementsChecked(true);

    STORM_LOG_WARN_COND(!isOrderBasedMonotonicityBackend(), "Order-based monotonicity not used for cumulative reward formula.");
}

template<typename SparseModelType, typename ConstantType, bool Robust>
storm::modelchecker::SparseInstantiationModelChecker<SparseModelType, ConstantType>&
SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::getInstantiationCheckerSAT(bool quantitative) {
    if (!instantiationCheckerSAT) {
        instantiationCheckerSAT =
            std::make_unique<storm::modelchecker::SparseDtmcInstantiationModelChecker<SparseModelType, ConstantType>>(*this->parametricModel);
        instantiationCheckerSAT->specifyFormula(quantitative ? *this->currentCheckTaskNoBound
                                                             : this->currentCheckTask->template convertValueType<ParametricType>());
        instantiationCheckerSAT->setInstantiationsAreGraphPreserving(true);
    }
    return *instantiationCheckerSAT;
}

template<typename SparseModelType, typename ConstantType, bool Robust>
storm::modelchecker::SparseInstantiationModelChecker<SparseModelType, ConstantType>&
SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::getInstantiationCheckerVIO(bool quantitative) {
    if (!instantiationCheckerVIO) {
        instantiationCheckerVIO =
            std::make_unique<storm::modelchecker::SparseDtmcInstantiationModelChecker<SparseModelType, ConstantType>>(*this->parametricModel);
        instantiationCheckerVIO->specifyFormula(quantitative ? *this->currentCheckTaskNoBound
                                                             : this->currentCheckTask->template convertValueType<ParametricType>());
        instantiationCheckerVIO->setInstantiationsAreGraphPreserving(true);
    }
    return *instantiationCheckerVIO;
}

template<typename SparseModelType, typename ConstantType, bool Robust>
storm::modelchecker::SparseInstantiationModelChecker<SparseModelType, ConstantType>&
SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::getInstantiationChecker(bool quantitative) {
    if (!instantiationChecker) {
        instantiationChecker =
            std::make_unique<storm::modelchecker::SparseDtmcInstantiationModelChecker<SparseModelType, ConstantType>>(*this->parametricModel);
        instantiationChecker->specifyFormula(quantitative ? *this->currentCheckTaskNoBound
                                                          : this->currentCheckTask->template convertValueType<ParametricType>());
        instantiationChecker->setInstantiationsAreGraphPreserving(true);
    }
    return *instantiationChecker;
}

template<typename SparseModelType, typename ConstantType, bool Robust>
std::vector<ConstantType> SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::computeQuantitativeValues(
    Environment const& env, AnnotatedRegion<ParametricType>& region, storm::solver::OptimizationDirection const& dirForParameters) {
    if (maybeStates.empty()) {
        this->updateKnownValueBoundInRegion(region, dirForParameters, resultsForNonMaybeStates);
        return resultsForNonMaybeStates;
    }
    parameterLifter->specifyRegion(region.region, dirForParameters);
    auto liftedMatrix = parameterLifter->getMatrix();
    auto liftedVector = parameterLifter->getVector();
    bool nonTrivialEndComponents = false;
    if constexpr (Robust) {
        if (parameterLifter->isCurrentRegionAllIllDefined()) {
            return std::vector<ConstantType>();
        }
        if (!graphPreserving) {
            transformer::IntervalEndComponentPreserver endComponentPreserver;
            auto const& result = endComponentPreserver.eliminateMECs(liftedMatrix, liftedVector);
            if (result) {
                // std::cout << liftedMatrix << std::endl;
                // std::cout << *result << std::endl;
                liftedMatrix = *result;
                nonTrivialEndComponents = true;
            }
        }
    }
    const uint64_t resultVectorSize = liftedMatrix.getColumnCount();

    if (stepBound) {
        if constexpr (!Robust) {
            assert(*stepBound > 0);
            x = std::vector<ConstantType>(resultVectorSize, storm::utility::zero<ConstantType>());
            auto multiplier = storm::solver::MultiplierFactory<ConstantType>().create(env, liftedMatrix);
            multiplier->repeatedMultiplyAndReduce(env, dirForParameters, x, &liftedVector, *stepBound);
        } else {
            STORM_LOG_ERROR("Cannot check step-bounded formulas in robust mode.");
        }
    } else {
        auto solver = solverFactory->create(env, liftedMatrix);
        solver->setHasUniqueSolution();
        solver->setHasNoEndComponents();
        // Uncertainty is not robust (=adversarial)
        solver->setUncertaintyResolutionMode(UncertaintyResolutionMode::Cooperative);
        if (lowerResultBound)
            solver->setLowerBound(lowerResultBound.value());
        if (upperResultBound) {
            solver->setUpperBound(upperResultBound.value());
        } else if (solvingRequiresUpperRewardBounds) {
            if constexpr (!Robust) {
                // For the min-case, we use DS-MPI, for the max-case variant 2 of the Baier et al. paper (CAV'17).
                std::vector<ConstantType> oneStepProbs;
                oneStepProbs.reserve(liftedMatrix.getRowCount());
                for (uint64_t row = 0; row < liftedMatrix.getRowCount(); ++row) {
                    oneStepProbs.push_back(storm::utility::one<ConstantType>() - liftedMatrix.getRowSum(row));
                }
                if (dirForParameters == storm::OptimizationDirection::Minimize) {
                    storm::modelchecker::helper::DsMpiMdpUpperRewardBoundsComputer<ConstantType> dsmpi(liftedMatrix, liftedVector, oneStepProbs);
                    solver->setUpperBounds(dsmpi.computeUpperBounds());
                } else {
                    storm::modelchecker::helper::BaierUpperRewardBoundsComputer<ConstantType> baier(liftedMatrix, oneStepProbs);
                    solver->setUpperBound(baier.computeTotalRewardBounds(liftedVector).upper);
                }
            } else {
                STORM_LOG_ERROR("Cannot use upper reward bounds in robust mode.");
            }
        }
        solver->setTrackScheduler(true);

        // Get reference to relevant scheduler choices
        auto& choices = storm::solver::minimize(dirForParameters) ? minSchedChoices : maxSchedChoices;

        // Potentially fix some choices if order based monotonicity is known
        if constexpr (!Robust) {
            storm::storage::BitVector statesWithFixedChoice;
            if (isOrderBasedMonotonicityBackend()) {
                // Ensure choices are initialized
                if (!choices.has_value()) {
                    choices.emplace(parameterLifter->getRowGroupCount(), 0u);
                }
                statesWithFixedChoice = getOrderBasedMonotonicityBackend().getChoicesToFixForPLASolver(region, dirForParameters, *choices);
            }

            // Set initial scheduler
            if (choices.has_value()) {
                solver->setInitialScheduler(std::move(choices.value()));
                if (statesWithFixedChoice.size() != 0) {
                    // Choices need to be fixed after setting a scheduler
                    solver->setSchedulerFixedForRowGroup(std::move(statesWithFixedChoice));
                }
            }
        } else {
            // Set initial scheduler
            if (!nonTrivialEndComponents && choices.has_value()) {
                solver->setInitialScheduler(std::move(choices.value()));
            }
        }

        if (this->currentCheckTask->isBoundSet() && solver->hasInitialScheduler()) {
            // If we reach this point, we know that after applying the hint, the x-values can only become larger (if we maximize) or smaller (if we
            // minimize).
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
        x.resize(resultVectorSize, storm::utility::zero<ConstantType>());
        solver->solveEquations(env, dirForParameters, x, liftedVector);
        if (isValueDeltaRegionSplitEstimates()) {
            computeStateValueDeltaRegionSplitEstimates(env, x, solver->getSchedulerChoices(), region.region, dirForParameters);
        }
        // Store choices for next time, if we have no non-trivial end components
        if (!nonTrivialEndComponents) {
            choices = solver->getSchedulerChoices();
        }
    }

    // Get the result for the complete model (including maybestates)
    std::vector<ConstantType> result = resultsForNonMaybeStates;
    auto maybeStateResIt = x.begin();
    for (auto const& maybeState : maybeStates) {
        result[maybeState] = *maybeStateResIt;
        ++maybeStateResIt;
    }

    STORM_LOG_INFO(dirForParameters << " " << region.region << ": " << result[this->getUniqueInitialState()] << std::endl);

    this->updateKnownValueBoundInRegion(region, dirForParameters, result);
    return result;
}

template<typename SparseModelType, typename ConstantType, bool Robust>
void SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::computeStateValueDeltaRegionSplitEstimates(
    Environment const& env, std::vector<ConstantType> const& quantitativeResult, std::vector<uint64_t> const& schedulerChoices,
    storm::storage::ParameterRegion<ParametricType> const& region, storm::solver::OptimizationDirection const& dirForParameters) {
    auto const& matrix = parameterLifter->getMatrix();
    auto const& vector = parameterLifter->getVector();

    std::vector<ConstantType> weighting = std::vector<ConstantType>(vector.size(), utility::one<ConstantType>());
    if (this->specifiedRegionSplitEstimateKind == RegionSplitEstimateKind::StateValueDeltaWeighted) {
        // Instantiated on center, instantiate on choices instead?
        // Kinda complicated tho
        storm::utility::ModelInstantiator<SparseModelType, storm::models::sparse::Dtmc<ConstantType>> instantiator(*this->parametricModel);
        auto const instantiatedModel = instantiator.instantiate(region.getCenterPoint());
        helper::SparseDeterministicVisitingTimesHelper<ConstantType> visitingTimesHelper(instantiatedModel.getTransitionMatrix());
        auto const visitingTimes = visitingTimesHelper.computeExpectedVisitingTimes(env, this->parametricModel->getInitialStates());
        uint64_t rowIndex = 0;
        for (auto const& state : maybeStates) {
            weighting[rowIndex++] = visitingTimes[state];
        }
    }

    switch (*this->specifiedRegionSplitEstimateKind) {
        case RegionSplitEstimateKind::StateValueDelta:
        case RegionSplitEstimateKind::StateValueDeltaWeighted: {
            std::map<VariableType, ConstantType> deltaLower, deltaUpper;
            for (auto const& p : region.getVariables()) {
                deltaLower.emplace(p, storm::utility::zero<ConstantType>());
                deltaUpper.emplace(p, storm::utility::zero<ConstantType>());
            }
            if constexpr (Robust) {
                // Cache all derivatives of functions that turn up in pMC
                static std::map<RationalFunction, RationalFunction> functionDerivatives;
                static std::vector<std::pair<bool, double>> constantDerivatives;
                if (constantDerivatives.empty()) {
                    for (uint64_t state : maybeStates) {
                        auto variables = parameterLifter->getOccurringVariablesAtState().at(state);
                        if (variables.size() == 0) {
                            continue;
                        }
                        STORM_LOG_ERROR_COND(variables.size() == 1,
                                             "Cannot compute state-value-delta split estimates in robust mode if there are states with multiple parameters.");
                        auto const p = *variables.begin();
                        for (auto const& entry : this->parametricModel->getTransitionMatrix().getRow(state)) {
                            auto const& function = entry.getValue();
                            if (functionDerivatives.count(function)) {
                                constantDerivatives.emplace_back(false, 0);
                                continue;
                            }
                            auto const derivative = function.derivative(p);
                            if (derivative.isConstant()) {
                                constantDerivatives.emplace_back(true, utility::convertNumber<double>(derivative.constantPart()));
                            } else if (!storm::transformer::BigStep::lastSavedAnnotations.count(entry.getValue())) {
                                functionDerivatives.emplace(function, derivative);
                                constantDerivatives.emplace_back(false, 0);
                            } else {
                                constantDerivatives.emplace_back(false, 0);
                            }
                        }
                    }
                }

                cachedRegionSplitEstimates.clear();
                for (auto const& p : region.getVariables()) {
                    cachedRegionSplitEstimates.emplace(p, utility::zero<ConstantType>());
                }

                uint64_t entryCount = 0;
                // Assumption: Only one parameter per state
                for (uint64_t state : maybeStates) {
                    auto variables = parameterLifter->getOccurringVariablesAtState().at(state);
                    if (variables.size() == 0) {
                        continue;
                    }
                    STORM_LOG_ERROR_COND(variables.size() == 1,
                                         "Cannot compute state-value-delta split estimates in robust mode if there are states with multiple parameters.");

                    auto const p = *variables.begin();

                    const uint64_t rowIndex = maybeStates.getNumberOfSetBitsBeforeIndex(state);

                    std::vector<ConstantType> derivatives;
                    for (auto const& entry : this->parametricModel->getTransitionMatrix().getRow(state)) {
                        if (storm::transformer::BigStep::lastSavedAnnotations.count(entry.getValue())) {
                            auto& annotation = storm::transformer::BigStep::lastSavedAnnotations.at(entry.getValue());
                            ConstantType derivative =
                                annotation.derivative()->template evaluate<ConstantType>(utility::convertNumber<ConstantType>(region.getCenter(p)));
                            derivatives.push_back(derivative);
                        } else {
                            auto const& cDer = constantDerivatives.at(entryCount);
                            if (cDer.first) {
                                derivatives.push_back(cDer.second);
                            } else {
                                CoefficientType derivative = functionDerivatives.at(entry.getValue()).evaluate(region.getCenterPoint());
                                derivatives.push_back(utility::convertNumber<ConstantType>(derivative));
                            }
                        }
                        entryCount++;
                    }

                    std::vector<ConstantType> results(0);

                    ConstantType distrToNegativeDerivative = storm::utility::zero<ConstantType>();
                    ConstantType distrToPositiveDerivative = storm::utility::zero<ConstantType>();

                    for (auto const& direction : {OptimizationDirection::Maximize, OptimizationDirection::Minimize}) {
                        // Do a step of robust value iteration
                        // TODO I think it is a problem if we have probabilities and a state that is going to the vector, we don't count that
                        // Currently "fixed in preprocessing"
                        // It's different for rewards (same problem in ValueIterationOperator.h, search for word "octopus" in codebase)
                        ConstantType remainingValue = utility::one<ConstantType>();
                        ConstantType result = utility::zero<ConstantType>();

                        STORM_LOG_ASSERT(vector[rowIndex].upper() == vector[rowIndex].lower(),
                                         "Non-constant vector indices not supported (this includes parametric rewards).");

                        std::vector<std::pair<ConstantType, std::pair<ConstantType, uint64_t>>> robustOrder;

                        uint64_t index = 0;
                        for (auto const& entry : matrix.getRow(rowIndex)) {
                            auto const lower = entry.getValue().lower();
                            result += quantitativeResult[entry.getColumn()] * lower;
                            remainingValue -= lower;
                            auto const diameter = entry.getValue().upper() - lower;
                            if (!storm::utility::isZero(diameter)) {
                                robustOrder.emplace_back(quantitativeResult[entry.getColumn()], std::make_pair(diameter, index));
                            }
                            index++;
                        }

                        std::sort(robustOrder.begin(), robustOrder.end(),
                                  [direction](const std::pair<ConstantType, std::pair<ConstantType, uint64_t>>& a,
                                              const std::pair<ConstantType, std::pair<ConstantType, uint64_t>>& b) {
                                      if (direction == OptimizationDirection::Maximize) {
                                          return a.first > b.first;
                                      } else {
                                          return a.first < b.first;
                                      }
                                  });

                        for (auto const& pair : robustOrder) {
                            auto availableMass = std::min(pair.second.first, remainingValue);
                            result += availableMass * pair.first;
                            // TODO hardcoded precision
                            if (direction == this->currentCheckTask->getOptimizationDirection()) {
                                if (derivatives[pair.second.second] > 1e-6) {
                                    distrToPositiveDerivative += availableMass;
                                } else if (derivatives[pair.second.second] < 1e-6) {
                                    distrToNegativeDerivative += availableMass;
                                }
                            }
                            remainingValue -= availableMass;
                        }

                        results.push_back(result);
                    }

                    ConstantType diff = std::abs(results[0] - results[1]);
                    if (distrToPositiveDerivative > distrToNegativeDerivative) {  // Choose as upper
                        deltaUpper[p] += diff * weighting[rowIndex];
                    } else {  // Choose as lower
                        deltaLower[p] += diff * weighting[rowIndex];
                    }
                }
            } else {
                auto const& choiceValuations = parameterLifter->getRowLabels();

                std::vector<ConstantType> stateResults;
                for (uint64_t state = 0; state < schedulerChoices.size(); ++state) {
                    uint64_t rowOffset = matrix.getRowGroupIndices()[state];
                    uint64_t optimalChoice = schedulerChoices[state];
                    auto const& optimalChoiceVal = choiceValuations[rowOffset + optimalChoice];
                    assert(optimalChoiceVal.getUnspecifiedParameters().empty());
                    stateResults.clear();
                    for (uint64_t row = rowOffset; row < matrix.getRowGroupIndices()[state + 1]; ++row) {
                        stateResults.push_back(matrix.multiplyRowWithVector(row, quantitativeResult) + vector[row]);
                    }
                    // Do this twice, once for upperbound once for lowerbound
                    bool checkUpperParameters = false;
                    do {
                        auto const& consideredParameters = checkUpperParameters ? optimalChoiceVal.getUpperParameters() : optimalChoiceVal.getLowerParameters();
                        for (auto const& p : consideredParameters) {
                            // Find the 'best' choice that assigns the parameter to the other bound
                            ConstantType bestValue = 0;
                            bool foundBestValue = false;
                            for (uint64_t choice = 0; choice < stateResults.size(); ++choice) {
                                if (choice != optimalChoice) {
                                    auto const& otherBoundParsOfChoice = checkUpperParameters ? choiceValuations[rowOffset + choice].getLowerParameters()
                                                                                              : choiceValuations[rowOffset + choice].getUpperParameters();
                                    if (otherBoundParsOfChoice.find(p) != otherBoundParsOfChoice.end()) {
                                        ConstantType const& choiceValue = stateResults[choice];
                                        if (!foundBestValue ||
                                            (storm::solver::minimize(dirForParameters) ? choiceValue < bestValue : choiceValue > bestValue)) {
                                            foundBestValue = true;
                                            bestValue = choiceValue;
                                        }
                                    }
                                }
                            }
                            auto const& optimal = stateResults[optimalChoice];
                            auto diff = storm::utility::abs<ConstantType>(optimal - storm::utility::convertNumber<ConstantType>(bestValue));
                            if (foundBestValue) {
                                if (checkUpperParameters) {
                                    deltaLower[p] += diff * weighting[state];
                                } else {
                                    deltaUpper[p] += diff * weighting[state];
                                }
                            }
                        }
                        checkUpperParameters = !checkUpperParameters;
                    } while (checkUpperParameters);
                }
            }

            cachedRegionSplitEstimates.clear();
            for (auto const& p : region.getVariables()) {
                // TODO: previously, the reginSplitEstimates were only used in splitting if at least one parameter is possibly monotone. Why?
                auto minDelta = std::min(deltaLower[p], deltaUpper[p]);
                cachedRegionSplitEstimates.emplace(p, minDelta);
            }

            // large regionsplitestimate implies that parameter p occurs as p and 1-p at least once
            break;
        }
        case RegionSplitEstimateKind::Derivative: {
            storm::modelchecker::SparseDtmcInstantiationModelChecker<storm::models::sparse::Dtmc<ParametricType>, ConstantType> instantiationModelChecker(
                *this->parametricModel);
            instantiationModelChecker.specifyFormula(*this->currentCheckTaskNoBound);

            auto const center = region.getCenterPoint();

            std::unique_ptr<storm::modelchecker::CheckResult> result = instantiationModelChecker.check(env, center);
            auto const reachabilityProbabilities = result->asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();

            STORM_LOG_ASSERT(this->derivativeChecker, "Derivative checker not intialized");

            for (auto const& param : region.getVariables()) {
                auto result = this->derivativeChecker->check(env, center, param, reachabilityProbabilities);
                ConstantType derivative =
                    result->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector()[this->derivativeChecker->getInitialState()];
                cachedRegionSplitEstimates[param] = utility::abs(derivative) * utility::convertNumber<ConstantType>(region.getDifference(param));
            }
            break;
        }
        default:
            STORM_LOG_ERROR("Region split estimate kind not handled by SparseDtmcParameterLiftingModelChecker.");
    }
}

template<typename SparseModelType, typename ConstantType, bool Robust>
void SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::reset() {
    maybeStates.resize(0);
    resultsForNonMaybeStates.clear();
    stepBound = std::nullopt;
    instantiationChecker = nullptr;
    instantiationCheckerSAT = nullptr;
    instantiationCheckerVIO = nullptr;
    parameterLifter = nullptr;
    minSchedChoices = std::nullopt;
    maxSchedChoices = std::nullopt;
    x.clear();
    lowerResultBound = std::nullopt;
    upperResultBound = std::nullopt;
    cachedRegionSplitEstimates.clear();
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

template<typename SparseModelType, typename ConstantType, bool Robust>
std::optional<storm::storage::Scheduler<ConstantType>> SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::getCurrentMinScheduler() {
    return getSchedulerHelper<ConstantType>(minSchedChoices);
}

template<typename SparseModelType, typename ConstantType, bool Robust>
std::optional<storm::storage::Scheduler<ConstantType>> SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::getCurrentMaxScheduler() {
    return getSchedulerHelper<ConstantType>(maxSchedChoices);
}

bool supportsStateValueDeltaEstimates(storm::logic::Formula const& f) {
    if (f.isOperatorFormula()) {
        auto const& sub = f.asOperatorFormula().getSubformula();
        return sub.isUntilFormula() || sub.isEventuallyFormula();
    }
    return false;
}

bool supportsOrderBasedMonotonicity(storm::logic::Formula const& f) {
    if (f.isProbabilityOperatorFormula()) {
        auto const& sub = f.asProbabilityOperatorFormula().getSubformula();
        return sub.isUntilFormula() || sub.isEventuallyFormula() || sub.isBoundedUntilFormula();
    }
    return false;
}

template<typename SparseModelType, typename ConstantType, bool Robust>
bool SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::isRegionSplitEstimateKindSupported(
    RegionSplitEstimateKind kind, CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const {
    return RegionModelChecker<ParametricType>::isRegionSplitEstimateKindSupported(kind, checkTask) ||
           (supportsStateValueDeltaEstimates(checkTask.getFormula()) &&
            (kind == RegionSplitEstimateKind::StateValueDelta || kind == RegionSplitEstimateKind::StateValueDeltaWeighted)) ||
           kind == RegionSplitEstimateKind::Derivative;
}

template<typename SparseModelType, typename ConstantType, bool Robust>
RegionSplitEstimateKind SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::getDefaultRegionSplitEstimateKind(
    CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const {
    return supportsStateValueDeltaEstimates(checkTask.getFormula()) ? RegionSplitEstimateKind::StateValueDelta
                                                                    : RegionModelChecker<ParametricType>::getDefaultRegionSplitEstimateKind(checkTask);
}

template<typename SparseModelType, typename ConstantType, bool Robust>
std::vector<typename SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::CoefficientType>
SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::obtainRegionSplitEstimates(
    std::set<VariableType> const& relevantParameters) const {
    if (isValueDeltaRegionSplitEstimates()) {
        // Cached region split estimates are value-delta
        std::vector<CoefficientType> result;
        for (auto const& par : relevantParameters) {
            auto est = cachedRegionSplitEstimates.find(par);
            STORM_LOG_ASSERT(est != cachedRegionSplitEstimates.end(),
                             "Requested region split estimate for parameter " << par.name() << " but none was generated.");
            result.push_back(storm::utility::convertNumber<CoefficientType>(est->second));
        }
        return result;
    } else {
        // Call super method, which might support the estimate type
        return RegionModelChecker<ParametricType>::obtainRegionSplitEstimates(relevantParameters);
    }
}

template<typename SparseModelType, typename ConstantType, bool Robust>
bool SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::isMonotonicitySupported(
    MonotonicityBackend<ParametricType> const& backend, CheckTask<storm::logic::Formula, ParametricType> const& checkTask) const {
    if (backend.requiresInteractionWithRegionModelChecker()) {
        return dynamic_cast<OrderBasedMonotonicityBackend<ParametricType, ConstantType> const*>(&backend) != nullptr &&
               supportsOrderBasedMonotonicity(checkTask.getFormula());
    } else {
        return true;
    }
}

template<typename SparseModelType, typename ConstantType, bool Robust>
bool SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::isOrderBasedMonotonicityBackend() const {
    return dynamic_cast<OrderBasedMonotonicityBackend<ParametricType, ConstantType>*>(this->monotonicityBackend.get()) != nullptr;
}

template<typename SparseModelType, typename ConstantType, bool Robust>
OrderBasedMonotonicityBackend<typename SparseModelType::ValueType, ConstantType>&
SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::getOrderBasedMonotonicityBackend() {
    return dynamic_cast<OrderBasedMonotonicityBackend<ParametricType, ConstantType>&>(*this->monotonicityBackend);
}

template<typename SparseModelType, typename ConstantType, bool Robust>
bool SparseDtmcParameterLiftingModelChecker<SparseModelType, ConstantType, Robust>::isValueDeltaRegionSplitEstimates() const {
    return this->getSpecifiedRegionSplitEstimateKind().has_value() &&
           (this->getSpecifiedRegionSplitEstimateKind().value() == RegionSplitEstimateKind::StateValueDelta ||
            this->getSpecifiedRegionSplitEstimateKind().value() == RegionSplitEstimateKind::StateValueDeltaWeighted ||
            this->getSpecifiedRegionSplitEstimateKind().value() == RegionSplitEstimateKind::Derivative);
}

template class SparseDtmcParameterLiftingModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, double, false>;
template class SparseDtmcParameterLiftingModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, double, true>;
template class SparseDtmcParameterLiftingModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, storm::RationalNumber, false>;
}  // namespace modelchecker
}  // namespace storm
