#include "storm/modelchecker/prctl/SymbolicDtmcPrctlModelChecker.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/prctl/helper/SymbolicDtmcPrctlHelper.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQuantitativeCheckResult.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/Add.h"
#include "storm/utility/FilteredRewardModel.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
template<typename ModelType>
SymbolicDtmcPrctlModelChecker<ModelType>::SymbolicDtmcPrctlModelChecker(ModelType const& model) : SymbolicPropositionalModelChecker<ModelType>(model) {
    // Intentionally left empty.
}

template<typename ModelType>
bool SymbolicDtmcPrctlModelChecker<ModelType>::canHandleStatic(CheckTask<storm::logic::Formula, ValueType> const& checkTask) {
    storm::logic::Formula const& formula = checkTask.getFormula();
    return formula.isInFragment(storm::logic::prctl()
                                    .setLongRunAverageRewardFormulasAllowed(false)
                                    .setTimeOperatorsAllowed(true)
                                    .setReachbilityTimeFormulasAllowed(true)
                                    .setRewardAccumulationAllowed(true));
}

template<typename ModelType>
bool SymbolicDtmcPrctlModelChecker<ModelType>::canHandle(CheckTask<storm::logic::Formula, ValueType> const& checkTask) const {
    return canHandleStatic(checkTask);
}

template<typename ModelType>
std::unique_ptr<CheckResult> SymbolicDtmcPrctlModelChecker<ModelType>::computeUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) {
    storm::logic::UntilFormula const& pathFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
    std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
    SymbolicQualitativeCheckResult<DdType> const& leftResult = leftResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    SymbolicQualitativeCheckResult<DdType> const& rightResult = rightResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    storm::dd::Add<DdType, ValueType> numericResult = storm::modelchecker::helper::SymbolicDtmcPrctlHelper<DdType, ValueType>::computeUntilProbabilities(
        env, this->getModel(), this->getModel().getTransitionMatrix(), leftResult.getTruthValuesVector(), rightResult.getTruthValuesVector(),
        checkTask.isQualitativeSet());
    return std::make_unique<SymbolicQuantitativeCheckResult<DdType, ValueType>>(this->getModel().getReachableStates(), numericResult);
}

template<typename ModelType>
std::unique_ptr<CheckResult> SymbolicDtmcPrctlModelChecker<ModelType>::computeGloballyProbabilities(
    Environment const& env, CheckTask<storm::logic::GloballyFormula, ValueType> const& checkTask) {
    storm::logic::GloballyFormula const& pathFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, pathFormula.getSubformula());
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    storm::dd::Add<DdType, ValueType> numericResult = storm::modelchecker::helper::SymbolicDtmcPrctlHelper<DdType, ValueType>::computeGloballyProbabilities(
        env, this->getModel(), this->getModel().getTransitionMatrix(), subResult.getTruthValuesVector(), checkTask.isQualitativeSet());
    return std::make_unique<SymbolicQuantitativeCheckResult<DdType, ValueType>>(this->getModel().getReachableStates(), numericResult);
}

template<typename ModelType>
std::unique_ptr<CheckResult> SymbolicDtmcPrctlModelChecker<ModelType>::computeNextProbabilities(
    Environment const& env, CheckTask<storm::logic::NextFormula, ValueType> const& checkTask) {
    storm::logic::NextFormula const& pathFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, pathFormula.getSubformula());
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    storm::dd::Add<DdType, ValueType> numericResult = storm::modelchecker::helper::SymbolicDtmcPrctlHelper<DdType, ValueType>::computeNextProbabilities(
        env, this->getModel(), this->getModel().getTransitionMatrix(), subResult.getTruthValuesVector());
    return std::make_unique<SymbolicQuantitativeCheckResult<DdType, ValueType>>(this->getModel().getReachableStates(), numericResult);
}

template<typename ModelType>
std::unique_ptr<CheckResult> SymbolicDtmcPrctlModelChecker<ModelType>::computeBoundedUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::BoundedUntilFormula, ValueType> const& checkTask) {
    storm::logic::BoundedUntilFormula const& pathFormula = checkTask.getFormula();
    STORM_LOG_THROW(!pathFormula.hasLowerBound() && pathFormula.hasUpperBound(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to have (a single) upper time bound, and no lower bound.");
    STORM_LOG_THROW(pathFormula.hasIntegerUpperBound(), storm::exceptions::InvalidPropertyException, "Formula needs to have discrete upper time bound.");
    std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
    std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
    SymbolicQualitativeCheckResult<DdType> const& leftResult = leftResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    SymbolicQualitativeCheckResult<DdType> const& rightResult = rightResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    storm::dd::Add<DdType, ValueType> numericResult = storm::modelchecker::helper::SymbolicDtmcPrctlHelper<DdType, ValueType>::computeBoundedUntilProbabilities(
        env, this->getModel(), this->getModel().getTransitionMatrix(), leftResult.getTruthValuesVector(), rightResult.getTruthValuesVector(),
        pathFormula.getNonStrictUpperBound<uint64_t>());
    return std::unique_ptr<SymbolicQuantitativeCheckResult<DdType, ValueType>>(
        new SymbolicQuantitativeCheckResult<DdType, ValueType>(this->getModel().getReachableStates(), numericResult));
}

template<typename ModelType>
std::unique_ptr<CheckResult> SymbolicDtmcPrctlModelChecker<ModelType>::computeCumulativeRewards(
    Environment const& env, CheckTask<storm::logic::CumulativeRewardFormula, ValueType> const& checkTask) {
    storm::logic::CumulativeRewardFormula const& rewardPathFormula = checkTask.getFormula();
    STORM_LOG_THROW(rewardPathFormula.hasIntegerBound(), storm::exceptions::InvalidPropertyException, "Formula needs to have a discrete time bound.");
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    storm::dd::Add<DdType, ValueType> numericResult = storm::modelchecker::helper::SymbolicDtmcPrctlHelper<DdType, ValueType>::computeCumulativeRewards(
        env, this->getModel(), this->getModel().getTransitionMatrix(), rewardModel.get(), rewardPathFormula.getNonStrictBound<uint64_t>());
    return std::unique_ptr<SymbolicQuantitativeCheckResult<DdType, ValueType>>(
        new SymbolicQuantitativeCheckResult<DdType, ValueType>(this->getModel().getReachableStates(), numericResult));
}

template<typename ModelType>
std::unique_ptr<CheckResult> SymbolicDtmcPrctlModelChecker<ModelType>::computeInstantaneousRewards(
    Environment const& env, CheckTask<storm::logic::InstantaneousRewardFormula, ValueType> const& checkTask) {
    storm::logic::InstantaneousRewardFormula const& rewardPathFormula = checkTask.getFormula();
    STORM_LOG_THROW(rewardPathFormula.hasIntegerBound(), storm::exceptions::InvalidPropertyException, "Formula needs to have a discrete time bound.");
    storm::dd::Add<DdType, ValueType> numericResult = storm::modelchecker::helper::SymbolicDtmcPrctlHelper<DdType, ValueType>::computeInstantaneousRewards(
        env, this->getModel(), this->getModel().getTransitionMatrix(),
        checkTask.isRewardModelSet() ? this->getModel().getRewardModel(checkTask.getRewardModel()) : this->getModel().getRewardModel(""),
        rewardPathFormula.getBound<uint64_t>());
    return std::make_unique<SymbolicQuantitativeCheckResult<DdType, ValueType>>(this->getModel().getReachableStates(), numericResult);
}

template<typename ModelType>
std::unique_ptr<CheckResult> SymbolicDtmcPrctlModelChecker<ModelType>::computeReachabilityRewards(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    storm::dd::Add<DdType, ValueType> numericResult = storm::modelchecker::helper::SymbolicDtmcPrctlHelper<DdType, ValueType>::computeReachabilityRewards(
        env, this->getModel(), this->getModel().getTransitionMatrix(), rewardModel.get(), subResult.getTruthValuesVector(), checkTask.isQualitativeSet());
    return std::make_unique<SymbolicQuantitativeCheckResult<DdType, ValueType>>(this->getModel().getReachableStates(), numericResult);
}

template<typename ModelType>
std::unique_ptr<CheckResult> SymbolicDtmcPrctlModelChecker<ModelType>::computeReachabilityTimes(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    storm::dd::Add<DdType, ValueType> numericResult = storm::modelchecker::helper::SymbolicDtmcPrctlHelper<DdType, ValueType>::computeReachabilityTimes(
        env, this->getModel(), this->getModel().getTransitionMatrix(), subResult.getTruthValuesVector(), checkTask.isQualitativeSet());
    return std::make_unique<SymbolicQuantitativeCheckResult<DdType, ValueType>>(this->getModel().getReachableStates(), numericResult);
}

template class SymbolicDtmcPrctlModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::CUDD, double>>;
template class SymbolicDtmcPrctlModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, double>>;

template class SymbolicDtmcPrctlModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class SymbolicDtmcPrctlModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, storm::RationalFunction>>;
}  // namespace modelchecker
}  // namespace storm
