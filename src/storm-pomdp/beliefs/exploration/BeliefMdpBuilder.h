#pragma once

#include <functional>
#include <memory>

#include "storm-pomdp/beliefs/exploration/ExplorationInformation.h"
#include "storm-pomdp/beliefs/verification/PropertyInformation.h"
#include "storm/logic/Formulas.h"
#include "storm/models/sparse/Mdp.h"

namespace storm::pomdp::beliefs {

std::shared_ptr<storm::logic::Formula const> createFormulaForBeliefMdp(PropertyInformation const& propertyInformation);

/**
 * Builds a belief MDP from the given exploration information and property information.
 * Frontier beliefs receive explicit cut-off choices from computeCutOffValueMap.
 *
 * The reward-aware belief exploration does not classify beliefs as terminal. A target observation alone does not decide whether the accumulated
 * rewards satisfy the bound, in particular when there are lower bounds. Such beliefs therefore remain explored or frontier states;
 * their transition reward vectors are retained on their ordinary edges.
 *
 * @tparam BeliefMdpValueType ValueType of the belief MDP
 * @tparam BeliefType Type of the belief
 * @tparam ExtraTransitionData Types of additional data to store for transitions (e.g. reward vectors)
 * @param explorationInformation object containing information about the exploration of the belief space (explored beliefs, transitions, etc.)
 * @param propertyInformation object containing information about the property to verify
 * @param computeCutOffValueMap function to compute all cut-off values for a belief given the provided information. A separate cut-off action is added for each
 * value, allowing the model checker to choose the best one. This choice can later be retraced.
 * @return the belief MDP and a mapping from represented belief states to their belief IDs; synthetic target and bottom sinks
 * are omitted from the mapping */
template<typename BeliefMdpValueType, typename BeliefType, typename... ExtraTransitionData>
std::pair<std::shared_ptr<models::sparse::Mdp<BeliefMdpValueType>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    ExplorationInformation<BeliefMdpValueType, BeliefType, ExtraTransitionData...> const& explorationInformation,
    PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, BeliefMdpValueType>(BeliefType const&)> const& computeCutOffValueMap);
}  // namespace storm::pomdp::beliefs
