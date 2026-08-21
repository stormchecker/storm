#pragma once

#include <memory>
#include <string>
#include <vector>

#include "storm/models/sparse/Model.h"
#include "storm/storage/BitVector.h"

namespace storm::transformer {

template<typename ValueType>
struct TransitionToActionRewardTransformerReturnType {
    std::shared_ptr<storm::models::sparse::Model<ValueType>> model;
    std::vector<uint64_t> originalToNewStateIndices;
};
/*!
 *
 * Replaces transition branch rewards from all given reward models and replaces them by equivalent state-action based rewards.
 * This is done by potentially adding intermediate states at which the corresponding reward is collected and which have a Dirac transition to the original
 * state.
 * Notes:
 * - this construction potentially invalidates step-based properties, e.g., step-bounded reachability or discrete-time LRA properties.
 * Also Until formulas with non-trivial left-hand-side will likely be invalidated
 * - originalToNewStateIndices maps states of the original model to their positions within the transformed model. Those states are kept in the same order.
 * - the introduced intermediate states that lead to original state 's' are located directly in front of 's'.
 * - the number of intermediate states is kept small, e.g., if two distinct states 's_1' and 's_2' transition to 's' with the same transition reward
 *   (w.r.t. *all* reward models), only one intermediate state is introduced.
 * - for Markov automata, the intermediate states are probabilistic (instantaneous). For CTMCs, the intermediate states have rate 1.
 * - intermediate states do not get any label. All labels from the original model are preserved at the original states
 *
 * possible improvement: Preprocessing: move transition rewards to action if it is the same for all successor states
 *
 * @param originalModel The original model.
 * @param relevantRewardModelNames The names of the reward models that should be transformed. Error if the model does not contain a reward model with this name.
 * @return The transformed model and the positions of the original model states in the new (larger) transformed model.
 */
template<typename ValueType>
TransitionToActionRewardTransformerReturnType<ValueType> transformTransitionToActionRewards(
    std::shared_ptr<storm::models::sparse::Model<ValueType>> originalModel, std::vector<std::string> const& relevantRewardModelNames);

}  // namespace storm::transformer
