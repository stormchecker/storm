#include "storm/modelchecker/csl/SparseCtmcCslModelChecker.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/csl/helper/SparseCtmcCslHelper.h"
#include "storm/modelchecker/helper/indefinitehorizon/visitingtimes/SparseDeterministicVisitingTimesHelper.h"
#include "storm/modelchecker/helper/infinitehorizon/SparseDeterministicInfiniteHorizonHelper.h"
#include "storm/modelchecker/helper/ltl/SparseLTLHelper.h"
#include "storm/modelchecker/helper/utility/SetInformationFromCheckTask.h"
#include "storm/modelchecker/prctl/helper/SparseDtmcPrctlHelper.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/utility/FilteredRewardModel.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
template<typename SparseCtmcModelType>
SparseCtmcCslModelChecker<SparseCtmcModelType>::SparseCtmcCslModelChecker(SparseCtmcModelType const& model)
    : SparsePropositionalModelChecker<SparseCtmcModelType>(model) {
    // Intentionally left empty.
}

template<typename ModelType>
bool SparseCtmcCslModelChecker<ModelType>::canHandleStatic(CheckTask<storm::logic::Formula, ValueType> const& checkTask) {
    auto fragment = storm::logic::csrlstar()
                        .setLongRunAverageRewardFormulasAllowed(true)
                        .setLongRunAverageProbabilitiesAllowed(true)
                        .setTimeAllowed(true)
                        .setTimeOperatorsAllowed(true)
                        .setTotalRewardFormulasAllowed(true)
                        .setRewardAccumulationAllowed(true);
    if (!storm::NumberTraits<ValueType>::SupportsExponential) {
        fragment.setBoundedUntilFormulasAllowed(false).setCumulativeRewardFormulasAllowed(false).setInstantaneousFormulasAllowed(false);
    }
    return checkTask.getFormula().isInFragment(fragment);
}

template<typename SparseCtmcModelType>
bool SparseCtmcCslModelChecker<SparseCtmcModelType>::canHandle(CheckTask<storm::logic::Formula, ValueType> const& checkTask) const {
    return canHandleStatic(checkTask);
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeBoundedUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::BoundedUntilFormula, ValueType> const& checkTask) {
    if constexpr (!storm::NumberTraits<ValueType>::SupportsExponential) {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing bounded until probabilities is not supported for this numeric type.");
        return nullptr;
    } else {
        storm::logic::BoundedUntilFormula const& pathFormula = checkTask.getFormula();
        std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
        std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
        ExplicitQualitativeCheckResult<ValueType> const& leftResult = leftResultPointer->template asExplicitQualitativeCheckResult<ValueType>();
        ExplicitQualitativeCheckResult<ValueType> const& rightResult = rightResultPointer->template asExplicitQualitativeCheckResult<ValueType>();

        STORM_LOG_THROW(pathFormula.getTimeBoundReference().isTimeBound(), storm::exceptions::NotImplementedException,
                        "Currently step-bounded or reward-bounded properties on CTMCs are not supported.");
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

        std::vector<ValueType> numericResult = storm::modelchecker::helper::SparseCtmcCslHelper::computeBoundedUntilProbabilities(
            env, storm::solver::SolveGoal<ValueType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
            this->getModel().getBackwardTransitions(), leftResult.getTruthValuesVector(), rightResult.getTruthValuesVector(),
            this->getModel().getExitRateVector(), checkTask.isQualitativeSet(), lowerBound, upperBound);
        return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
    }
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeNextProbabilities(
    Environment const& env, CheckTask<storm::logic::NextFormula, ValueType> const& checkTask) {
    storm::logic::NextFormula const& pathFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, pathFormula.getSubformula());
    ExplicitQualitativeCheckResult<ValueType> const& subResult = subResultPointer->template asExplicitQualitativeCheckResult<ValueType>();
    std::vector<ValueType> numericResult = storm::modelchecker::helper::SparseCtmcCslHelper::computeNextProbabilities(
        env, this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector(), subResult.getTruthValuesVector());
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeGloballyProbabilities(
    Environment const& env, CheckTask<storm::logic::GloballyFormula, ValueType> const& checkTask) {
    storm::logic::GloballyFormula const& pathFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, pathFormula.getSubformula());
    ExplicitQualitativeCheckResult<ValueType> const& subResult = subResultPointer->template asExplicitQualitativeCheckResult<ValueType>();
    auto probabilisticTransitions = this->getModel().computeProbabilityMatrix();
    std::vector<ValueType> numericResult = storm::modelchecker::helper::SparseDtmcPrctlHelper<ValueType>::computeGloballyProbabilities(
        env, storm::solver::SolveGoal<ValueType>(this->getModel(), checkTask), probabilisticTransitions, probabilisticTransitions.transpose(),
        subResult.getTruthValuesVector(), checkTask.isQualitativeSet());
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) {
    storm::logic::UntilFormula const& pathFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
    std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
    ExplicitQualitativeCheckResult<ValueType> const& leftResult = leftResultPointer->template asExplicitQualitativeCheckResult<ValueType>();
    ExplicitQualitativeCheckResult<ValueType> const& rightResult = rightResultPointer->template asExplicitQualitativeCheckResult<ValueType>();
    std::vector<ValueType> numericResult = storm::modelchecker::helper::SparseCtmcCslHelper::computeUntilProbabilities(
        env, storm::solver::SolveGoal<ValueType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
        this->getModel().getBackwardTransitions(), this->getModel().getExitRateVector(), leftResult.getTruthValuesVector(), rightResult.getTruthValuesVector(),
        checkTask.isQualitativeSet());
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeHOAPathProbabilities(
    Environment const& env, CheckTask<storm::logic::HOAPathFormula, ValueType> const& checkTask) {
    storm::logic::HOAPathFormula const& pathFormula = checkTask.getFormula();

    auto probabilisticTransitions = this->getModel().computeProbabilityMatrix();
    storm::modelchecker::helper::SparseLTLHelper<ValueType, false> helper(probabilisticTransitions);
    storm::modelchecker::helper::setInformationFromCheckTaskDeterministic(helper, checkTask, this->getModel());

    auto formulaChecker = [&](storm::logic::Formula const& formula) {
        return this->check(env, formula)->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
    };
    auto apSets = helper.computeApSets(pathFormula.getAPMapping(), formulaChecker);
    std::vector<ValueType> numericResult = helper.computeDAProductProbabilities(env, *pathFormula.readAutomaton(), apSets);

    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeLTLProbabilities(
    Environment const& env, CheckTask<storm::logic::PathFormula, ValueType> const& checkTask) {
    storm::logic::PathFormula const& pathFormula = checkTask.getFormula();

    auto probabilisticTransitions = this->getModel().computeProbabilityMatrix();
    storm::modelchecker::helper::SparseLTLHelper<ValueType, false> helper(probabilisticTransitions);
    storm::modelchecker::helper::setInformationFromCheckTaskDeterministic(helper, checkTask, this->getModel());

    auto formulaChecker = [&](storm::logic::Formula const& formula) {
        return this->check(env, formula)->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
    };
    std::vector<ValueType> numericResult = helper.computeLTLProbabilities(env, pathFormula, formulaChecker);

    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeInstantaneousRewards(
    Environment const& env, CheckTask<storm::logic::InstantaneousRewardFormula, ValueType> const& checkTask) {
    if constexpr (!storm::NumberTraits<ValueType>::SupportsExponential) {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing instantaneous rewards is not supported for this numeric type.");
        return nullptr;
    } else {
        storm::logic::InstantaneousRewardFormula const& rewardPathFormula = checkTask.getFormula();
        STORM_LOG_THROW(!rewardPathFormula.isStepBounded(), storm::exceptions::NotImplementedException,
                        "Currently step-bounded properties on CTMCs are not supported.");
        std::vector<ValueType> numericResult = storm::modelchecker::helper::SparseCtmcCslHelper::computeInstantaneousRewards(
            env, storm::solver::SolveGoal<ValueType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector(),
            checkTask.isRewardModelSet() ? this->getModel().getRewardModel(checkTask.getRewardModel()) : this->getModel().getRewardModel(""),
            rewardPathFormula.getBound<ValueType>());
        return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
    }
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeCumulativeRewards(
    Environment const& env, CheckTask<storm::logic::CumulativeRewardFormula, ValueType> const& checkTask) {
    if constexpr (!storm::NumberTraits<ValueType>::SupportsExponential) {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing cumulative rewards is not supported for this numeric type.");
        return nullptr;
    } else {
        storm::logic::CumulativeRewardFormula const& rewardPathFormula = checkTask.getFormula();
        STORM_LOG_THROW(rewardPathFormula.getTimeBoundReference().isTimeBound(), storm::exceptions::NotImplementedException,
                        "Currently step-bounded and reward-bounded properties on CTMCs are not supported.");
        auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
        std::vector<ValueType> numericResult = storm::modelchecker::helper::SparseCtmcCslHelper::computeCumulativeRewards(
            env, storm::solver::SolveGoal<ValueType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(), this->getModel().getExitRateVector(),
            rewardModel.get(), rewardPathFormula.getNonStrictBound<ValueType>());
        return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
    }
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeReachabilityRewards(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    ExplicitQualitativeCheckResult<ValueType> const& subResult = subResultPointer->template asExplicitQualitativeCheckResult<ValueType>();
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    std::vector<ValueType> numericResult = storm::modelchecker::helper::SparseCtmcCslHelper::computeReachabilityRewards(
        env, storm::solver::SolveGoal<ValueType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
        this->getModel().getBackwardTransitions(), this->getModel().getExitRateVector(), rewardModel.get(), subResult.getTruthValuesVector(),
        checkTask.isQualitativeSet());
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeTotalRewards(
    Environment const& env, CheckTask<storm::logic::TotalRewardFormula, ValueType> const& checkTask) {
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    std::vector<ValueType> numericResult = storm::modelchecker::helper::SparseCtmcCslHelper::computeTotalRewards(
        env, storm::solver::SolveGoal<ValueType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
        this->getModel().getBackwardTransitions(), this->getModel().getExitRateVector(), rewardModel.get(), checkTask.isQualitativeSet());
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeLongRunAverageProbabilities(
    Environment const& env, CheckTask<storm::logic::StateFormula, ValueType> const& checkTask) {
    storm::logic::StateFormula const& stateFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, stateFormula);
    ExplicitQualitativeCheckResult<ValueType> const& subResult = subResultPointer->template asExplicitQualitativeCheckResult<ValueType>();

    auto probabilisticTransitions = this->getModel().computeProbabilityMatrix();
    storm::modelchecker::helper::SparseDeterministicInfiniteHorizonHelper<ValueType> helper(probabilisticTransitions, this->getModel().getExitRateVector());
    storm::modelchecker::helper::setInformationFromCheckTaskDeterministic(helper, checkTask, this->getModel());
    auto values = helper.computeLongRunAverageProbabilities(env, subResult.getTruthValuesVector());

    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(values)));
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeLongRunAverageRewards(
    Environment const& env, CheckTask<storm::logic::LongRunAverageRewardFormula, ValueType> const& checkTask) {
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    auto probabilisticTransitions = this->getModel().computeProbabilityMatrix();
    storm::modelchecker::helper::SparseDeterministicInfiniteHorizonHelper<ValueType> helper(probabilisticTransitions, this->getModel().getExitRateVector());
    storm::modelchecker::helper::setInformationFromCheckTaskDeterministic(helper, checkTask, this->getModel());
    auto values = helper.computeLongRunAverageRewards(env, rewardModel.get());
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(values)));
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeReachabilityTimes(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    ExplicitQualitativeCheckResult<ValueType>& subResult = subResultPointer->template asExplicitQualitativeCheckResult<ValueType>();

    std::vector<ValueType> numericResult = storm::modelchecker::helper::SparseCtmcCslHelper::computeReachabilityTimes(
        env, storm::solver::SolveGoal<ValueType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
        this->getModel().getBackwardTransitions(), this->getModel().getExitRateVector(), subResult.getTruthValuesVector(), checkTask.isQualitativeSet());
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
}

template<typename SparseCtmcModelType>
std::vector<typename SparseCtmcModelType::ValueType> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeAllTransientProbabilities(
    Environment const& env, CheckTask<storm::logic::BoundedUntilFormula, ValueType> const& checkTask) {
    if constexpr (!storm::NumberTraits<ValueType>::SupportsExponential) {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing transient probabilities is not supported for this numeric type.");
        return {};
    } else {
        storm::logic::BoundedUntilFormula const& pathFormula = checkTask.getFormula();
        STORM_LOG_THROW(pathFormula.getTimeBoundReference().isTimeBound(), storm::exceptions::NotImplementedException,
                        "Currently step-bounded or reward-bounded properties on CTMCs are not supported.");
        STORM_LOG_THROW(pathFormula.hasUpperBound(), storm::exceptions::NotImplementedException, "Computation needs upper limit for time bound.");
        ValueType upperBound = pathFormula.getNonStrictUpperBound<ValueType>();

        std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
        std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
        ExplicitQualitativeCheckResult<ValueType> const& leftResult = leftResultPointer->template asExplicitQualitativeCheckResult<ValueType>();
        ExplicitQualitativeCheckResult<ValueType> const& rightResult = rightResultPointer->template asExplicitQualitativeCheckResult<ValueType>();

        std::vector<ValueType> result = storm::modelchecker::helper::SparseCtmcCslHelper::computeAllTransientProbabilities(
            env, this->getModel().getTransitionMatrix(), this->getModel().getInitialStates(), leftResult.getTruthValuesVector(),
            rightResult.getTruthValuesVector(), this->getModel().getExitRateVector(), upperBound);
        return result;
    }
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeSteadyStateDistribution(Environment const& env) {
    // Initialize helper
    auto probabilisticTransitions = this->getModel().computeProbabilityMatrix();
    storm::modelchecker::helper::SparseDeterministicInfiniteHorizonHelper<ValueType> helper(probabilisticTransitions, this->getModel().getExitRateVector());

    // Compute result
    std::vector<ValueType> result;
    auto const& initialStates = this->getModel().getInitialStates();
    uint64_t numInitStates = initialStates.getNumberOfSetBits();
    if (numInitStates == 1) {
        result = helper.computeLongRunAverageStateDistribution(env, *initialStates.begin());
    } else {
        STORM_LOG_WARN("Multiple initial states found. A uniform distribution over initial states is assumed.");
        ValueType initProb = storm::utility::one<ValueType>() / storm::utility::convertNumber<ValueType, uint64_t>(numInitStates);
        result = helper.computeLongRunAverageStateDistribution(env, [&initialStates, &initProb](uint64_t const& stateIndex) {
            return initialStates.get(stateIndex) ? initProb : storm::utility::zero<ValueType>();
        });
    }

    // Return CheckResult
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(result)));
}

template<typename SparseCtmcModelType>
std::unique_ptr<CheckResult> SparseCtmcCslModelChecker<SparseCtmcModelType>::computeExpectedVisitingTimes(Environment const& env) {
    // Initialize helper
    auto probabilisticTransitions = this->getModel().computeProbabilityMatrix();
    storm::modelchecker::helper::SparseDeterministicVisitingTimesHelper<ValueType> helper(probabilisticTransitions, this->getModel().getExitRateVector());

    // Compute result
    std::vector<ValueType> result;
    auto const& initialStates = this->getModel().getInitialStates();
    uint64_t numInitStates = initialStates.getNumberOfSetBits();
    STORM_LOG_THROW(numInitStates > 0, storm::exceptions::InvalidOperationException, "No initial states given. Cannot compute expected visiting times.");
    STORM_LOG_WARN_COND(numInitStates == 1, "Multiple initial states found. A uniform distribution over initial states is assumed.");
    result = helper.computeExpectedVisitingTimes(env, initialStates);

    // Return CheckResult
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(result)));
}

// Explicitly instantiate the model checker.
template class SparseCtmcCslModelChecker<storm::models::sparse::Ctmc<double>>;

template class SparseCtmcCslModelChecker<storm::models::sparse::Ctmc<storm::RationalNumber>>;
template class SparseCtmcCslModelChecker<storm::models::sparse::Ctmc<storm::RationalFunction>>;

}  // namespace modelchecker
}  // namespace storm
