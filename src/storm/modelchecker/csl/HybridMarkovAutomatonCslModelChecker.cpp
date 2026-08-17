#include "storm/modelchecker/csl/HybridMarkovAutomatonCslModelChecker.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/csl/helper/HybridMarkovAutomatonCslHelper.h"
#include "storm/modelchecker/csl/helper/SparseMarkovAutomatonCslHelper.h"
#include "storm/modelchecker/helper/infinitehorizon/HybridInfiniteHorizonHelper.h"
#include "storm/modelchecker/helper/utility/SetInformationFromCheckTask.h"
#include "storm/modelchecker/prctl/helper/HybridMdpPrctlHelper.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/FilteredRewardModel.h"

namespace storm {
namespace modelchecker {
template<typename ModelType>
HybridMarkovAutomatonCslModelChecker<ModelType>::HybridMarkovAutomatonCslModelChecker(ModelType const& model)
    : SymbolicPropositionalModelChecker<ModelType>(model) {
    // Intentionally left empty.
}

template<typename ModelType>
bool HybridMarkovAutomatonCslModelChecker<ModelType>::canHandleStatic(CheckTask<storm::logic::Formula, ValueType> const& checkTask) {
    auto singleObjectiveFragment = storm::logic::csl()
                                       .setGloballyFormulasAllowed(false)
                                       .setNextFormulasAllowed(false)
                                       .setRewardOperatorsAllowed(true)
                                       .setReachabilityRewardFormulasAllowed(true)
                                       .setTotalRewardFormulasAllowed(false)
                                       .setTimeAllowed(true)
                                       .setLongRunAverageProbabilitiesAllowed(true)
                                       .setLongRunAverageRewardFormulasAllowed(true)
                                       .setRewardAccumulationAllowed(true)
                                       .setInstantaneousFormulasAllowed(false)
                                       .setCumulativeRewardFormulasAllowed(false);
    if (!storm::NumberTraits<ValueType>::SupportsExponential) {
        singleObjectiveFragment.setBoundedUntilFormulasAllowed(false);
    }
    return checkTask.getFormula().isInFragment(singleObjectiveFragment);
}

template<typename ModelType>
bool HybridMarkovAutomatonCslModelChecker<ModelType>::canHandle(CheckTask<storm::logic::Formula, ValueType> const& checkTask) const {
    return canHandleStatic(checkTask);
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMarkovAutomatonCslModelChecker<ModelType>::computeUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) {
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    storm::logic::UntilFormula const& pathFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
    std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
    SymbolicQualitativeCheckResult<DdType> const& leftResult = leftResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    SymbolicQualitativeCheckResult<DdType> const& rightResult = rightResultPointer->asSymbolicQualitativeCheckResult<DdType>();

    return storm::modelchecker::helper::HybridMdpPrctlHelper<DdType, ValueType>::computeUntilProbabilities(
        env, checkTask.getOptimizationDirection(), this->getModel(), this->getModel().getTransitionMatrix(), leftResult.getTruthValuesVector(),
        rightResult.getTruthValuesVector(), checkTask.isQualitativeSet());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMarkovAutomatonCslModelChecker<ModelType>::computeReachabilityRewards(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    return storm::modelchecker::helper::HybridMarkovAutomatonCslHelper::computeReachabilityRewards<DdType, ValueType>(
        env, checkTask.getOptimizationDirection(), this->getModel(), this->getModel().getTransitionMatrix(), this->getModel().getMarkovianStates(),
        this->getModel().getExitRateVector(), rewardModel.get(), subResult.getTruthValuesVector(), checkTask.isQualitativeSet());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMarkovAutomatonCslModelChecker<ModelType>::computeReachabilityTimes(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");

    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();

    storm::models::symbolic::StandardRewardModel<DdType, ValueType> timeRewardModel(this->getModel().getManager().getConstant(storm::utility::one<ValueType>()),
                                                                                    boost::none, boost::none);
    return storm::modelchecker::helper::HybridMarkovAutomatonCslHelper::computeReachabilityRewards<DdType, ValueType>(
        env, checkTask.getOptimizationDirection(), this->getModel(), this->getModel().getTransitionMatrix(), this->getModel().getMarkovianStates(),
        this->getModel().getExitRateVector(), timeRewardModel, subResult.getTruthValuesVector(), checkTask.isQualitativeSet());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMarkovAutomatonCslModelChecker<ModelType>::computeBoundedUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::BoundedUntilFormula, ValueType> const& checkTask) {
    storm::logic::BoundedUntilFormula const& pathFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
    SymbolicQualitativeCheckResult<DdType> const& leftResult = leftResultPointer->asSymbolicQualitativeCheckResult<DdType>();

    std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
    SymbolicQualitativeCheckResult<DdType> const& rightResult = rightResultPointer->asSymbolicQualitativeCheckResult<DdType>();

    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    STORM_LOG_THROW(pathFormula.getTimeBoundReference().isTimeBound(), storm::exceptions::NotImplementedException,
                    "Currently step-bounded and reward-bounded properties on MarkovAutomatons are not supported.");
    double lowerBound = 0;
    double upperBound = 0;
    if (pathFormula.hasLowerBound()) {
        lowerBound = pathFormula.getLowerBound<double>();
    }
    if (pathFormula.hasUpperBound()) {
        upperBound = pathFormula.getNonStrictUpperBound<double>();
    } else {
        upperBound = storm::utility::infinity<double>();
    }

    return storm::modelchecker::helper::HybridMarkovAutomatonCslHelper::computeBoundedUntilProbabilities<DdType, ValueType>(
        env, checkTask.getOptimizationDirection(), this->getModel(), this->getModel().getTransitionMatrix(), this->getModel().getMarkovianStates(),
        this->getModel().getExitRateVector(), leftResult.getTruthValuesVector(), rightResult.getTruthValuesVector(), checkTask.isQualitativeSet(), lowerBound,
        upperBound);
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMarkovAutomatonCslModelChecker<ModelType>::computeLongRunAverageProbabilities(
    Environment const& env, CheckTask<storm::logic::StateFormula, ValueType> const& checkTask) {
    storm::logic::StateFormula const& stateFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, stateFormula);
    SymbolicQualitativeCheckResult<DdType> const& subResult = subResultPointer->asSymbolicQualitativeCheckResult<DdType>();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");

    storm::modelchecker::helper::HybridInfiniteHorizonHelper<ValueType, DdType, true> helper(
        this->getModel(), this->getModel().getTransitionMatrix(), this->getModel().getMarkovianStates(), this->getModel().getExitRateVector());
    storm::modelchecker::helper::setInformationFromCheckTaskNondeterministic(helper, checkTask, this->getModel());
    return helper.computeLongRunAverageProbabilities(env, subResult.getTruthValuesVector());
}

template<typename ModelType>
std::unique_ptr<CheckResult> HybridMarkovAutomatonCslModelChecker<ModelType>::computeLongRunAverageRewards(
    Environment const& env, CheckTask<storm::logic::LongRunAverageRewardFormula, ValueType> const& checkTask) {
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    storm::modelchecker::helper::HybridInfiniteHorizonHelper<ValueType, DdType, true> helper(
        this->getModel(), this->getModel().getTransitionMatrix(), this->getModel().getMarkovianStates(), this->getModel().getExitRateVector());
    storm::modelchecker::helper::setInformationFromCheckTaskNondeterministic(helper, checkTask, this->getModel());
    return helper.computeLongRunAverageRewards(env, rewardModel.get());
}

// Explicitly instantiate the model checker.
template class HybridMarkovAutomatonCslModelChecker<storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::CUDD, double>>;
template class HybridMarkovAutomatonCslModelChecker<storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::Sylvan, double>>;

template class HybridMarkovAutomatonCslModelChecker<storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::Sylvan, storm::RationalNumber>>;

}  // namespace modelchecker
}  // namespace storm
