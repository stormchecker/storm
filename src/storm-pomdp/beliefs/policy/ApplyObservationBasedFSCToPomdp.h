#pragma once

#include "storm-pomdp/beliefs/policy/ObservationBasedFiniteStateController.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Pomdp.h"

namespace storm {
namespace pomdp::policy {
template<typename PomdpValueType>
ObservationSignature getObservationSignature(storm::models::sparse::Pomdp<PomdpValueType> const& pomdp, uint64_t const observationId);

/**
 * Applies an observation-based finite-state controller to a POMDP and constructs the induced DTMC.
 *
 * States of the resulting DTMC represent reachable pairs of a POMDP state and an FSC memory node. If the FSC provides observation signatures or action
 * names, they are used to match its outputs to the POMDP; otherwise, the corresponding numeric IDs are assumed to agree. Observation signatures match
 * observable names and typed values, so the observable-storage order need not agree. State-action rewards are averaged over the FSC action distribution
 * and added to state rewards. Models containing transition rewards are not supported.
 *
 * @tparam PomdpValueType The type used for probabilities and rewards in the POMDP and resulting DTMC.
 * @tparam FscValueType The type used for probabilities in randomised FSC action outputs.
 * @param pomdp The canonic POMDP to which the controller is applied.
 * @param fsc The finite-state controller to apply.
 * @param treatUnspecifiedChoiceAsDontCare If true, an unspecified FSC output selects local action zero and keeps the current FSC memory node. If false,
 * an unspecified output causes an exception.
 * @return The DTMC induced by the POMDP and the FSC.
 * @throws storm::exceptions::IllegalArgumentException If the POMDP is not canonic.
 * @throws storm::exceptions::NotSupportedException If the POMDP does not have exactly one initial state or any reward model has transition rewards.
 * @throws storm::exceptions::UnexpectedException If required observation or action names cannot be matched, or if the FSC has an unspecified output and
 * @p treatUnspecifiedChoiceAsDontCare is false.
 */
template<typename PomdpValueType, typename FscValueType>
storm::models::sparse::Dtmc<PomdpValueType> applyObservationBasedFSCToPomdp(
    storm::models::sparse::Pomdp<PomdpValueType> const& pomdp, storm::pomdp::policy::ObservationBasedFiniteStateController<FscValueType> const& fsc,
    bool treatUnspecifiedChoiceAsDontCare = false);
}  // namespace pomdp::policy
}  // namespace storm
