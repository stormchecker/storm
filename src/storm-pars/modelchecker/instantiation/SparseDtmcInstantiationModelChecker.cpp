#include "storm-pars/modelchecker/instantiation/SparseDtmcInstantiationModelChecker.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/hints/ExplicitModelCheckerHint.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {

template<typename SparseModelType, typename ConstantType>
SparseDtmcInstantiationModelChecker<SparseModelType, ConstantType>::SparseDtmcInstantiationModelChecker(SparseModelType const& parametricModel)
    : SparseInstantiationModelChecker<SparseModelType, ConstantType>(parametricModel), modelInstantiator(parametricModel) {
    // Intentionally left empty
}

template<typename SparseModelType, typename ConstantType>
std::unique_ptr<CheckResult> SparseDtmcInstantiationModelChecker<SparseModelType, ConstantType>::check(
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

    storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ConstantType>> modelChecker(instantiatedModel);

    // Check if there are some optimizations implemented for the specified property
    if (this->currentCheckTask->getFormula().isInFragment(storm::logic::reachability())) {
        return checkReachabilityProbabilityFormula(env, modelChecker);
    } else if (this->currentCheckTask->getFormula().isInFragment(storm::logic::propositional()
                                                                     .setRewardOperatorsAllowed(true)
                                                                     .setReachabilityRewardFormulasAllowed(true)
                                                                     .setOperatorAtTopLevelRequired(true)
                                                                     .setNestedOperatorsAllowed(false))) {
        return checkReachabilityRewardFormula(env, modelChecker);
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
std::unique_ptr<CheckResult> SparseDtmcInstantiationModelChecker<SparseModelType, ConstantType>::checkReachabilityProbabilityFormula(
    Environment const& env, storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ConstantType>>& modelChecker) {
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
            qualitativeResult = modelChecker.check(env, newCheckTask)->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();
        } else {
            auto newCheckTask = this->currentCheckTask->substituteFormula(this->currentCheckTask->getFormula().asOperatorFormula().getSubformula());
            newCheckTask.setQualitative(true);
            newCheckTask.setOnlyInitialStatesRelevant(false);
            qualitativeResult =
                modelChecker.computeProbabilities(env, newCheckTask)->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();
        }
        storm::storage::BitVector maybeStates = storm::utility::vector::filter<ConstantType>(qualitativeResult, [](ConstantType const& value) -> bool {
            return !(storm::utility::isZero<ConstantType>(value) || storm::utility::isOne<ConstantType>(value));
        });
        hint.setMaybeStates(std::move(maybeStates));
        hint.setResultHint(std::move(qualitativeResult));
        hint.setComputeOnlyMaybeStates(true);
    }

    std::unique_ptr<CheckResult> result;
    // Check the formula and store the result as a hint for the next call.
    // For qualitative properties, we still want a quantitative result hint. Hence we perform the check on the subformula
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

    return result;
}

template<typename SparseModelType, typename ConstantType>
std::unique_ptr<CheckResult> SparseDtmcInstantiationModelChecker<SparseModelType, ConstantType>::checkReachabilityRewardFormula(
    Environment const& env, storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ConstantType>>& modelChecker) {
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
            qualitativeResult = modelChecker.check(env, newCheckTask)->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();
        } else {
            auto newCheckTask = this->currentCheckTask->substituteFormula(this->currentCheckTask->getFormula().asOperatorFormula().getSubformula());
            newCheckTask.setQualitative(true);
            newCheckTask.setOnlyInitialStatesRelevant(false);
            qualitativeResult = modelChecker.computeRewards(env, newCheckTask)->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();
        }
        storm::storage::BitVector maybeStates = storm::utility::vector::filter<ConstantType>(qualitativeResult, [](ConstantType const& value) -> bool {
            return !(storm::utility::isZero<ConstantType>(value) || storm::utility::isInfinity<ConstantType>(value));
        });
        hint.setMaybeStates(std::move(maybeStates));
        hint.setResultHint(std::move(qualitativeResult));
        hint.setComputeOnlyMaybeStates(true);
    }

    std::unique_ptr<CheckResult> result;

    // Check the formula and store the result as a hint for the next call.
    // For qualitative properties, we still want a quantitative result hint. Hence we perform the check on the subformula
    if (this->currentCheckTask->getFormula().asOperatorFormula().hasQuantitativeResult()) {
        result = modelChecker.check(env, *this->currentCheckTask);
        this->currentCheckTask->getHint().template asExplicitModelCheckerHint<ConstantType>().setResultHint(
            result->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector());
    } else {
        auto newCheckTask = this->currentCheckTask->substituteFormula(this->currentCheckTask->getFormula().asOperatorFormula().getSubformula())
                                .setOnlyInitialStatesRelevant(false);
        std::unique_ptr<CheckResult> quantitativeResult = modelChecker.computeRewards(env, newCheckTask);
        result = quantitativeResult->template asExplicitQuantitativeCheckResult<ConstantType>().compareAgainstBound(
            this->currentCheckTask->getFormula().asOperatorFormula().getComparisonType(),
            this->currentCheckTask->getFormula().asOperatorFormula().template getThresholdAs<ConstantType>());
        this->currentCheckTask->getHint().template asExplicitModelCheckerHint<ConstantType>().setResultHint(
            std::move(quantitativeResult->template asExplicitQuantitativeCheckResult<ConstantType>().getValueVector()));
    }

    return result;
}

template<typename SparseModelType, typename ConstantType>
std::unique_ptr<CheckResult> SparseDtmcInstantiationModelChecker<SparseModelType, ConstantType>::checkBoundedUntilFormula(
    Environment const& env, storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ConstantType>>& modelChecker) {
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
bool SparseDtmcInstantiationModelChecker<SparseModelType, ConstantType>::isWellDefined(
    storm::utility::parametric::Valuation<typename SparseModelType::ValueType> const& valuation) {
    auto const& instantiatedModel = modelInstantiator.instantiate(valuation);
    // Should be moved further outside.
    if (instantiatedModel.isExact()) {
        return instantiatedModel.getTransitionMatrix().isProbabilistic(storm::utility::zero<ConstantType>());
    } else {
        auto const& generalSettings = storm::settings::getModule<storm::settings::modules::GeneralSettings>();
        return instantiatedModel.getTransitionMatrix().isProbabilistic(storm::utility::convertNumber<ConstantType>(generalSettings.getPrecision()));
    }
}

template class SparseDtmcInstantiationModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, double>;
template class SparseDtmcInstantiationModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, storm::RationalNumber>;

}  // namespace modelchecker
}  // namespace storm
