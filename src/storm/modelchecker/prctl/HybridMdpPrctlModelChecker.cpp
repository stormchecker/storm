#include "storm/modelchecker/prctl/HybridMdpPrctlModelChecker.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/multiobjective/MultiObjectiveModelChecking.h"
#include "storm/modelchecker/prctl/helper/HybridMdpPrctlHelper.h"
#include "storm/modelchecker/results/ExplicitParetoCurveCheckResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicParetoCurveCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQuantitativeCheckResult.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/transformer/SymbolicToSparseTransformer.h"
#include "storm/utility/FilteredRewardModel.h"

namespace storm {
namespace modelchecker {
template<typename ModelType>
HybridMdpPrctlModelChecker<ModelType>::HybridMdpPrctlModelChecker(ModelType const& model) : SymbolicPropositionalModelChecker<ModelType>(model) {
    // Intentionally left empty.
}

template<typename ModelType>
bool HybridMdpPrctlModelChecker<ModelType>::canHandleStatic(CheckTask<storm::logic::Formula, ValueType> const& checkTask, bool* requiresSingleInitialState) {
    storm::logic::Formula const& formula = checkTask.getFormula();
    if (formula.isInFragment(storm::logic::prctl()
                                 .setLongRunAverageRewardFormulasAllowed(false)
                                 .setTimeOperatorsAllowed(true)
                                 .setReachbilityTimeFormulasAllowed(true)
                                 .setRewardAccumulationAllowed(true))) {
        return true;
    } else if (checkTask.isOnlyInitialStatesRelevantSet() && formula.isInFragment(storm::logic::multiObjective().setCumulativeRewardFormulasAllowed(true))) {
        if (requiresSingleInitialState) {
            *requiresSingleInitialState = true;
        }
        return true;
    }
    return false;
}

template<typename ModelType>
bool HybridMdpPrctlModelChecker<ModelType>::canHandle(CheckTask<storm::logic::Formula, ValueType> const& checkTask) const {
    bool requiresSingleInitialState = false;
    if (canHandleStatic(checkTask, &requiresSingleInitialState)) {
        return !requiresSingleInitialState || this->getModel().getInitialStates().getNonZeroCount() == 1;
    } else {
        return false;
    }
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMdpPrctlModelChecker<ModelType>::computeUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) {
    storm::logic::UntilFormula const& pathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
    std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
    SymbolicQualitativeCheckResult<DdType> const& leftResult = leftResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    SymbolicQualitativeCheckResult<DdType> const& rightResult = rightResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    return storm::modelchecker::helper::HybridMdpPrctlHelper<DdType, ValueType>::computeUntilProbabilities(
        env, checkTask.getOptimizationDirection(), this->getModel(), this->getModel().getTransitionMatrix(), leftResult.getTruthValuesVector(),
        rightResult.getTruthValuesVector(), checkTask.isQualitativeSet());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMdpPrctlModelChecker<ModelType>::computeGloballyProbabilities(
    Environment const& env, CheckTask<storm::logic::GloballyFormula, ValueType> const& checkTask) {
    storm::logic::GloballyFormula const& pathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, pathFormula.getSubformula());
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    return storm::modelchecker::helper::HybridMdpPrctlHelper<DdType, ValueType>::computeGloballyProbabilities(
        env, checkTask.getOptimizationDirection(), this->getModel(), this->getModel().getTransitionMatrix(), subResult.getTruthValuesVector(),
        checkTask.isQualitativeSet());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMdpPrctlModelChecker<ModelType>::computeNextProbabilities(Environment const& env,
                                                                                             CheckTask<storm::logic::NextFormula, ValueType> const& checkTask) {
    storm::logic::NextFormula const& pathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, pathFormula.getSubformula());
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    return storm::modelchecker::helper::HybridMdpPrctlHelper<DdType, ValueType>::computeNextProbabilities(
        env, checkTask.getOptimizationDirection(), this->getModel(), this->getModel().getTransitionMatrix(), subResult.getTruthValuesVector());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMdpPrctlModelChecker<ModelType>::computeBoundedUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::BoundedUntilFormula, ValueType> const& checkTask) {
    storm::logic::BoundedUntilFormula const& pathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    STORM_LOG_THROW(!pathFormula.hasLowerBound() && pathFormula.hasUpperBound(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to have (a single) upper time bound, and no lower bound.");
    STORM_LOG_THROW(pathFormula.hasIntegerUpperBound(), storm::exceptions::InvalidPropertyException, "Formula needs to have discrete upper time bound.");
    std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
    std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
    SymbolicQualitativeCheckResult<DdType> const& leftResult = leftResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    SymbolicQualitativeCheckResult<DdType> const& rightResult = rightResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    return storm::modelchecker::helper::HybridMdpPrctlHelper<DdType, ValueType>::computeBoundedUntilProbabilities(
        env, checkTask.getOptimizationDirection(), this->getModel(), this->getModel().getTransitionMatrix(), leftResult.getTruthValuesVector(),
        rightResult.getTruthValuesVector(), pathFormula.getNonStrictUpperBound<uint64_t>());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMdpPrctlModelChecker<ModelType>::computeCumulativeRewards(
    Environment const& env, CheckTask<storm::logic::CumulativeRewardFormula, ValueType> const& checkTask) {
    storm::logic::CumulativeRewardFormula const& rewardPathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    STORM_LOG_THROW(rewardPathFormula.hasIntegerBound(), storm::exceptions::InvalidPropertyException, "Formula needs to have a discrete time bound.");
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    return storm::modelchecker::helper::HybridMdpPrctlHelper<DdType, ValueType>::computeCumulativeRewards(
        env, checkTask.getOptimizationDirection(), this->getModel(), this->getModel().getTransitionMatrix(), rewardModel.get(),
        rewardPathFormula.getNonStrictBound<uint64_t>());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMdpPrctlModelChecker<ModelType>::computeInstantaneousRewards(
    Environment const& env, CheckTask<storm::logic::InstantaneousRewardFormula, ValueType> const& checkTask) {
    storm::logic::InstantaneousRewardFormula const& rewardPathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    STORM_LOG_THROW(rewardPathFormula.hasIntegerBound(), storm::exceptions::InvalidPropertyException, "Formula needs to have a discrete time bound.");
    return storm::modelchecker::helper::HybridMdpPrctlHelper<DdType, ValueType>::computeInstantaneousRewards(
        env, checkTask.getOptimizationDirection(), this->getModel(), this->getModel().getTransitionMatrix(),
        checkTask.isRewardModelSet() ? this->getModel().getRewardModel(checkTask.getRewardModel()) : this->getModel().getRewardModel(""),
        rewardPathFormula.getBound<uint64_t>());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMdpPrctlModelChecker<ModelType>::computeReachabilityRewards(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    return storm::modelchecker::helper::HybridMdpPrctlHelper<DdType, ValueType>::computeReachabilityRewards(
        env, checkTask.getOptimizationDirection(), this->getModel(), this->getModel().getTransitionMatrix(), rewardModel.get(),
        subResult.getTruthValuesVector(), checkTask.isQualitativeSet());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMdpPrctlModelChecker<ModelType>::computeReachabilityTimes(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    return storm::modelchecker::helper::HybridMdpPrctlHelper<DdType, ValueType>::computeReachabilityTimes(
        env, checkTask.getOptimizationDirection(), this->getModel(), this->getModel().getTransitionMatrix(), subResult.getTruthValuesVector(),
        checkTask.isQualitativeSet());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMdpPrctlModelChecker<ModelType>::checkMultiObjectiveFormula(
    Environment const& env, CheckTask<storm::logic::MultiObjectiveFormula, ValueType> const& checkTask) {
    auto sparseModel = storm::transformer::SymbolicMdpToSparseMdpTransformer<DdType, ValueType>::translate(this->getModel());
    std::unique_ptr<CheckResult> explicitResult =
        multiobjective::performMultiObjectiveModelChecking(env, *sparseModel, checkTask.getFormula(), checkTask.isProduceSchedulersSet());

    // Convert the explicit result
    if (explicitResult->isExplicitQualitativeCheckResult()) {
        if (explicitResult->template asExplicitQualitativeCheckResult<ValueType>()[*sparseModel->getInitialStates().begin()]) {
            return std::unique_ptr<CheckResult>(new storm::modelchecker::SymbolicQualitativeCheckResult<DdType>(
                this->getModel().getReachableStates(), this->getModel().getInitialStates(), this->getModel().getManager().getBddOne()));
        } else {
            return std::unique_ptr<CheckResult>(new storm::modelchecker::SymbolicQualitativeCheckResult<DdType>(
                this->getModel().getReachableStates(), this->getModel().getInitialStates(), this->getModel().getManager().getBddZero()));
        }
    } else if (explicitResult->isExplicitQuantitativeCheckResult()) {
        ValueType const& res = explicitResult->template asExplicitQuantitativeCheckResult<ValueType>()[*sparseModel->getInitialStates().begin()];
        return std::unique_ptr<CheckResult>(new storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>(
            this->getModel().getReachableStates(), this->getModel().getInitialStates(), this->getModel().getManager().template getConstant<ValueType>(res)));
    } else if (explicitResult->isExplicitParetoCurveCheckResult()) {
        ExplicitParetoCurveCheckResult<ValueType> const& paretoResult = explicitResult->template asExplicitParetoCurveCheckResult<ValueType>();
        return std::unique_ptr<CheckResult>(new storm::modelchecker::SymbolicParetoCurveCheckResult<DdType, ValueType>(
            this->getModel().getInitialStates(), paretoResult.getPoints(), paretoResult.getUnderApproximation(), paretoResult.getOverApproximation()));
    } else {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "The obtained checkresult has an unexpected type.");
        return nullptr;
    }
}

template class HybridMdpPrctlModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::CUDD, double>>;
template class HybridMdpPrctlModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::Sylvan, double>>;
template class HybridMdpPrctlModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::Sylvan, storm::RationalNumber>>;

}  // namespace modelchecker
}  // namespace storm
