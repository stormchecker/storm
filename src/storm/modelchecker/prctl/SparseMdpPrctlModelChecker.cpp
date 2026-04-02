#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/IntervalForward.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/helper/conditional/ConditionalHelper.h"
#include "storm/modelchecker/helper/finitehorizon/SparseNondeterministicStepBoundedHorizonHelper.h"
#include "storm/modelchecker/helper/infinitehorizon/SparseNondeterministicInfiniteHorizonHelper.h"
#include "storm/modelchecker/helper/ltl/SparseLTLHelper.h"
#include "storm/modelchecker/helper/utility/SetInformationFromCheckTask.h"
#include "storm/modelchecker/lexicographic/LexicographicModelChecking.h"
#include "storm/modelchecker/multiobjective/MultiObjectiveModelChecking.h"
#include "storm/modelchecker/prctl/helper/SparseMdpPrctlHelper.h"
#include "storm/modelchecker/prctl/helper/rewardbounded/QuantileHelper.h"
#include "storm/modelchecker/results/ExplicitParetoCurveCheckResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/modelchecker/results/LexicographicCheckResult.h"
#include "storm/solver/SolveGoal.h"
#include "storm/utility/FilteredRewardModel.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
template<typename SparseMdpModelType>
SparseMdpPrctlModelChecker<SparseMdpModelType>::SparseMdpPrctlModelChecker(SparseMdpModelType const& model)
    : SparsePropositionalModelChecker<SparseMdpModelType>(model) {
    // Intentionally left empty.
}

template<typename SparseMdpModelType>
bool SparseMdpPrctlModelChecker<SparseMdpModelType>::canHandleStatic(CheckTask<storm::logic::Formula, SolutionType> const& checkTask,
                                                                     bool* requiresSingleInitialState) {
    storm::logic::Formula const& formula = checkTask.getFormula();
    if constexpr (storm::IsIntervalType<ValueType>) {
        if (formula.isInFragment(storm::logic::propositional())) {
            return true;
        }
        if (formula.isInFragment(storm::logic::reachability())) {
            return true;
        }
        if (formula.isInFragment(storm::logic::prctlstar().setBoundedUntilFormulasAllowed(true))) {
            return true;
        }
    } else {
        if (formula.isInFragment(storm::logic::prctlstar()
                                     .setLongRunAverageRewardFormulasAllowed(true)
                                     .setLongRunAverageProbabilitiesAllowed(true)
                                     .setConditionalProbabilityFormulasAllowed(true)
                                     .setOnlyEventuallyFormuluasInConditionalFormulasAllowed(true)
                                     .setTotalRewardFormulasAllowed(true)
                                     .setRewardBoundedUntilFormulasAllowed(true)
                                     .setRewardBoundedCumulativeRewardFormulasAllowed(true)
                                     .setMultiDimensionalBoundedUntilFormulasAllowed(true)
                                     .setMultiDimensionalCumulativeRewardFormulasAllowed(true)
                                     .setTimeOperatorsAllowed(true)
                                     .setReachbilityTimeFormulasAllowed(true)
                                     .setRewardAccumulationAllowed(true)
                                     .setDiscountedTotalRewardFormulasAllowed(true)
                                     .setDiscountedCumulativeRewardFormulasAllowed(true))) {
            return true;
        } else if (checkTask.isOnlyInitialStatesRelevantSet()) {
            auto multiObjectiveFragment = storm::logic::multiObjective()
                                              .setTimeAllowed(true)
                                              .setCumulativeRewardFormulasAllowed(true)
                                              .setTimeBoundedCumulativeRewardFormulasAllowed(true)
                                              .setStepBoundedCumulativeRewardFormulasAllowed(true)
                                              .setRewardBoundedCumulativeRewardFormulasAllowed(true)
                                              .setTimeBoundedUntilFormulasAllowed(true)
                                              .setStepBoundedUntilFormulasAllowed(true)
                                              .setRewardBoundedUntilFormulasAllowed(true)
                                              .setMultiDimensionalBoundedUntilFormulasAllowed(true)
                                              .setMultiDimensionalCumulativeRewardFormulasAllowed(true)
                                              .setRewardAccumulationAllowed(true);
            auto lexObjectiveFragment = storm::logic::lexObjective()
                                            .setHOAPathFormulasAllowed(true)
                                            .setCumulativeRewardFormulasAllowed(true)
                                            .setTimeBoundedCumulativeRewardFormulasAllowed(true)
                                            .setStepBoundedCumulativeRewardFormulasAllowed(true)
                                            .setRewardBoundedCumulativeRewardFormulasAllowed(true)
                                            .setTimeBoundedUntilFormulasAllowed(true)
                                            .setStepBoundedUntilFormulasAllowed(true)
                                            .setRewardBoundedUntilFormulasAllowed(true)
                                            .setMultiDimensionalBoundedUntilFormulasAllowed(true)
                                            .setMultiDimensionalCumulativeRewardFormulasAllowed(true)
                                            .setRewardAccumulationAllowed(true);

            if (formula.isInFragment(multiObjectiveFragment) || formula.isInFragment(storm::logic::quantiles()) || formula.isInFragment(lexObjectiveFragment)) {
                if (requiresSingleInitialState) {
                    *requiresSingleInitialState = true;
                }
                return true;
            }
        }
    }
    return false;
}

template<typename SparseMdpModelType>
bool SparseMdpPrctlModelChecker<SparseMdpModelType>::canHandle(CheckTask<storm::logic::Formula, SolutionType> const& checkTask) const {
    bool requiresSingleInitialState = false;
    if (canHandleStatic(checkTask, &requiresSingleInitialState)) {
        return !requiresSingleInitialState || this->getModel().getInitialStates().getNumberOfSetBits() == 1;
    } else {
        return false;
    }
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeBoundedUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::BoundedUntilFormula, SolutionType> const& checkTask) {
    storm::logic::BoundedUntilFormula const& pathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    if (pathFormula.isMultiDimensional() || pathFormula.getTimeBoundReference().isRewardBound()) {
        if constexpr (storm::IsIntervalType<ValueType>) {
            STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We have not yet implemented multi dimensional bounded until with intervals");
            return nullptr;
        } else {
            STORM_LOG_THROW(checkTask.isOnlyInitialStatesRelevantSet(), storm::exceptions::InvalidOperationException,
                            "Checking non-trivial bounded until probabilities can only be computed for the initial states of the model.");
            STORM_LOG_WARN_COND(!checkTask.isQualitativeSet(), "Checking non-trivial bounded until formulas is not optimized w.r.t. qualitative queries");
            storm::logic::OperatorInformation opInfo(checkTask.getOptimizationDirection());
            if (checkTask.isBoundSet()) {
                opInfo.bound = checkTask.getBound();
            }
            auto formula = std::make_shared<storm::logic::ProbabilityOperatorFormula>(checkTask.getFormula().asSharedPointer(), opInfo);
            helper::rewardbounded::MultiDimensionalRewardUnfolding<ValueType, true> rewardUnfolding(this->getModel(), formula);
            auto numericResult = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, SolutionType>::computeRewardBoundedValues(
                env, checkTask.getOptimizationDirection(), rewardUnfolding, this->getModel().getInitialStates());
            return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(numericResult)));
        }
    } else {
        STORM_LOG_THROW(pathFormula.hasUpperBound(), storm::exceptions::InvalidPropertyException, "Formula needs to have (a single) upper step bound.");
        STORM_LOG_THROW(pathFormula.hasIntegerLowerBound(), storm::exceptions::InvalidPropertyException, "Formula lower step bound must be discrete/integral.");
        STORM_LOG_THROW(pathFormula.hasIntegerUpperBound(), storm::exceptions::InvalidPropertyException, "Formula needs to have discrete upper time bound.");
        std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
        std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
        ExplicitQualitativeCheckResult<SolutionType> const& leftResult = leftResultPointer->template asExplicitQualitativeCheckResult<SolutionType>();
        ExplicitQualitativeCheckResult<SolutionType> const& rightResult = rightResultPointer->template asExplicitQualitativeCheckResult<SolutionType>();
        storm::modelchecker::helper::SparseNondeterministicStepBoundedHorizonHelper<ValueType, SolutionType> helper;
        std::vector<SolutionType> numericResult =
            helper.compute(env, storm::solver::SolveGoal<ValueType, SolutionType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
                           this->getModel().getBackwardTransitions(), leftResult.getTruthValuesVector(), rightResult.getTruthValuesVector(),
                           pathFormula.getNonStrictLowerBound<uint64_t>(), pathFormula.getNonStrictUpperBound<uint64_t>(), checkTask.getHint());
        return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(numericResult)));
    }
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeNextProbabilities(
    Environment const& env, CheckTask<storm::logic::NextFormula, SolutionType> const& checkTask) {
    storm::logic::NextFormula const& pathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, pathFormula.getSubformula());
    ExplicitQualitativeCheckResult<SolutionType> const& subResult = subResultPointer->template asExplicitQualitativeCheckResult<SolutionType>();
    std::vector<SolutionType> numericResult = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, SolutionType>::computeNextProbabilities(
        env, checkTask.getOptimizationDirection(), checkTask.getUncertaintyResolutionMode(), this->getModel().getTransitionMatrix(),
        subResult.getTruthValuesVector());
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(numericResult)));
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeUntilProbabilities(
    Environment const& env, CheckTask<storm::logic::UntilFormula, SolutionType> const& checkTask) {
    storm::logic::UntilFormula const& pathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    std::unique_ptr<CheckResult> leftResultPointer = this->check(env, pathFormula.getLeftSubformula());
    std::unique_ptr<CheckResult> rightResultPointer = this->check(env, pathFormula.getRightSubformula());
    ExplicitQualitativeCheckResult<SolutionType> const& leftResult = leftResultPointer->template asExplicitQualitativeCheckResult<SolutionType>();
    ExplicitQualitativeCheckResult<SolutionType> const& rightResult = rightResultPointer->template asExplicitQualitativeCheckResult<SolutionType>();
    auto ret = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, SolutionType>::computeUntilProbabilities(
        env, storm::solver::SolveGoal<ValueType, SolutionType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
        this->getModel().getBackwardTransitions(), leftResult.getTruthValuesVector(), rightResult.getTruthValuesVector(), checkTask.isQualitativeSet(),
        checkTask.isProduceSchedulersSet(), checkTask.getHint());
    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(ret.values)));
    if (checkTask.isProduceSchedulersSet() && ret.scheduler) {
        result->asExplicitQuantitativeCheckResult<SolutionType>().setScheduler(std::move(ret.scheduler));
    }
    return result;
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeGloballyProbabilities(
    Environment const& env, CheckTask<storm::logic::GloballyFormula, SolutionType> const& checkTask) {
    storm::logic::GloballyFormula const& pathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, pathFormula.getSubformula());
    ExplicitQualitativeCheckResult<SolutionType> const& subResult = subResultPointer->template asExplicitQualitativeCheckResult<SolutionType>();
    auto ret = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, SolutionType>::computeGloballyProbabilities(
        env, storm::solver::SolveGoal<ValueType, SolutionType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
        this->getModel().getBackwardTransitions(), subResult.getTruthValuesVector(), checkTask.isQualitativeSet(), checkTask.isProduceSchedulersSet());
    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(ret.values)));
    if (checkTask.isProduceSchedulersSet() && ret.scheduler) {
        result->asExplicitQuantitativeCheckResult<SolutionType>().setScheduler(std::move(ret.scheduler));
    }
    return result;
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeHOAPathProbabilities(
    Environment const& env, CheckTask<storm::logic::HOAPathFormula, SolutionType> const& checkTask) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We have not yet implemented automata-props with intervals");
    } else {
        storm::logic::HOAPathFormula const& pathFormula = checkTask.getFormula();

        storm::modelchecker::helper::SparseLTLHelper<ValueType, true> helper(this->getModel().getTransitionMatrix());
        storm::modelchecker::helper::setInformationFromCheckTaskNondeterministic(helper, checkTask, this->getModel());

        auto formulaChecker = [&](storm::logic::Formula const& formula) {
            return this->check(env, formula)->template asExplicitQualitativeCheckResult<SolutionType>().getTruthValuesVector();
        };
        auto apSets = helper.computeApSets(pathFormula.getAPMapping(), formulaChecker);
        std::vector<SolutionType> numericResult = helper.computeDAProductProbabilities(env, *pathFormula.readAutomaton(), apSets);

        std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(numericResult)));
        if (checkTask.isProduceSchedulersSet()) {
            result->asExplicitQuantitativeCheckResult<SolutionType>().setScheduler(
                std::make_unique<storm::storage::Scheduler<SolutionType>>(helper.extractScheduler(this->getModel())));
        }

        return result;
    }
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeLTLProbabilities(
    Environment const& env, CheckTask<storm::logic::PathFormula, SolutionType> const& checkTask) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We have not yet implemented  LTL with intervals");
    } else {
        storm::logic::PathFormula const& pathFormula = checkTask.getFormula();

        STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                        "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");

        storm::modelchecker::helper::SparseLTLHelper<SolutionType, true> helper(this->getModel().getTransitionMatrix());
        storm::modelchecker::helper::setInformationFromCheckTaskNondeterministic(helper, checkTask, this->getModel());

        auto formulaChecker = [&](storm::logic::Formula const& formula) {
            return this->check(env, formula)->template asExplicitQualitativeCheckResult<SolutionType>().getTruthValuesVector();
        };
        std::vector<SolutionType> numericResult = helper.computeLTLProbabilities(env, pathFormula, formulaChecker);

        std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(numericResult)));
        if (checkTask.isProduceSchedulersSet()) {
            result->asExplicitQuantitativeCheckResult<SolutionType>().setScheduler(
                std::make_unique<storm::storage::Scheduler<SolutionType>>(helper.extractScheduler(this->getModel())));
        }

        return result;
    }
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeConditionalProbabilities(
    Environment const& env, CheckTask<storm::logic::ConditionalFormula, SolutionType> const& checkTask) {
    storm::logic::ConditionalFormula const& conditionalFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    STORM_LOG_THROW(this->getModel().getInitialStates().hasUniqueSetBit(), storm::exceptions::InvalidPropertyException,
                    "Cannot compute conditional probabilities on MDPs with more than one initial state.");
    STORM_LOG_THROW(checkTask.isOnlyInitialStatesRelevantSet(), storm::exceptions::InvalidPropertyException,
                    "Conditional probabilities can only be computed for the initial states of the model.");
    STORM_LOG_THROW(conditionalFormula.getSubformula().isEventuallyFormula(), storm::exceptions::InvalidPropertyException,
                    "Illegal conditional probability formula.");
    STORM_LOG_THROW(conditionalFormula.getConditionFormula().isEventuallyFormula(), storm::exceptions::InvalidPropertyException,
                    "Illegal conditional probability formula.");

    std::unique_ptr<CheckResult> leftResultPointer = this->check(env, conditionalFormula.getSubformula().asEventuallyFormula().getSubformula());
    std::unique_ptr<CheckResult> rightResultPointer = this->check(env, conditionalFormula.getConditionFormula().asEventuallyFormula().getSubformula());
    ExplicitQualitativeCheckResult<SolutionType> const& leftResult = leftResultPointer->template asExplicitQualitativeCheckResult<SolutionType>();
    ExplicitQualitativeCheckResult<SolutionType> const& rightResult = rightResultPointer->template asExplicitQualitativeCheckResult<SolutionType>();
    if constexpr (storm::IsIntervalType<ValueType>) {
        throw exceptions::NotImplementedException() << "Conditional Probabilities are not supported with interval models";
    } else {
        return storm::modelchecker::computeConditionalProbabilities(env, storm::solver::SolveGoal<ValueType, SolutionType>(this->getModel(), checkTask),
                                                                    this->getModel().getTransitionMatrix(), this->getModel().getBackwardTransitions(),
                                                                    leftResult.getTruthValuesVector(), rightResult.getTruthValuesVector());
    }
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeCumulativeRewards(
    Environment const& env, CheckTask<storm::logic::CumulativeRewardFormula, SolutionType> const& checkTask) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Cumulative reward properties are not implemented for interval models.");
    }
    storm::logic::CumulativeRewardFormula const& rewardPathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    if (rewardPathFormula.isMultiDimensional() || rewardPathFormula.getTimeBoundReference().isRewardBound()) {
        STORM_LOG_THROW(checkTask.isOnlyInitialStatesRelevantSet(), storm::exceptions::InvalidOperationException,
                        "Checking reward bounded cumulative reward formulas can only be done for the initial states of the model.");
        STORM_LOG_THROW(!checkTask.getFormula().hasRewardAccumulation(), storm::exceptions::InvalidOperationException,
                        "Checking reward bounded cumulative reward formulas is not supported if reward accumulations are given.");
        STORM_LOG_WARN_COND(!checkTask.isQualitativeSet(), "Checking reward bounded until formulas is not optimized w.r.t. qualitative queries");
        storm::logic::OperatorInformation opInfo(checkTask.getOptimizationDirection());
        if (checkTask.isBoundSet()) {
            opInfo.bound = checkTask.getBound();
        }
        auto formula = std::make_shared<storm::logic::RewardOperatorFormula>(checkTask.getFormula().asSharedPointer(), checkTask.getRewardModel(), opInfo);
        helper::rewardbounded::MultiDimensionalRewardUnfolding<ValueType, true> rewardUnfolding(this->getModel(), formula);
        auto numericResult = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, SolutionType>::computeRewardBoundedValues(
            env, checkTask.getOptimizationDirection(), rewardUnfolding, this->getModel().getInitialStates());
        return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(numericResult)));
    } else {
        STORM_LOG_THROW(rewardPathFormula.hasIntegerBound(), storm::exceptions::InvalidPropertyException, "Formula needs to have a discrete time bound.");
        auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
        std::vector<SolutionType> numericResult = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, SolutionType>::computeCumulativeRewards(
            env, storm::solver::SolveGoal<ValueType, SolutionType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(), rewardModel.get(),
            rewardPathFormula.getNonStrictBound<uint64_t>());
        return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(numericResult)));
    }
}

template<>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::Interval>>::computeDiscountedCumulativeRewards(
    Environment const& env, CheckTask<storm::logic::DiscountedCumulativeRewardFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Discounted properties are not implemented for interval models.");
}

template<>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::RationalInterval>>::computeDiscountedCumulativeRewards(
    Environment const& env, CheckTask<storm::logic::DiscountedCumulativeRewardFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Discounted properties are not implemented for interval models.");
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeDiscountedCumulativeRewards(
    Environment const& env, CheckTask<storm::logic::DiscountedCumulativeRewardFormula, SolutionType> const& checkTask) {
    storm::logic::DiscountedCumulativeRewardFormula const& rewardPathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    STORM_LOG_THROW(rewardPathFormula.hasIntegerBound(), storm::exceptions::InvalidPropertyException, "Formula needs to have a discrete time bound.");
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    std::vector<SolutionType> numericResult = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, SolutionType>::computeDiscountedCumulativeRewards(
        env, storm::solver::SolveGoal<ValueType, SolutionType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(), rewardModel.get(),
        rewardPathFormula.getNonStrictBound<uint64_t>(), rewardPathFormula.getDiscountFactor<ValueType>());
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(numericResult)));
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeInstantaneousRewards(
    Environment const& env, CheckTask<storm::logic::InstantaneousRewardFormula, SolutionType> const& checkTask) {
    storm::logic::InstantaneousRewardFormula const& rewardPathFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    STORM_LOG_THROW(rewardPathFormula.hasIntegerBound(), storm::exceptions::InvalidPropertyException, "Formula needs to have a discrete time bound.");
    std::vector<SolutionType> numericResult = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, SolutionType>::computeInstantaneousRewards(
        env, storm::solver::SolveGoal<ValueType, SolutionType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
        checkTask.isRewardModelSet() ? this->getModel().getRewardModel(checkTask.getRewardModel()) : this->getModel().getRewardModel(""),
        rewardPathFormula.getBound<uint64_t>());
    return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(numericResult)));
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeReachabilityRewards(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, SolutionType> const& checkTask) {
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    ExplicitQualitativeCheckResult<SolutionType> const& subResult = subResultPointer->template asExplicitQualitativeCheckResult<SolutionType>();
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    auto ret = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, SolutionType>::computeReachabilityRewards(
        env, storm::solver::SolveGoal<ValueType, SolutionType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
        this->getModel().getBackwardTransitions(), rewardModel.get(), subResult.getTruthValuesVector(), checkTask.isQualitativeSet(),
        checkTask.isProduceSchedulersSet(), checkTask.getHint());
    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(ret.values)));
    if (checkTask.isProduceSchedulersSet() && ret.scheduler) {
        result->asExplicitQuantitativeCheckResult<SolutionType>().setScheduler(std::move(ret.scheduler));
    }
    return result;
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeReachabilityTimes(
    Environment const& env, CheckTask<storm::logic::EventuallyFormula, SolutionType> const& checkTask) {
    storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula();
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    std::unique_ptr<CheckResult> subResultPointer = this->check(env, eventuallyFormula.getSubformula());
    ExplicitQualitativeCheckResult<SolutionType> const& subResult = subResultPointer->template asExplicitQualitativeCheckResult<SolutionType>();
    auto ret = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, SolutionType>::computeReachabilityTimes(
        env, storm::solver::SolveGoal<ValueType, SolutionType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
        this->getModel().getBackwardTransitions(), subResult.getTruthValuesVector(), checkTask.isQualitativeSet(), checkTask.isProduceSchedulersSet(),
        checkTask.getHint());
    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(ret.values)));
    if (checkTask.isProduceSchedulersSet() && ret.scheduler) {
        result->asExplicitQuantitativeCheckResult<SolutionType>().setScheduler(std::move(ret.scheduler));
    }
    return result;
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeTotalRewards(
    Environment const& env, CheckTask<storm::logic::TotalRewardFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    auto ret = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, SolutionType>::computeTotalRewards(
        env, storm::solver::SolveGoal<ValueType, SolutionType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
        this->getModel().getBackwardTransitions(), rewardModel.get(), checkTask.isQualitativeSet(), checkTask.isProduceSchedulersSet(), checkTask.getHint());
    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(ret.values)));
    if (checkTask.isProduceSchedulersSet() && ret.scheduler) {
        result->asExplicitQuantitativeCheckResult<SolutionType>().setScheduler(std::move(ret.scheduler));
    }
    return result;
}

template<>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::Interval>>::computeDiscountedTotalRewards(
    Environment const& env, CheckTask<storm::logic::DiscountedTotalRewardFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Discounted properties are not implemented for interval models.");
}

template<>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::RationalInterval>>::computeDiscountedTotalRewards(
    Environment const& env, CheckTask<storm::logic::DiscountedTotalRewardFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Discounted properties are not implemented for interval models.");
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeDiscountedTotalRewards(
    Environment const& env, CheckTask<storm::logic::DiscountedTotalRewardFormula, SolutionType> const& checkTask) {
    STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    storm::logic::DiscountedTotalRewardFormula const& rewardPathFormula = checkTask.getFormula();
    auto discountFactor = rewardPathFormula.getDiscountFactor<ValueType>();
    auto ret = storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType, SolutionType>::computeDiscountedTotalRewards(
        env, storm::solver::SolveGoal<ValueType, SolutionType>(this->getModel(), checkTask), this->getModel().getTransitionMatrix(),
        this->getModel().getBackwardTransitions(), rewardModel.get(), checkTask.isQualitativeSet(), checkTask.isProduceSchedulersSet(), discountFactor,
        checkTask.getHint());
    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(ret.values)));
    if (checkTask.isProduceSchedulersSet() && ret.scheduler) {
        result->asExplicitQuantitativeCheckResult<SolutionType>().setScheduler(std::move(ret.scheduler));
    }
    return result;
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeLongRunAverageProbabilities(
    Environment const& env, CheckTask<storm::logic::StateFormula, SolutionType> const& checkTask) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We have not yet implemented LRA probabilities with intervals");
    } else {
        storm::logic::StateFormula const& stateFormula = checkTask.getFormula();
        STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                        "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
        std::unique_ptr<CheckResult> subResultPointer = this->check(env, stateFormula);
        ExplicitQualitativeCheckResult<SolutionType> const& subResult = subResultPointer->template asExplicitQualitativeCheckResult<SolutionType>();

        storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType> helper(this->getModel().getTransitionMatrix());
        storm::modelchecker::helper::setInformationFromCheckTaskNondeterministic(helper, checkTask, this->getModel());
        auto values = helper.computeLongRunAverageProbabilities(env, subResult.getTruthValuesVector());

        std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(values)));
        if (checkTask.isProduceSchedulersSet()) {
            result->asExplicitQuantitativeCheckResult<SolutionType>().setScheduler(
                std::make_unique<storm::storage::Scheduler<SolutionType>>(helper.extractScheduler()));
        }
        return result;
    }
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::computeLongRunAverageRewards(
    Environment const& env, CheckTask<storm::logic::LongRunAverageRewardFormula, SolutionType> const& checkTask) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We have not yet implemented lra with intervals");
    } else {
        STORM_LOG_THROW(checkTask.isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                        "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
        auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
        storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType> helper(this->getModel().getTransitionMatrix());
        storm::modelchecker::helper::setInformationFromCheckTaskNondeterministic(helper, checkTask, this->getModel());
        auto values = helper.computeLongRunAverageRewards(env, rewardModel.get());
        std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<SolutionType>(std::move(values)));
        if (checkTask.isProduceSchedulersSet()) {
            result->asExplicitQuantitativeCheckResult<SolutionType>().setScheduler(
                std::make_unique<storm::storage::Scheduler<SolutionType>>(helper.extractScheduler()));
        }
        return result;
    }
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::checkMultiObjectiveFormula(
    Environment const& env, CheckTask<storm::logic::MultiObjectiveFormula, SolutionType> const& checkTask) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We have not yet implemented multi-objective with intervals");
    } else {
        return multiobjective::performMultiObjectiveModelChecking(env, this->getModel(), checkTask.getFormula(), checkTask.isProduceSchedulersSet());
    }
}

template<class SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::checkLexObjectiveFormula(
    const Environment& env, const CheckTask<storm::logic::MultiObjectiveFormula, SolutionType>& checkTask) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We have not yet implemented lexicographic model checking with intervals");
    } else {
        auto formulaChecker = [&](storm::logic::Formula const& formula) {
            return this->check(env, formula)->template asExplicitQualitativeCheckResult<SolutionType>().getTruthValuesVector();
        };
        auto ret = lexicographic::check(env, this->getModel(), checkTask, formulaChecker);
        std::unique_ptr<CheckResult> result(new LexicographicCheckResult<SolutionType>(ret.values, *this->getModel().getInitialStates().begin()));
        return result;
    }
}

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> SparseMdpPrctlModelChecker<SparseMdpModelType>::checkQuantileFormula(
    Environment const& env, CheckTask<storm::logic::QuantileFormula, SolutionType> const& checkTask) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "We have not yet implemented quantile formulas with intervals");
    } else {
        STORM_LOG_THROW(checkTask.isOnlyInitialStatesRelevantSet(), storm::exceptions::InvalidOperationException,
                        "Computing quantiles is only supported for the initial states of a model.");
        STORM_LOG_THROW(this->getModel().getInitialStates().getNumberOfSetBits() == 1, storm::exceptions::InvalidOperationException,
                        "Quantiles not supported on models with multiple initial states.");
        uint64_t initialState = *this->getModel().getInitialStates().begin();

        helper::rewardbounded::QuantileHelper<SparseMdpModelType> qHelper(this->getModel(), checkTask.getFormula());
        auto res = qHelper.computeQuantile(env);

        if (res.size() == 1 && res.front().size() == 1) {
            return std::unique_ptr<CheckResult>(new ExplicitQuantitativeCheckResult<SolutionType>(initialState, std::move(res.front().front())));
        } else {
            return std::unique_ptr<CheckResult>(new ExplicitParetoCurveCheckResult<SolutionType>(initialState, std::move(res)));
        }
    }
}

template class SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>>;
template class SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::Interval>>;
template class SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<storm::RationalInterval>>;
}  // namespace modelchecker
}  // namespace storm
