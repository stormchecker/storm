#include "storm/modelchecker/csl/SparseMarkovAutomatonCslModelChecker.h"

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/csl/helper/SparseMarkovAutomatonCslHelper.h"
#include "storm/modelchecker/helper/infinitehorizon/SparseNondeterministicInfiniteHorizonHelper.h"
#include "storm/modelchecker/helper/ltl/SparseLTLHelper.h"
#include "storm/modelchecker/helper/utility/SetInformationFromCheckTask.h"
#include "storm/modelchecker/multiobjective/MultiObjectiveModelChecking.h"
#include "storm/modelchecker/prctl/helper/SparseMdpPrctlHelper.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/solver/SolveGoal.h"
#include "storm/utility/FilteredRewardModel.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
template<typename SparseMarkovAutomatonModelType>
SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::SparseMarkovAutomatonCslModelChecker(SparseMarkovAutomatonModelType const& model)
    : SparsePropositionalModelChecker<SparseMarkovAutomatonModelType>(model) {
    // Intentionally left empty.
}

template<typename ModelType>
bool SparseMarkovAutomatonCslModelChecker<ModelType>::canHandleStatic(CheckTask<storm::logic::Formula, ValueType> const& checkTask,
                                                                      bool* requiresSingleInitialState) {
    auto singleObjectiveFragment = storm::logic::csrlstar()
                                       .setRewardOperatorsAllowed(true)
                                       .setReachabilityRewardFormulasAllowed(true)
                                       .setTotalRewardFormulasAllowed(true)
                                       .setTimeAllowed(true)
                                       .setLongRunAverageProbabilitiesAllowed(true)
                                       .setLongRunAverageRewardFormulasAllowed(true)
                                       .setRewardAccumulationAllowed(true)
                                       .setInstantaneousFormulasAllowed(false);
    auto multiObjectiveFragment =
        storm::logic::multiObjective().setTimeAllowed(true).setTimeBoundedUntilFormulasAllowed(true).setRewardAccumulationAllowed(true);
    if (!storm::NumberTraits<ValueType>::SupportsExponential) {
        singleObjectiveFragment.setBoundedUntilFormulasAllowed(false).setCumulativeRewardFormulasAllowed(false);
        multiObjectiveFragment.setTimeBoundedUntilFormulasAllowed(false).setCumulativeRewardFormulasAllowed(false);
    }
    if (checkTask.getFormula().isInFragment(singleObjectiveFragment)) {
        return true;
    } else if (checkTask.isOnlyInitialStatesRelevantSet() && checkTask.getFormula().isInFragment(multiObjectiveFragment)) {
        if (requiresSingleInitialState) {
            *requiresSingleInitialState = true;
        }
        return true;
    }
    return false;
}

template<typename SparseMarkovAutomatonModelType>
bool SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::canHandle(CheckTask<storm::logic::Formula, ValueType> const& checkTask) const {
    bool requiresSingleInitialState = false;
    if (canHandleStatic(checkTask, &requiresSingleInitialState)) {
        return !requiresSingleInitialState || this->getModel().getInitialStates().getNumberOfSetBits() == 1;
    } else {
        return false;
    }
}

template<typename SparseMarkovAutomatonModelType>
std::unique_ptr<CheckResult> SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::computeBoundedUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::BoundedUntilFormula, ValueType> const& checkTask) {
    storm::logic::BoundedUntilFormula const& pathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    STORM_LOG_THROW(this->getModel().isClosed(), storm::exceptions::InvalidPropertyException,
                    "Unable to compute time-bounded reachability probabilities in non-closed Markov automaton.");
    std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
    ExplicitQualitativeCheckResult<ValueType> const& rightResult = rightResultPointer->template asExplicitQualitativeCheckResult<ValueType>();

    std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
    ExplicitQualitativeCheckResult<ValueType> const& leftResult = leftResultPointer->template asExplicitQualitativeCheckResult<ValueType>();

    STORM_LOG_THROW(pathFormula.getTimeBoundReference().isTimeBound(), storm::exceptions::NotImplementedException,
                    "Currently step-bounded and reward-bounded properties on MAs are not supported.");
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

    std::vector<ValueType> result = storm::modelchecker::helper::SparseMarkovAutomatonCslHelper::computeBoundedUntilProbabilities(
        env, storm::solver::SolveGoal<ValueType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(), this->getModel().getExitRates(),
        this->getModel().getMarkovianStates(), leftResult.getTruthValuesVector(), rightResult.getTruthValuesVector(), std::make_pair(lowerBound, upperBound));
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(result)));
}

template<typename SparseMarkovAutomatonModelType>
std::unique_ptr<CheckResult> SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::computeNextProbabilities(
    Environment const& env, CheckTask<storm::logic::NextFormula, ValueType> const& checkTask) {
    storm::logic::NextFormula const& pathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, pathFormula.getSubformula());
    ExplicitQualitativeCheckResult<ValueType> const& subResult = subResultPointer->template asExplicitQualitativeCheckResult<ValueType>();
    std::vector<ValueType> numericResult = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType>::computeNextProbabilities(
        env, checkTask.getOptimizationDirection(), checkTask.getUncertaintyResolutionMode(), this->getModel().getTransitionMatrix(),
        subResult.getTruthValuesVector());
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
}

template<typename SparseMarkovAutomatonModelType>
std::unique_ptr<CheckResult> SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::computeGloballyProbabilities(
    Environment const& env, CheckTask<storm::logic::GloballyFormula, ValueType> const& checkTask) {
    storm::logic::GloballyFormula const& pathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, pathFormula.getSubformula());
    ExplicitQualitativeCheckResult<ValueType> const& subResult = subResultPointer->template asExplicitQualitativeCheckResult<ValueType>();
    auto ret = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType>::computeGloballyProbabilities(
        env, storm::solver::SolveGoal<ValueType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
        this->getModel().getBackwardTransitions(), subResult.getTruthValuesVector(), checkTask.isQualitativeSet(), checkTask.isProduceSchedulersSet());
    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<ValueType>(std::move(ret.values)));
    if (checkTask.isProduceSchedulersSet() && ret.scheduler) {
        result->asExplicitQuantitativeCheckResult<ValueType>().setScheduler(std::move(ret.scheduler));
    }
    return result;
}

template<typename SparseMarkovAutomatonModelType>
std::unique_ptr<CheckResult> SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::computeUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) {
    storm::logic::UntilFormula const& pathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
    std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
    ExplicitQualitativeCheckResult<ValueType>& leftResult = leftResultPointer->template asExplicitQualitativeCheckResult<ValueType>();
    ExplicitQualitativeCheckResult<ValueType>& rightResult = rightResultPointer->template asExplicitQualitativeCheckResult<ValueType>();

    auto ret = storm::modelchecker::helper::SparseMarkovAutomatonCslHelper::computeUntilProbabilities(
        env, checkTask.getOptimizationDirection(), this->getModel().getTransitionMatrix(), this->getModel().getBackwardTransitions(),
        leftResult.getTruthValuesVector(), rightResult.getTruthValuesVector(), checkTask.isQualitativeSet(), checkTask.isProduceSchedulersSet());
    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<ValueType>(std::move(ret.values)));
    if (checkTask.isProduceSchedulersSet() && ret.scheduler) {
        result->asExplicitQuantitativeCheckResult<ValueType>().setScheduler(std::move(ret.scheduler));
    }
    return result;
}

template<typename SparseMarkovAutomatonModelType>
std::unique_ptr<CheckResult> SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::computeHOAPathProbabilities(
    Environment const& env, CheckTask<storm::logic::HOAPathFormula, ValueType> const& checkTask) {
    storm::logic::HOAPathFormula const& pathFormula = checkTask.getFormula();

    storm::modelchecker::helper::SparseLTLHelper<ValueType, true> helper(this->getModel().getTransitionMatrix());
    storm::modelchecker::helper::setInformationFromCheckTaskNondeterministic(helper, checkTask, this->getModel());

    auto formulaChecker = [&](storm::logic::Formula const& formula) {
        return this->check(env, formula)->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
    };
    auto apSets = helper.computeApSets(pathFormula.getAPMapping(), formulaChecker);
    std::vector<ValueType> numericResult = helper.computeDAProductProbabilities(env, *pathFormula.readAutomaton(), apSets);

    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
    if (checkTask.isProduceSchedulersSet()) {
        result->asExplicitQuantitativeCheckResult<ValueType>().setScheduler(
            std::make_unique<storm::storage::Scheduler<ValueType>>(helper.extractScheduler(this->getModel())));
    }

    return result;
}

template<typename SparseMarkovAutomatonModelType>
std::unique_ptr<CheckResult> SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::computeLTLProbabilities(
    Environment const& env, CheckTask<storm::logic::PathFormula, ValueType> const& checkTask) {
    storm::logic::PathFormula const& pathFormula = checkTask.getFormula();

    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");

    storm::modelchecker::helper::SparseLTLHelper<ValueType, true> helper(this->getModel().getTransitionMatrix());
    storm::modelchecker::helper::setInformationFromCheckTaskNondeterministic(helper, checkTask, this->getModel());

    auto formulaChecker = [&](storm::logic::Formula const& formula) {
        return this->check(env, formula)->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
    };
    std::vector<ValueType> numericResult = helper.computeLTLProbabilities(env, pathFormula, formulaChecker);

    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<ValueType>(std::move(numericResult)));
    if (checkTask.isProduceSchedulersSet()) {
        result->asExplicitQuantitativeCheckResult<ValueType>().setScheduler(
            std::make_unique<storm::storage::Scheduler<ValueType>>(helper.extractScheduler(this->getModel())));
    }

    return result;
}

template<typename SparseMarkovAutomatonModelType>
std::unique_ptr<CheckResult> SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::computeReachabilityRewards(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    STORM_LOG_THROW(this->getModel().isClosed(), storm::exceptions::InvalidPropertyException,
                    "Unable to compute reachability rewards in non-closed Markov automaton.");
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    ExplicitQualitativeCheckResult<ValueType> const& subResult = subResultPointer->template asExplicitQualitativeCheckResult<ValueType>();
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);

    auto ret = storm::modelchecker::helper::SparseMarkovAutomatonCslHelper::computeReachabilityRewards(
        env, checkTask.getOptimizationDirection(), this->getModel().getTransitionMatrix(), this->getModel().getBackwardTransitions(),
        this->getModel().getExitRates(), this->getModel().getMarkovianStates(), rewardModel.get(), subResult.getTruthValuesVector(),
        checkTask.isProduceSchedulersSet());
    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<ValueType>(std::move(ret.values)));
    if (checkTask.isProduceSchedulersSet() && ret.scheduler) {
        result->asExplicitQuantitativeCheckResult<ValueType>().setScheduler(std::move(ret.scheduler));
    }
    return result;
}

template<typename SparseMarkovAutomatonModelType>
std::unique_ptr<CheckResult> SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::computeTotalRewards(
    Environment const& env, CheckTask<storm::logic::TotalRewardFormula, ValueType> const& checkTask) {
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    STORM_LOG_THROW(this->getModel().isClosed(), storm::exceptions::InvalidPropertyException,
                    "Unable to compute reachability rewards in non-closed Markov automaton.");
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);

    auto ret = storm::modelchecker::helper::SparseMarkovAutomatonCslHelper::computeTotalRewards(
        env, checkTask.getOptimizationDirection(), this->getModel().getTransitionMatrix(), this->getModel().getBackwardTransitions(),
        this->getModel().getExitRates(), this->getModel().getMarkovianStates(), rewardModel.get(), checkTask.isProduceSchedulersSet());
    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<ValueType>(std::move(ret.values)));
    if (checkTask.isProduceSchedulersSet() && ret.scheduler) {
        result->asExplicitQuantitativeCheckResult<ValueType>().setScheduler(std::move(ret.scheduler));
    }
    return result;
}

template<typename SparseMarkovAutomatonModelType>
std::unique_ptr<CheckResult> SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::computeLongRunAverageProbabilities(
    Environment const& env, CheckTask<storm::logic::StateFormula, ValueType> const& checkTask) {
    storm::logic::StateFormula const& stateFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    STORM_LOG_THROW(this->getModel().isClosed(), storm::exceptions::InvalidPropertyException,
                    "Unable to compute long-run average in non-closed Markov automaton.");
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, stateFormula);
    ExplicitQualitativeCheckResult<ValueType> const& subResult = subResultPointer->template asExplicitQualitativeCheckResult<ValueType>();

    storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType> helper(
        this->getModel().getTransitionMatrix(), this->getModel().getMarkovianStates(), this->getModel().getExitRates());
    storm::modelchecker::helper::setInformationFromCheckTaskNondeterministic(helper, checkTask, this->getModel());
    auto values = helper.computeLongRunAverageProbabilities(env, subResult.getTruthValuesVector());

    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<ValueType>(std::move(values)));
    if (checkTask.isProduceSchedulersSet()) {
        result->asExplicitQuantitativeCheckResult<ValueType>().setScheduler(std::make_unique<storm::storage::Scheduler<ValueType>>(helper.extractScheduler()));
    }
    return result;
}

template<typename SparseMarkovAutomatonModelType>
std::unique_ptr<CheckResult> SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::computeLongRunAverageRewards(
    Environment const& env, CheckTask<storm::logic::LongRunAverageRewardFormula, ValueType> const& checkTask) {
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    STORM_LOG_THROW(this->getModel().isClosed(), storm::exceptions::InvalidPropertyException,
                    "Unable to compute long run average rewards in non-closed Markov automaton.");
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);

    storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType> helper(
        this->getModel().getTransitionMatrix(), this->getModel().getMarkovianStates(), this->getModel().getExitRates());
    storm::modelchecker::helper::setInformationFromCheckTaskNondeterministic(helper, checkTask, this->getModel());
    auto values = helper.computeLongRunAverageRewards(env, rewardModel.get());

    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<ValueType>(std::move(values)));
    if (checkTask.isProduceSchedulersSet()) {
        result->asExplicitQuantitativeCheckResult<ValueType>().setScheduler(std::make_unique<storm::storage::Scheduler<ValueType>>(helper.extractScheduler()));
    }
    return result;
}

template<typename SparseMarkovAutomatonModelType>
std::unique_ptr<CheckResult> SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::computeReachabilityTimes(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    STORM_LOG_THROW(this->getModel().isClosed(), storm::exceptions::InvalidPropertyException,
                    "Unable to compute expected times in non-closed Markov automaton.");
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    ExplicitQualitativeCheckResult<ValueType>& subResult = subResultPointer->template asExplicitQualitativeCheckResult<ValueType>();

    auto ret = storm::modelchecker::helper::SparseMarkovAutomatonCslHelper::computeReachabilityTimes(
        env, checkTask.getOptimizationDirection(), this->getModel().getTransitionMatrix(), this->getModel().getBackwardTransitions(),
        this->getModel().getExitRates(), this->getModel().getMarkovianStates(), subResult.getTruthValuesVector(), checkTask.isProduceSchedulersSet());
    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<ValueType>(std::move(ret.values)));
    if (checkTask.isProduceSchedulersSet() && ret.scheduler) {
        result->asExplicitQuantitativeCheckResult<ValueType>().setScheduler(std::move(ret.scheduler));
    }
    return result;
}

template<typename SparseMarkovAutomatonModelType>
std::unique_ptr<CheckResult> SparseMarkovAutomatonCslModelChecker<SparseMarkovAutomatonModelType>::checkMultiObjectiveFormula(
    Environment const& env, CheckTask<storm::logic::MultiObjectiveFormula, ValueType> const& checkTask) {
    return multiobjective::performMultiObjectiveModelChecking(env, this->getModel(), checkTask.getFormula(), checkTask.isProduceSchedulersSet());
}

template class SparseMarkovAutomatonCslModelChecker<storm::models::sparse::MarkovAutomaton<double>>;
template class SparseMarkovAutomatonCslModelChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;
}  // namespace modelchecker
}  // namespace storm
