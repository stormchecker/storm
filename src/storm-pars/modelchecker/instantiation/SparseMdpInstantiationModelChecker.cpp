#include "storm-pars/modelchecker/instantiation/SparseMdpInstantiationModelChecker.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/hints/ExplicitModelCheckerHint.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/storage/Scheduler.h"
#include "storm/utility/graph.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {

template<typename SparseModelType, typename ConstantType>
SparseMdpInstantiationModelChecker<SparseModelType, ConstantType>::SparseMdpInstantiationModelChecker(SparseModelType const& parametricModel,
                                                                                                      bool produceScheduler)
    : SparseInstantiationModelChecker<SparseModelType, ConstantType>(parametricModel), modelInstantiator(parametricModel), produceScheduler(produceScheduler) {
    // Intentionally left empty
}

template<typename SparseModelType, typename ConstantType>
std::unique_ptr<CheckResult> SparseMdpInstantiationModelChecker<SparseModelType, ConstantType>::check(
    Environment const& env, storm::utility::parametric::Valuation<typename SparseModelType::ValueType> const& valuation) {
    STORM_LOG_THROW(this->currentCheckTask, storm::exceptions::InvalidStateException, "Checking has been invoked but no property has been specified before.");

    auto const& instantiatedModel = modelInstantiator.instantiate(valuation);
    if (instantiatedModel.isExact()) {
        STORM_LOG_THROW(instantiatedModel.getTransitionMatrix().isProbabilistic(storm::utility::zero<ConstantType>()),
                        storm::exceptions::InvalidArgumentException, "Instantiation point is invalid as the transition matrix becomes non-stochastic.");
    } else {
        auto const& generalSettings = storm::settings::getModule<storm::settings::modules::GeneralSettings>();
        STORM_LOG_THROW(instantiatedModel.getTransitionMatrix().isProbabilistic(storm::utility::convertNumber<ConstantType>(generalSettings.getPrecision())),
                        storm::exceptions::InvalidArgumentException, "Instantiation point is invalid as the transition matrix becomes non-stochastic.");
    }

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ConstantType>> modelChecker(instantiatedModel);

    // Check if there are some optimizations implemented for the specified property
    if (this->currentCheckTask->getFormula().isInFragment(storm::logic::reachability())) {
        return checkReachabilityProbabilityFormula(env, modelChecker, instantiatedModel);
    } else if (this->currentCheckTask->getFormula().isInFragment(storm::logic::propositional()
                                                                     .setRewardOperatorsAllowed(true)
                                                                     .setReachabilityRewardFormulasAllowed(true)
                                                                     .setOperatorAtTopLevelRequired(true)
                                                                     .setNestedOperatorsAllowed(false))) {
        return checkReachabilityRewardFormula(env, modelChecker, instantiatedModel);
    } else if (this->currentCheckTask->getFormula().isInFragment(storm::logic::propositional()
                                                                     .setProbabilityOperatorsAllowed(true)
                                                                     .setBoundedUntilFormulasAllowed(true)
                                                                     .setStepBoundedUntilFormulasAllowed(true)
                                                                     .setTimeBoundedUntilFormulasAllowed(true)
                                                                     .setOperatorAtTopLevelRequired(true)
                                                                     .setNestedOperatorsAllowed(false))) {
        return checkBoundedUntilFormula(env, modelChecker);
    } else {
        return modelChecker.check(env, *this->currentCheckTask);
    }
}

template<typename SparseModelType, typename ConstantType>
std::unique_ptr<CheckResult> SparseMdpInstantiationModelChecker<SparseModelType, ConstantType>::checkReachabilityProbabilityFormula(
    Environment const& env, storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ConstantType>>& modelChecker,
    storm::models::sparse::Mdp<ConstantType> const& instantiatedModel) {
    if (produceScheduler) {
        this->currentCheckTask->setProduceSchedulers(true);
    }

    if (!this->currentCheckTask->getHint().isExplicitModelCheckerHint()) {
        this->currentCheckTask->setHint(std::make_shared<ExplicitModelCheckerHint<ConstantType>>());
    }
    ExplicitModelCheckerHint<ConstantType>& hint = this->currentCheckTask->getHint().template asExplicitModelCheckerHint<ConstantType>();

    if (this->getInstantiationsAreGraphPreserving() && !hint.hasMaybeStates()) {
        // Perform purely qualitative analysis once
        std::vector<ConstantType> qualitativeResult;
        if (this->currentCheckTask->getFormula().asOperatorFormula().hasQuantitativeResult()) {
            auto newCheckTask = *this->currentCheckTask;
            newCheckTask.setQualitative(true);
            newCheckTask.setOnlyInitialStatesRelevant(false);
            newCheckTask.setProduceSchedulers(false);
            qualitativeResult = modelChecker.check(env, newCheckTask)->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();
        } else {
            auto newCheckTask = this->currentCheckTask->substituteFormula(this->currentCheckTask->getFormula().asOperatorFormula().getSubformula());
            newCheckTask.setQualitative(true);
            newCheckTask.setOnlyInitialStatesRelevant(false);
            newCheckTask.setProduceSchedulers(false);
            qualitativeResult =
                modelChecker.computeProbabilities(env, newCheckTask)->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();
        }
        storm::storage::BitVector maybeStates = storm::utility::vector::filter<ConstantType>(qualitativeResult, [](ConstantType const& value) -> bool {
            return !(storm::utility::isZero<ConstantType>(value) || storm::utility::isOne<ConstantType>(value));
        });
        hint.setMaybeStates(std::move(maybeStates));
        hint.setResultHint(std::move(qualitativeResult));
        hint.setComputeOnlyMaybeStates(true);

        // Check if there can be end components within the maybestates
        if (storm::solver::minimize(this->currentCheckTask->getOptimizationDirection()) ||
            storm::utility::graph::performProb1A(instantiatedModel.getTransitionMatrix(), instantiatedModel.getTransitionMatrix().getRowGroupIndices(),
                                                 instantiatedModel.getBackwardTransitions(), hint.getMaybeStates(), ~hint.getMaybeStates())
                .full()) {
            hint.setNoEndComponentsInMaybeStates(true);
        }
    }

    std::unique_ptr<CheckResult> result;
    // Check the formula and store the result as a hint for the next call.
    // For qualitative properties, we still want a quantitative result hint. Hence we perform the check on the subformula
    if (this->currentCheckTask->getFormula().asOperatorFormula().hasQuantitativeResult()) {
        result = modelChecker.check(env, *this->currentCheckTask);
        hint.setResultHint(result->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector());
        if (produceScheduler) {
            storm::storage::Scheduler<ConstantType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ConstantType>().getScheduler();
            hint.setSchedulerHint(dynamic_cast<storm::storage::Scheduler<ConstantType> const&>(scheduler));
        }
    } else {
        auto newCheckTask = this->currentCheckTask->substituteFormula(this->currentCheckTask->getFormula().asOperatorFormula().getSubformula())
                                .setOnlyInitialStatesRelevant(false);
        std::unique_ptr<storm::modelchecker::CheckResult> quantitativeResult = modelChecker.computeProbabilities(env, newCheckTask);
        result = quantitativeResult->template asExplicitQuantitativeCheckResult<ConstantType>().compareAgainstBound(
            this->currentCheckTask->getFormula().asOperatorFormula().getComparisonType(),
            this->currentCheckTask->getFormula().asOperatorFormula().template getThresholdAs<ConstantType>());
        hint.setResultHint(std::move(quantitativeResult->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector()));
        if (produceScheduler) {
            storm::storage::Scheduler<ConstantType>& scheduler = quantitativeResult->template asExplicitQuantitativeCheckResult<ConstantType>().getScheduler();
            hint.setSchedulerHint(std::move(dynamic_cast<storm::storage::Scheduler<ConstantType>&>(scheduler)));
        }
    }

    return result;
}

template<typename SparseModelType, typename ConstantType>
std::unique_ptr<CheckResult> SparseMdpInstantiationModelChecker<SparseModelType, ConstantType>::checkReachabilityRewardFormula(
    Environment const& env, storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ConstantType>>& modelChecker,
    storm::models::sparse::Mdp<ConstantType> const& instantiatedModel) {
    this->currentCheckTask->setProduceSchedulers(true);

    if (!this->currentCheckTask->getHint().isExplicitModelCheckerHint()) {
        this->currentCheckTask->setHint(std::make_shared<ExplicitModelCheckerHint<ConstantType>>());
    }
    ExplicitModelCheckerHint<ConstantType>& hint = this->currentCheckTask->getHint().template asExplicitModelCheckerHint<ConstantType>();

    if (this->getInstantiationsAreGraphPreserving() && !hint.hasMaybeStates()) {
        // Perform purely qualitative analysis once
        std::vector<ConstantType> qualitativeResult;
        if (this->currentCheckTask->getFormula().asOperatorFormula().hasQuantitativeResult()) {
            auto newCheckTask = *this->currentCheckTask;
            newCheckTask.setQualitative(true);
            newCheckTask.setOnlyInitialStatesRelevant(false);
            newCheckTask.setProduceSchedulers(false);
            qualitativeResult = modelChecker.check(env, newCheckTask)->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();
        } else {
            auto newCheckTask = this->currentCheckTask->substituteFormula(this->currentCheckTask->getFormula().asOperatorFormula().getSubformula());
            newCheckTask.setQualitative(true);
            newCheckTask.setOnlyInitialStatesRelevant(false);
            newCheckTask.setProduceSchedulers(false);
            qualitativeResult = modelChecker.computeRewards(env, newCheckTask)->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();
        }
        storm::storage::BitVector maybeStates = storm::utility::vector::filter<ConstantType>(qualitativeResult, [](ConstantType const& value) -> bool {
            return !(storm::utility::isZero<ConstantType>(value) || storm::utility::isInfinity<ConstantType>(value));
        });
        hint.setMaybeStates(std::move(maybeStates));
        hint.setResultHint(std::move(qualitativeResult));
        hint.setComputeOnlyMaybeStates(true);

        // Check if there can be end components within the maybestates
        if (storm::solver::maximize(this->currentCheckTask->getOptimizationDirection()) ||
            storm::utility::graph::performProb1A(instantiatedModel.getTransitionMatrix(), instantiatedModel.getTransitionMatrix().getRowGroupIndices(),
                                                 instantiatedModel.getBackwardTransitions(), hint.getMaybeStates(), ~hint.getMaybeStates())
                .full()) {
            hint.setNoEndComponentsInMaybeStates(true);
        }
    }

    std::unique_ptr<CheckResult> result;
    // Check the formula and store the result as a hint for the next call.
    // For qualitative properties, we still want a quantitative result hint. Hence we perform the check on the subformula
    if (this->currentCheckTask->getFormula().asOperatorFormula().hasQuantitativeResult()) {
        result = modelChecker.check(env, *this->currentCheckTask);
        storm::storage::Scheduler<ConstantType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ConstantType>().getScheduler();
        hint.setResultHint(result->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector());
        hint.setSchedulerHint(dynamic_cast<storm::storage::Scheduler<ConstantType> const&>(scheduler));
    } else {
        auto newCheckTask = this->currentCheckTask->substituteFormula(this->currentCheckTask->getFormula().asOperatorFormula().getSubformula())
                                .setOnlyInitialStatesRelevant(false);
        std::unique_ptr<storm::modelchecker::CheckResult> quantitativeResult = modelChecker.computeRewards(env, newCheckTask);
        result = quantitativeResult->template asExplicitQuantitativeCheckResult<ConstantType>().compareAgainstBound(
            this->currentCheckTask->getFormula().asOperatorFormula().getComparisonType(),
            this->currentCheckTask->getFormula().asOperatorFormula().template getThresholdAs<ConstantType>());
        storm::storage::Scheduler<ConstantType>& scheduler = quantitativeResult->template asExplicitQuantitativeCheckResult<ConstantType>().getScheduler();
        hint.setResultHint(std::move(quantitativeResult->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector()));
        hint.setSchedulerHint(std::move(dynamic_cast<storm::storage::Scheduler<ConstantType>&>(scheduler)));
    }

    return result;
}

template<typename SparseModelType, typename ConstantType>
std::unique_ptr<CheckResult> SparseMdpInstantiationModelChecker<SparseModelType, ConstantType>::checkBoundedUntilFormula(
    Environment const& env, storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ConstantType>>& modelChecker) {
    if (!this->currentCheckTask->getHint().isExplicitModelCheckerHint()) {
        this->currentCheckTask->setHint(std::make_shared<ExplicitModelCheckerHint<ConstantType>>());
    }
    std::unique_ptr<CheckResult> result;
    ExplicitModelCheckerHint<ConstantType>& hint = this->currentCheckTask->getHint().template asExplicitModelCheckerHint<ConstantType>();

    if (this->getInstantiationsAreGraphPreserving() && !hint.hasMaybeStates()) {
        // We extract the maybestates from the quantitative result
        // For qualitative properties, we still need a quantitative result. Hence we perform the check on the subformula
        if (this->currentCheckTask->getFormula().asOperatorFormula().hasQuantitativeResult()) {
            result = modelChecker.check(env, *this->currentCheckTask);
            hint.setResultHint(result->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector());
        } else {
            auto newCheckTask = this->currentCheckTask->substituteFormula(this->currentCheckTask->getFormula().asOperatorFormula().getSubformula())
                                    .setOnlyInitialStatesRelevant(false);
            std::unique_ptr<CheckResult> quantitativeResult = modelChecker.computeProbabilities(env, newCheckTask);
            result = quantitativeResult->template asExplicitQuantitativeCheckResult<ConstantType>().compareAgainstBound(
                this->currentCheckTask->getFormula().asOperatorFormula().getComparisonType(),
                this->currentCheckTask->getFormula().asOperatorFormula().template getThresholdAs<ConstantType>());
            hint.setResultHint(std::move(quantitativeResult->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector()));
        }
        storm::storage::BitVector maybeStates = storm::utility::vector::filterGreaterZero(hint.getResultHint());
        // We need to exclude the target states from the maybe states.
        // Note that we can not consider the states with probability one since a state might reach a target state with prob 1 within >0 steps
        std::unique_ptr<CheckResult> subFormulaResult =
            modelChecker.check(env, this->currentCheckTask->getFormula().asOperatorFormula().getSubformula().asBoundedUntilFormula().getRightSubformula());
        maybeStates = maybeStates & ~(subFormulaResult->template asExplicitQualitativeCheckResult<ConstantType>().getTruthValuesVector());
        hint.setMaybeStates(std::move(maybeStates));
        hint.setComputeOnlyMaybeStates(true);
    } else {
        result = modelChecker.check(env, *this->currentCheckTask);
    }
    return result;
}

template<typename SparseModelType, typename ConstantType>
bool SparseMdpInstantiationModelChecker<SparseModelType, ConstantType>::isWellDefined(
    storm::utility::parametric::Valuation<typename SparseModelType::ValueType> const& valuation) {
    // TODO note that this is quite an expensive routine currently.
    auto const& instantiatedModel = modelInstantiator.instantiate(valuation);
    if (instantiatedModel.isExact()) {
        return instantiatedModel.getTransitionMatrix().isProbabilistic(storm::utility::zero<ConstantType>());
    } else {
        auto const& generalSettings = storm::settings::getModule<storm::settings::modules::GeneralSettings>();
        return instantiatedModel.getTransitionMatrix().isProbabilistic(storm::utility::convertNumber<ConstantType>(generalSettings.getPrecision()));
    }
}

template class SparseMdpInstantiationModelChecker<storm::models::sparse::Mdp<storm::RationalFunction>, double>;
template class SparseMdpInstantiationModelChecker<storm::models::sparse::Mdp<storm::RationalFunction>, storm::RationalNumber>;

}  // namespace modelchecker
}  // namespace storm
