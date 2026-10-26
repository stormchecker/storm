#include "storm-pomdp/api/verification.h"

#include <algorithm>

#include "storm-pomdp/analysis/FormulaInformation.h"
#include "storm-pomdp/beliefs/verification/BeliefBasedModelChecker.h"
#include "storm-pomdp/modelchecker/PreprocessingPomdpValueBoundsModelChecker.h"
#include "storm-pomdp/transformer/MakeStateSetObservationClosed.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/logic/Formula.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/transformer/MakePOMDPCanonic.h"
#include "storm/utility/macros.h"

namespace storm::pomdp::api {
namespace {

/** Preprocessed POMDP, formula, and property metadata used by belief-based verification. */
template<typename PomdpValueType>
struct BeliefVerificationInput {
    std::shared_ptr<storm::models::sparse::Pomdp<PomdpValueType>> pomdp;
    std::shared_ptr<storm::logic::Formula const> formula;
    analysis::FormulaInformation formulaInformation;
    beliefs::PropertyInformation propertyInformation;
};

template<typename PomdpValueType, typename BeliefMdpValueType>
BeliefVerificationInput<PomdpValueType> prepareBeliefVerification(storm::models::sparse::Pomdp<PomdpValueType> const& input,
                                                                  storm::logic::Formula const& formula,
                                                                  beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType>& options) {
    auto fragment = storm::logic::reachability();
    fragment.setRewardOperatorsAllowed(true).setReachabilityRewardFormulasAllowed(true).setQualitativeOperatorResultsAllowed(false);
    STORM_LOG_THROW(formula.isInFragment(fragment), storm::exceptions::NotSupportedException,
                    "The one-shot belief API supports only non-nested, unbounded reachability probabilities and expected reachability rewards without "
                    "thresholds. Got '"
                        << formula << "'.");
    auto formulaInformation = analysis::getFormulaInformation(input, formula);
    STORM_LOG_THROW(input.getInitialStates().getNumberOfSetBits() == 1, storm::exceptions::NotSupportedException,
                    "One-shot belief verification requires exactly one initial state.");
    if (options.useClipping) {
        STORM_LOG_THROW(options.clippingResolutions.has_value() && options.clippingResolutions->size() == input.getNrObservations(),
                        storm::exceptions::IllegalArgumentException, "Clipping requires one resolution per input POMDP observation.");
        STORM_LOG_THROW(std::all_of(options.clippingResolutions->begin(), options.clippingResolutions->end(), [](auto resolution) { return resolution > 0; }),
                        storm::exceptions::IllegalArgumentException, "Clipping resolutions must be positive.");
    }

    using PomdpType = storm::models::sparse::Pomdp<PomdpValueType>;
    auto pomdp = input.isCanonic() ? std::make_shared<PomdpType>(input) : storm::transformer::MakePOMDPCanonic<PomdpValueType>(input).transform();
    STORM_LOG_THROW(!options.buildChoiceLabeling || pomdp->hasChoiceLabeling(), storm::exceptions::IllegalArgumentException,
                    "Building belief MDP choice labels requires choice labels in the input POMDP. Otherwise, set options.buildChoiceLabeling to false.");
    auto [targetClosedPomdp, targetObservations] =
        storm::transformer::MakeStateSetObservationClosed<PomdpValueType>(pomdp).transform(formulaInformation.getTargetStates().states);
    pomdp = std::move(targetClosedPomdp);
    if (options.useClipping) {
        // Canonicalization and target refinement preserve state IDs. Extend the caller's observation-indexed grid accordingly.
        auto const originalResolutions = *options.clippingResolutions;
        options.clippingResolutions->resize(pomdp->getNrObservations());
        for (uint64_t state = 0; state < input.getNumberOfStates(); ++state) {
            options.clippingResolutions->at(pomdp->getObservation(state)) = originalResolutions.at(input.getObservation(state));
        }
    }
    // FormulaInformation defines sinks only for probability until formulas. For expected reachability rewards,
    // preprocessing and belief-MDP verification use reachability-reward model checking, which assigns infinity
    // to states unable to reach the target, even with zero rewards. No additional sink transformation is needed.
    if (formulaInformation.isNonNestedReachabilityProbability() && !formulaInformation.getSinkStates().empty()) {
        // Keep the row groups and action identities intact, including when sink states share an observation with other states.
        pomdp->getTransitionMatrix().makeRowGroupsAbsorbing(formulaInformation.getSinkStates().states, true);
    }

    beliefs::PropertyInformation propertyInformation;
    propertyInformation.dir = formulaInformation.getOptimizationDirection();
    propertyInformation.targetObservations = std::move(targetObservations);
    if (formulaInformation.isNonNestedExpectedRewardFormula()) {
        propertyInformation.kind = beliefs::PropertyInformation::Kind::ExpectedTotalReachabilityReward;
        propertyInformation.rewardModelName = formulaInformation.getRewardModelName();
    } else {
        propertyInformation.kind = beliefs::PropertyInformation::Kind::ReachabilityProbability;
    }
    // Preprocessing must see the refined observations and the absorbing failure states.
    formulaInformation = analysis::getFormulaInformation(*pomdp, formula);
    return {std::move(pomdp), formula.asSharedPointer(), std::move(formulaInformation), std::move(propertyInformation)};
}

template<typename PomdpValueType>
storage::BeliefExplorationBounds<PomdpValueType> computeBeliefBounds(storm::Environment const& env, BeliefVerificationInput<PomdpValueType> const& prepared,
                                                                     bool useClipping) {
    modelchecker::PreprocessingPomdpValueBoundsModelChecker<storm::models::sparse::Pomdp<PomdpValueType>> checker(*prepared.pomdp);
    storage::BeliefExplorationBounds<PomdpValueType> bounds;
    bounds.preprocessingBounds = checker.getValueBounds(env, *prepared.formula, prepared.formulaInformation);
    if (useClipping && prepared.propertyInformation.kind == beliefs::PropertyInformation::Kind::ExpectedTotalReachabilityReward) {
        bounds.extremeBounds = checker.getExtremeValueBound(env, *prepared.formula, prepared.formulaInformation);
    }
    return bounds;
}

template<typename PomdpValueType, typename BeliefValueType, typename BeliefMdpValueType, typename PolicyValueType>
beliefs::BeliefBasedModelCheckerResult<BeliefMdpValueType, PolicyValueType> unfoldPrepared(
    storm::Environment const& env, BeliefVerificationInput<PomdpValueType> const& prepared,
    beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options) {
    auto bounds = computeBeliefBounds(env, prepared, options.useClipping);
    beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<PomdpValueType>, BeliefValueType, BeliefMdpValueType, PolicyValueType> checker(
        *prepared.pomdp);
    return checker.checkUnfold(env, prepared.propertyInformation, options, bounds);
}

}  // namespace

template<typename PomdpValueType, typename BeliefValueType, typename BeliefMdpValueType, typename PolicyValueType>
beliefs::BeliefBasedModelCheckerResult<BeliefMdpValueType, PolicyValueType> verifyWithBeliefUnfolding(
    storm::Environment const& env, storm::models::sparse::Pomdp<PomdpValueType> const& pomdp, storm::logic::Formula const& formula,
    beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options) {
    auto effectiveOptions = options;
    auto prepared = prepareBeliefVerification(pomdp, formula, effectiveOptions);
    return unfoldPrepared<PomdpValueType, BeliefValueType, BeliefMdpValueType, PolicyValueType>(env, prepared, effectiveOptions);
}

template<typename PomdpValueType, typename BeliefValueType, typename BeliefMdpValueType, typename PolicyValueType>
beliefs::BeliefBasedModelCheckerResult<BeliefMdpValueType, PolicyValueType> verifyWithBeliefDiscretization(
    storm::Environment const& env, storm::models::sparse::Pomdp<PomdpValueType> const& pomdp, storm::logic::Formula const& formula, uint64_t resolution,
    bool useDynamic, beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options) {
    STORM_LOG_THROW(resolution > 0, storm::exceptions::IllegalArgumentException, "The discretization resolution must be positive.");
    STORM_LOG_THROW(options.getTerminationCriterion() == beliefs::NONE, storm::exceptions::NotSupportedException,
                    "Belief discretization does not support exploration size or time limits.");
    STORM_LOG_THROW(!options.useClipping, storm::exceptions::NotSupportedException, "Belief discretization does not support clipping.");
    auto effectiveOptions = options;
    auto prepared = prepareBeliefVerification(pomdp, formula, effectiveOptions);
    auto bounds = computeBeliefBounds(env, prepared, false);
    beliefs::BeliefBasedModelChecker<storm::models::sparse::Pomdp<PomdpValueType>, BeliefValueType, BeliefMdpValueType, PolicyValueType> checker(
        *prepared.pomdp);
    return checker.checkDiscretize(env, prepared.propertyInformation, effectiveOptions, resolution, useDynamic, bounds);
}

template beliefs::BeliefBasedModelCheckerResult<double> verifyWithBeliefUnfolding<double, double, double>(
    storm::Environment const&, storm::models::sparse::Pomdp<double> const&, storm::logic::Formula const&,
    beliefs::BeliefBasedModelCheckerOptions<double> const&);
template beliefs::BeliefBasedModelCheckerResult<double> verifyWithBeliefDiscretization<double, double, double>(
    storm::Environment const&, storm::models::sparse::Pomdp<double> const&, storm::logic::Formula const&, uint64_t, bool,
    beliefs::BeliefBasedModelCheckerOptions<double> const&);

template beliefs::BeliefBasedModelCheckerResult<double> verifyWithBeliefUnfolding<double, storm::RationalNumber, double>(
    storm::Environment const&, storm::models::sparse::Pomdp<double> const&, storm::logic::Formula const&,
    beliefs::BeliefBasedModelCheckerOptions<double> const&);
template beliefs::BeliefBasedModelCheckerResult<double> verifyWithBeliefDiscretization<double, storm::RationalNumber, double>(
    storm::Environment const&, storm::models::sparse::Pomdp<double> const&, storm::logic::Formula const&, uint64_t, bool,
    beliefs::BeliefBasedModelCheckerOptions<double> const&);

template beliefs::BeliefBasedModelCheckerResult<storm::RationalNumber>
verifyWithBeliefUnfolding<storm::RationalNumber, storm::RationalNumber, storm::RationalNumber>(
    storm::Environment const&, storm::models::sparse::Pomdp<storm::RationalNumber> const&, storm::logic::Formula const&,
    beliefs::BeliefBasedModelCheckerOptions<storm::RationalNumber> const&);
template beliefs::BeliefBasedModelCheckerResult<storm::RationalNumber>
verifyWithBeliefDiscretization<storm::RationalNumber, storm::RationalNumber, storm::RationalNumber>(
    storm::Environment const&, storm::models::sparse::Pomdp<storm::RationalNumber> const&, storm::logic::Formula const&, uint64_t, bool,
    beliefs::BeliefBasedModelCheckerOptions<storm::RationalNumber> const&);

template beliefs::BeliefBasedModelCheckerResult<double> verifyWithBeliefUnfolding<storm::RationalNumber, storm::RationalNumber, double>(
    storm::Environment const&, storm::models::sparse::Pomdp<storm::RationalNumber> const&, storm::logic::Formula const&,
    beliefs::BeliefBasedModelCheckerOptions<double> const&);
template beliefs::BeliefBasedModelCheckerResult<double> verifyWithBeliefDiscretization<storm::RationalNumber, storm::RationalNumber, double>(
    storm::Environment const&, storm::models::sparse::Pomdp<storm::RationalNumber> const&, storm::logic::Formula const&, uint64_t, bool,
    beliefs::BeliefBasedModelCheckerOptions<double> const&);

template beliefs::BeliefBasedModelCheckerResult<storm::RationalNumber> verifyWithBeliefUnfolding<double, double, storm::RationalNumber>(
    storm::Environment const&, storm::models::sparse::Pomdp<double> const&, storm::logic::Formula const&,
    beliefs::BeliefBasedModelCheckerOptions<storm::RationalNumber> const&);
template beliefs::BeliefBasedModelCheckerResult<storm::RationalNumber> verifyWithBeliefDiscretization<double, double, storm::RationalNumber>(
    storm::Environment const&, storm::models::sparse::Pomdp<double> const&, storm::logic::Formula const&, uint64_t, bool,
    beliefs::BeliefBasedModelCheckerOptions<storm::RationalNumber> const&);

template beliefs::BeliefBasedModelCheckerResult<double, storm::RationalNumber> verifyWithBeliefUnfolding<double, double, double, storm::RationalNumber>(
    storm::Environment const&, storm::models::sparse::Pomdp<double> const&, storm::logic::Formula const&,
    beliefs::BeliefBasedModelCheckerOptions<double> const&);
template beliefs::BeliefBasedModelCheckerResult<double, storm::RationalNumber> verifyWithBeliefDiscretization<double, double, double, storm::RationalNumber>(
    storm::Environment const&, storm::models::sparse::Pomdp<double> const&, storm::logic::Formula const&, uint64_t, bool,
    beliefs::BeliefBasedModelCheckerOptions<double> const&);

}  // namespace storm::pomdp::api
