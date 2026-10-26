#pragma once

#include "storm-pomdp/beliefs/verification/BeliefBasedModelCheckerOptions.h"
#include "storm-pomdp/beliefs/verification/BeliefBasedModelCheckerResult.h"
#include "storm/environment/Environment.h"
#include "storm/models/sparse/Pomdp.h"

namespace storm::logic {
class Formula;
}

namespace storm::pomdp::api {

/**
 * Approximates an unbounded reachability probability or expected reachability reward by unfolding the belief MDP.
 *
 * Preparation canonicalizes the POMDP if needed (requiring choice labels),
 * refines observations to identify targets, makes until-failure states absorbing, and computes frontier value bounds.
 * Exactly one initial state and an explicit min/max optimization direction are required. Only non-nested formulae without
 * thresholds are supported; the result is quantitative. Solver precision and arithmetic follow the supplied environment
 * and numeric types.
 *
 * The returned value is a lower bound for maximization and an upper bound for minimization. Without clipping, complete exploration
 * gives the unabstracted belief MDP value, subject to numerical model checking. Completion with clipping does not imply exactness.
 *
 * @param options All unfolding settings. Unset size/time limits mean unlimited exploration; no heuristic budget is selected.
 * maxExplorationSize counts discovered beliefs, not transition depth, and is checked between expansions, so one expansion's
 * successors can exceed it. Zero uses preprocessing cut-offs without expanding any belief. maxExplorationTime is in seconds
 * and applies to exploration, not preprocessing or MDP verification. Building choice labels requires input choice labels,
 * even for an already canonical POMDP; otherwise disable buildChoiceLabeling. When clipping is enabled, clippingResolutions
 * must contain one positive entry per input observation. Observations introduced by target refinement inherit their original
 * observation's resolution.
 *
 * The supported (POMDP, belief, belief MDP) value types are (double, double, double), (double, RationalNumber, double),
 * (RationalNumber, RationalNumber, RationalNumber), (RationalNumber, RationalNumber, double), and (double, double, RationalNumber).
 * PolicyValueType defaults to BeliefMdpValueType for these combinations. Additionally, (double, double, double, RationalNumber)
 * is supported when specifying all four value types.
 *
 * Setting options.generatePolicy returns a finite-state controller in result.policy. Policy generation requires
 * options.buildChoiceLabeling and is incompatible with options.cutZeroGap. PolicyValueType determines the type of policy
 * probabilities independently of BeliefMdpValueType.
 *
 * The returned policy refers to the prepared POMDP: its local action indices follow canonicalization, its observation IDs
 * include target refinement, and until-failure states are absorbing. The API returns neither the prepared model nor a
 * mapping back to the input model. Callers must account for these transformations before applying the policy to the input.
 * With clipping, redirected probability mass has no reconstructed continuation policy, so the controller may be partial
 * and its value on the input POMDP may differ from result.value.
 *
 * @return The bound, exploration completion status, statistics, and optional policy. result.policy is unset unless
 * options.generatePolicy is enabled.
 *
 * Example:
 * @code
 * storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<double> options;
 * options.maxExplorationSize = 1000;
 * options.maxExplorationTime = 10;
 * auto result = storm::pomdp::api::verifyWithBeliefUnfolding(env, pomdp, formula, options);
 * @endcode
 */
template<typename PomdpValueType, typename BeliefValueType = PomdpValueType, typename BeliefMdpValueType = PomdpValueType,
         typename PolicyValueType = BeliefMdpValueType>
beliefs::BeliefBasedModelCheckerResult<BeliefMdpValueType, PolicyValueType> verifyWithBeliefUnfolding(
    storm::Environment const& env, storm::models::sparse::Pomdp<PomdpValueType> const& pomdp, storm::logic::Formula const& formula,
    beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options);

/** Uses a default Environment; otherwise identical to the environment-taking overload. */
template<typename PomdpValueType, typename BeliefValueType = PomdpValueType, typename BeliefMdpValueType = PomdpValueType,
         typename PolicyValueType = BeliefMdpValueType>
beliefs::BeliefBasedModelCheckerResult<BeliefMdpValueType, PolicyValueType> verifyWithBeliefUnfolding(
    storm::models::sparse::Pomdp<PomdpValueType> const& pomdp, storm::logic::Formula const& formula,
    beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options) {
    return verifyWithBeliefUnfolding<PomdpValueType, BeliefValueType, BeliefMdpValueType, PolicyValueType>(storm::Environment(), pomdp, formula, options);
}

/**
 * Approximates an unbounded reachability probability or expected reachability reward by Freudenthal discretization.
 *
 * Preparation and numeric support match verifyWithBeliefUnfolding. The value is an upper bound for maximization and a
 * lower bound for minimization. Complete exploration describes the discretized MDP and does not imply exactness.
 *
 * @param resolution Positive integer grid resolution (default 2).
 * @param useDynamic Select per-belief resolutions using dynamic triangulation (default false).
 * @param options Exploration settings. Size/time limits, useClipping, and generatePolicy are rejected. cutZeroGap,
 * buildChoiceLabeling, and explorationQueueOrder are forwarded to the belief model checker and honored as in unfolding.
 * clippingResolutions is ignored because clipping is disabled.
 * @return The bound, exploration completion status, and statistics; result.policy is unset.
 *
 * @code
 * auto result = storm::pomdp::api::verifyWithBeliefDiscretization(env, pomdp, formula, 10, true);
 * @endcode
 */
template<typename PomdpValueType, typename BeliefValueType = PomdpValueType, typename BeliefMdpValueType = PomdpValueType,
         typename PolicyValueType = BeliefMdpValueType>
beliefs::BeliefBasedModelCheckerResult<BeliefMdpValueType, PolicyValueType> verifyWithBeliefDiscretization(
    storm::Environment const& env, storm::models::sparse::Pomdp<PomdpValueType> const& pomdp, storm::logic::Formula const& formula, uint64_t resolution = 2,
    bool useDynamic = false, beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options = {});

/** Uses a default Environment; otherwise identical to the environment-taking overload. */
template<typename PomdpValueType, typename BeliefValueType = PomdpValueType, typename BeliefMdpValueType = PomdpValueType,
         typename PolicyValueType = BeliefMdpValueType>
beliefs::BeliefBasedModelCheckerResult<BeliefMdpValueType, PolicyValueType> verifyWithBeliefDiscretization(
    storm::models::sparse::Pomdp<PomdpValueType> const& pomdp, storm::logic::Formula const& formula, uint64_t resolution = 2, bool useDynamic = false,
    beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options = {}) {
    return verifyWithBeliefDiscretization<PomdpValueType, BeliefValueType, BeliefMdpValueType, PolicyValueType>(storm::Environment(), pomdp, formula,
                                                                                                                resolution, useDynamic, options);
}

}  // namespace storm::pomdp::api
