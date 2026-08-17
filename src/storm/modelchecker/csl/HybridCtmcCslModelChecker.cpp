#include "storm/modelchecker/csl/HybridCtmcCslModelChecker.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/csl/helper/HybridCtmcCslHelper.h"
#include "storm/modelchecker/csl/helper/SparseCtmcCslHelper.h"
#include "storm/modelchecker/helper/infinitehorizon/HybridInfiniteHorizonHelper.h"
#include "storm/modelchecker/helper/utility/SetInformationFromCheckTask.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/utility/FilteredRewardModel.h"

namespace storm {
namespace modelchecker {
template<typename ModelType>
HybridCtmcCslModelChecker<ModelType>::HybridCtmcCslModelChecker(ModelType const& model) : SymbolicPropositionalModelChecker<ModelType>(model) {
    // Intentionally left empty.
}

template<typename ModelType>
bool HybridCtmcCslModelChecker<ModelType>::canHandleStatic(CheckTask<storm::logic::Formula, ValueType> const& checkTask) {
    auto fragment = storm::logic::csrl()
                        .setGloballyFormulasAllowed(false)
                        .setLongRunAverageRewardFormulasAllowed(true)
                        .setLongRunAverageProbabilitiesAllowed(true)
                        .setTimeAllowed(true)
                        .setTimeOperatorsAllowed(true)
                        .setRewardAccumulationAllowed(true);
    if (!storm::NumberTraits<ValueType>::SupportsExponential) {
        fragment.setBoundedUntilFormulasAllowed(false).setCumulativeRewardFormulasAllowed(false).setInstantaneousFormulasAllowed(false);
    }
    return checkTask.getFormula().isInFragment(fragment);
}

template<typename ModelType>
bool HybridCtmcCslModelChecker<ModelType>::canHandle(CheckTask<storm::logic::Formula, ValueType> const& checkTask) const {
    return canHandleStatic(checkTask);
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridCtmcCslModelChecker<ModelType>::computeUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) {
    storm::logic::UntilFormula const& pathFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
    std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
    SymbolicQualitativeCheckResult<DdType> const& leftResult = leftResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    SymbolicQualitativeCheckResult<DdType> const& rightResult = rightResultPointer->asSymbolicQualitativeCheckResult<DdType>();

    return storm::modelchecker::helper::HybridCtmcCslHelper::computeUntilProbabilities<DdType, ValueType>(
        env, this->getModel(), this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector(), leftResult.getTruthValuesVector(),
        rightResult.getTruthValuesVector(), checkTask.isQualitativeSet());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridCtmcCslModelChecker<ModelType>::computeNextProbabilities(Environment const& env,
                                                                                            CheckTask<storm::logic::NextFormula, ValueType> const& checkTask) {
    storm::logic::NextFormula const& pathFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, pathFormula.getSubformula());
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();

    return storm::modelchecker::helper::HybridCtmcCslHelper::computeNextProbabilities<DdType, ValueType>(
        env, this->getModel(), this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector(), subResult.getTruthValuesVector());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridCtmcCslModelChecker<ModelType>::computeReachabilityRewards(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    return storm::modelchecker::helper::HybridCtmcCslHelper::computeReachabilityRewards<DdType, ValueType>(
        env, this->getModel(), this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector(), rewardModel.get(),
        subResult.getTruthValuesVector(), checkTask.isQualitativeSet());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridCtmcCslModelChecker<ModelType>::computeReachabilityTimes(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();

    storm::models::symbolic::StandardRewardModel<DdType, ValueType> timeRewardModel(this->getModel().getManager().getConstant(storm::utility::one<ValueType>()),
                                                                                    boost::none, boost::none);
    return storm::modelchecker::helper::HybridCtmcCslHelper::computeReachabilityRewards<DdType, ValueType>(
        env, this->getModel(), this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector(), timeRewardModel, subResult.getTruthValuesVector(),
        checkTask.isQualitativeSet());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridCtmcCslModelChecker<ModelType>::computeBoundedUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::BoundedUntilFormula, ValueType> const& checkTask) {
    if constexpr (!storm::NumberTraits<ValueType>::SupportsExponential) {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing bounded until probabilities is not supported for this numeric type.");
        return nullptr;
    } else {
        storm::logic::BoundedUntilFormula const& pathFormula = checkTask.getFormula();
        std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
        std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
        SymbolicQualitativeCheckResult<DdType> const& leftResult = leftResultPointer->asSymbolicQualitativeCheckResult<DdType>();
        SymbolicQualitativeCheckResult<DdType> const& rightResult = rightResultPointer->asSymbolicQualitativeCheckResult<DdType>();

        STORM_LOG_THROW(pathFormula.getTimeBoundReference().isTimeBound(), storm::exceptions::NotImplementedException,
                        "Currently step-bounded and reward-bounded properties on CTMCs are not supported.");
        ValueType lowerBound = 0;
        ValueType upperBound = 0;
        if (pathFormula.hasLowerBound()) {
            lowerBound = pathFormula.getLowerBound<ValueType>();
        }
        if (pathFormula.hasUpperBound()) {
            upperBound = pathFormula.getNonStrictUpperBound<ValueType>();
        } else {
            upperBound = storm::utility::infinity<ValueType>();
        }

        return storm::modelchecker::helper::HybridCtmcCslHelper::computeBoundedUntilProbabilities<DdType, ValueType>(
            env, this->getModel(), checkTask.isOnlyInitialStatesRelevantSet(), this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector(),
            leftResult.getTruthValuesVector(), rightResult.getTruthValuesVector(), checkTask.isQualitativeSet(), lowerBound, upperBound);
    }
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridCtmcCslModelChecker<ModelType>::computeInstantaneousRewards(
    Environment const& env, CheckTask<storm::logic::InstantaneousRewardFormula, ValueType> const& checkTask) {
    if constexpr (!storm::NumberTraits<ValueType>::SupportsExponential) {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing Instantaneous Rewards is not supported for this numeric type.");
        return nullptr;
    } else {
        storm::logic::InstantaneousRewardFormula const& rewardPathFormula = checkTask.getFormula();

        STORM_LOG_THROW(!rewardPathFormula.isStepBounded(), storm::exceptions::NotImplementedException,
                        "Currently step-bounded properties on CTMCs are not supported.");
        return storm::modelchecker::helper::HybridCtmcCslHelper::computeInstantaneousRewards<DdType, ValueType>(
            env, this->getModel(), checkTask.isOnlyInitialStatesRelevantSet(), this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector(),
            checkTask.isRewardModelSet() ? this->getModel().getRewardModel(checkTask.getRewardModel()) : this->getModel().getRewardModel(""),
            rewardPathFormula.getBound<ValueType>());
    }
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridCtmcCslModelChecker<ModelType>::computeCumulativeRewards(
    Environment const& env, CheckTask<storm::logic::CumulativeRewardFormula, ValueType> const& checkTask) {
    if constexpr (!storm::NumberTraits<ValueType>::SupportsExponential) {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing Cumulative Rewards is not supported for this numeric type.");
        return nullptr;
    } else {
        storm::logic::CumulativeRewardFormula const& rewardPathFormula = checkTask.getFormula();

        STORM_LOG_THROW(rewardPathFormula.getTimeBoundReference().isTimeBound(), storm::exceptions::NotImplementedException,
                        "Currently step-bounded and reward-bounded properties on CTMCs are not supported.");
        auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
        return storm::modelchecker::helper::HybridCtmcCslHelper::computeCumulativeRewards<DdType, ValueType>(
            env, this->getModel(), checkTask.isOnlyInitialStatesRelevantSet(), this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector(),
            rewardModel.get(), rewardPathFormula.getNonStrictBound<ValueType>());
    }
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridCtmcCslModelChecker<ModelType>::computeLongRunAverageProbabilities(
    Environment const& env, CheckTask<storm::logic::StateFormula, ValueType> const& checkTask) {
    storm::logic::StateFormula const& stateFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, stateFormula);
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();

    auto probabilisticTransitions = this->getModel().computeProbabilityMatrix();
    storm::modelchecker::helper::HybridInfiniteHorizonHelper<ValueType, DdType, false> helper(this->getModel(), probabilisticTransitions,
                                                                                              this->getModel().getExitRateVector());
    storm::modelchecker::helper::setInformationFromCheckTaskDeterministic(helper, checkTask, this->getModel());
    return helper.computeLongRunAverageProbabilities(env, subResult.getTruthValuesVector());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridCtmcCslModelChecker<ModelType>::computeLongRunAverageRewards(
    Environment const& env, CheckTask<storm::logic::LongRunAverageRewardFormula, ValueType> const& checkTask) {
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    auto probabilisticTransitions = this->getModel().computeProbabilityMatrix();
    storm::modelchecker::helper::HybridInfiniteHorizonHelper<ValueType, DdType, false> helper(this->getModel(), probabilisticTransitions,
                                                                                              this->getModel().getExitRateVector());
    storm::modelchecker::helper::setInformationFromCheckTaskDeterministic(helper, checkTask, this->getModel());
    return helper.computeLongRunAverageRewards(env, rewardModel.get());
}

// Explicitly instantiate the model checker.
template class HybridCtmcCslModelChecker<storm::models::symbolic::Ctmc<storm::dd::DdType::CUDD, double>>;
template class HybridCtmcCslModelChecker<storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, double>>;

template class HybridCtmcCslModelChecker<storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class HybridCtmcCslModelChecker<storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalFunction>>;

}  // namespace modelchecker
}  // namespace storm
