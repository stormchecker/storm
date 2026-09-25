#pragma once

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "storm-pomdp/beliefs/exploration/BeliefExplorationMatrix.h"
#include "storm-pomdp/beliefs/exploration/ExplorationQueue.h"
#include "storm-pomdp/beliefs/storage/BeliefCollector.h"
#include "storm-pomdp/beliefs/utility/types.h"

namespace storm::pomdp::beliefs {
/**
 * Mutable state of one belief-MDP exploration.
 *
 * A discovered belief is either explored, terminal, or on the frontier. The matrix contains rows only for explored
 * beliefs; frontier beliefs are completed with explicit cut-off choices when the belief MDP is built.
 */
template<typename BeliefMdpValueType, typename BeliefType, typename... ExtraTransitionData>
struct ExplorationInformation {
    BeliefExplorationMatrix<BeliefMdpValueType, ExtraTransitionData...> matrix;
    std::vector<BeliefMdpValueType> actionRewards;
    storm::pomdp::beliefs::BeliefCollector<BeliefType> discoveredBeliefs;
    std::unordered_map<BeliefId, BeliefStateType> exploredBeliefs;
    std::unordered_map<BeliefId, BeliefMdpValueType> terminalBeliefValues;
    BeliefId initialBeliefId;
    ExplorationQueue queue;
    uint64_t nrObservationsInPomdp;
    bool generateChoiceLabeling = false;

    /** @return discovered beliefs that have neither been explored nor classified as terminal. */
    [[nodiscard]] std::unordered_set<BeliefId> getFrontierBeliefs() const {
        std::unordered_set<BeliefId> resFrontierBeliefs;
        for (uint64_t id = 0; id < discoveredBeliefs.getNumberOfBeliefIds(); id++) {
            if (!exploredBeliefs.contains(id) && !terminalBeliefValues.contains(id)) {
                resFrontierBeliefs.insert(id);
            }
        }
        return resFrontierBeliefs;
    }
};

template<typename BeliefMdpValueType, typename BeliefType>
/** Exploration information without transition metadata. */
using StandardExplorationInformation = ExplorationInformation<BeliefMdpValueType, BeliefType>;

template<typename BeliefMdpValueType, typename BeliefType>
/** Exploration information that stores a reward vector on each transition. */
using RewardAwareExplorationInformation = ExplorationInformation<BeliefMdpValueType, BeliefType, std::vector<BeliefMdpValueType>>;

template<typename BeliefMdpValueType, typename BeliefType>
/** Exploration information that stores optional clipping probability and reward-adjustment metadata. */
using ClippingExplorationInformation =
    ExplorationInformation<BeliefMdpValueType, BeliefType, std::optional<BeliefMdpValueType>, std::optional<BeliefMdpValueType>>;
}  // namespace storm::pomdp::beliefs
