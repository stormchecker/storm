#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "storm-pomdp/beliefs/exploration/ExplorationInformation.h"
#include "storm-pomdp/beliefs/exploration/FirstStateNextStateGenerator.h"
#include "storm/utility/OptionalRef.h"
#include "storm/utility/SignalHandler.h"

namespace storm::pomdp::beliefs {

template<typename BeliefType>
class FreudenthalTriangulationBeliefAbstraction;

template<typename BeliefMdpValueType, typename PomdpType, typename BeliefType>
class RewardBoundedBeliefSplitter;

template<typename BeliefMdpValueType, typename PomdpType, typename BeliefType>
/** Stores ordinary successor transitions in exploration information. */
struct StandardDiscoverCallback {
    StandardExplorationInformation<BeliefMdpValueType, BeliefType>& info;

    explicit StandardDiscoverCallback(StandardExplorationInformation<BeliefMdpValueType, BeliefType>& info) : info(info) {
        // Intentionally left empty
    }
    void operator()(BeliefType&& bel, typename BeliefType::ValueType&& val) {
        auto const belId = info.discoveredBeliefs.getIdOrAddBelief(std::move(bel));
        if (info.exploredBeliefs.count(belId) == 0u && info.terminalBeliefValues.count(belId) == 0u) {
            info.queue.push(belId);
        }
        info.matrix.transitions.push_back({storm::utility::convertNumber<BeliefMdpValueType>(val), belId});
    }
};

template<typename BeliefMdpValueType, typename PomdpType, typename BeliefType>
/** Stores successor transitions together with clipping metadata. */
struct ClippingDiscoverCallback {
    ClippingExplorationInformation<BeliefMdpValueType, BeliefType>& info;

    explicit ClippingDiscoverCallback(ClippingExplorationInformation<BeliefMdpValueType, BeliefType>& info) : info(info) {
        // Intentionally left empty
    }
    void operator()(BeliefType&& bel, typename BeliefType::ValueType&& val, std::optional<typename BeliefType::ValueType> optionalWeightedClippingValue,
                    std::optional<typename BeliefType::ValueType> optionalRewardAdjustment) {
        auto const belId = info.discoveredBeliefs.getIdOrAddBelief(std::move(bel));
        if (info.exploredBeliefs.count(belId) == 0u && info.terminalBeliefValues.count(belId) == 0u) {
            info.queue.push(belId);
        }
        if (optionalWeightedClippingValue) {
            if (optionalRewardAdjustment) {
                STORM_LOG_TRACE("Add transition to belief " << belId << " with val " << val << ", weighted clipping value " << *optionalWeightedClippingValue
                                                            << " and reward adjustment " << *optionalRewardAdjustment << ".");
                info.matrix.transitions.push_back({storm::utility::convertNumber<BeliefMdpValueType>(val),
                                                   belId,
                                                   {storm::utility::convertNumber<BeliefMdpValueType>(*optionalWeightedClippingValue),
                                                    storm::utility::convertNumber<BeliefMdpValueType>(*optionalRewardAdjustment)}});
            } else {
                info.matrix.transitions.push_back({storm::utility::convertNumber<BeliefMdpValueType>(val),
                                                   belId,
                                                   {storm::utility::convertNumber<BeliefMdpValueType>(*optionalWeightedClippingValue), std::nullopt}});
            }
        } else {
            info.matrix.transitions.push_back({storm::utility::convertNumber<BeliefMdpValueType>(val), belId, {std::nullopt, std::nullopt}});
        }
    }
    void operator()(BeliefType&& bel, typename BeliefType::ValueType&& val) {
        auto const belId = info.discoveredBeliefs.getIdOrAddBelief(std::move(bel));
        if (info.exploredBeliefs.count(belId) == 0u && info.terminalBeliefValues.count(belId) == 0u) {
            info.queue.push(belId);
        }
        info.matrix.transitions.push_back({storm::utility::convertNumber<BeliefMdpValueType>(val), belId, {std::nullopt, std::nullopt}});
    }
};

template<typename BeliefMdpValueType, typename PomdpType, typename BeliefType>
/** Stores reward-aware successor transitions and their reward vectors. */
struct RewardAwareDiscoverCallback {
    RewardAwareExplorationInformation<BeliefMdpValueType, BeliefType>& info;

    RewardAwareDiscoverCallback(RewardAwareExplorationInformation<BeliefMdpValueType, BeliefType>& info) : info(info) {
        // Intentionally left empty
    }
    void operator()(BeliefType&& bel, typename BeliefType::ValueType&& val, std::vector<BeliefMdpValueType> const& rewards) {
        auto const belId = info.discoveredBeliefs.getIdOrAddBelief(std::move(bel));
        if (info.exploredBeliefs.count(belId) == 0u && info.terminalBeliefValues.count(belId) == 0u) {
            info.queue.push(belId);
        }
        info.matrix.transitions.push_back({storm::utility::convertNumber<BeliefMdpValueType>(val), belId, rewards});
    }
};

/**
 *  Class to perform belief exploration. Heavily templated to allow for different belief, value, abstraction, etc. types.
 *  Therefore, implementations are in the header file.
 * @tparam BeliefMdpValueType Value type used in the constructed belief MDP.
 * @tparam PomdpType POMDP type used for successor generation.
 * @tparam BeliefType Sparse belief representation.
 */
template<typename BeliefMdpValueType, typename PomdpType, typename BeliefType>
class BeliefExploration {
   public:
    using TerminationCallback = std::function<bool()>;
    using TerminalBeliefCallback = std::function<std::optional<BeliefMdpValueType>(BeliefType const&)>;

    /** Creates an explorer for @p pomdp. The POMDP must outlive the explorer. */
    explicit BeliefExploration(PomdpType const& pomdp);

    /**
     * Initializes an exploration with the initial belief as its only queued frontier belief.
     *
     * @tparam InfoType A standard, clipping, or reward-aware exploration-information type.
     */
    template<typename InfoType>
    InfoType initializeExploration(uint64_t nrObservationsInPomdp, ExplorationQueueOrder const explorationQueueOrder = ExplorationQueueOrder::Unordered) {
        InfoType info;
        info.queue.changeOrder(explorationQueueOrder);
        info.initialBeliefId = info.discoveredBeliefs.addBelief(firstStateNextStateGenerator.computeInitialBelief());
        info.queue.push(info.initialBeliefId);
        info.nrObservationsInPomdp = nrObservationsInPomdp;
        return info;
    }

    /**
     * Continues an ordinary exploration until the queue is empty, a terminal callback applies, or termination is requested.
     *
     * A post-abstraction is applied to successor beliefs when provided.
     */
    template<typename AbstractionType>
    void resumeExploration(StandardExplorationInformation<BeliefMdpValueType, BeliefType>& info, TerminalBeliefCallback const& terminalBeliefCallback,
                           TerminationCallback const& terminationCallback, storm::OptionalRef<std::string const> rewardModelName,
                           storm::OptionalRef<AbstractionType> abstraction) {
        if (rewardModelName.has_value()) {
            firstStateNextStateGenerator.setRewardModel(rewardModelName.value());
        }
        StandardDiscoverCallback<BeliefMdpValueType, PomdpType, BeliefType> discoverCallback(info);
        if (abstraction) {
            performExploration(info, firstStateNextStateGenerator.getPostAbstractionHandle(abstraction.value(), discoverCallback), terminalBeliefCallback,
                               terminationCallback);
        } else {
            performExploration(info, firstStateNextStateGenerator.getHandle(discoverCallback), terminalBeliefCallback, terminationCallback);
        }
    }

    /**
     * Continues reward-aware exploration, splitting a belief by reward vector before generating successors.
     */
    template<typename AbstractionType>
    void resumeRewardAwareExploration(RewardAwareExplorationInformation<BeliefMdpValueType, BeliefType>& info,
                                      TerminalBeliefCallback const& terminalBeliefCallback, TerminationCallback const& terminationCallback,
                                      RewardBoundedBeliefSplitter<BeliefMdpValueType, PomdpType, BeliefType> rewardSplitter,
                                      storm::OptionalRef<AbstractionType> abstraction) {
        RewardAwareDiscoverCallback<BeliefMdpValueType, PomdpType, BeliefType> discoverCallback(info);
        if (abstraction) {
            performExploration(info, firstStateNextStateGenerator.getPrePostAbstractionHandle(rewardSplitter, abstraction.value(), discoverCallback),
                               terminalBeliefCallback, terminationCallback);
        } else {
            performExploration(info, firstStateNextStateGenerator.getPreAbstractionHandle(rewardSplitter, discoverCallback), terminalBeliefCallback,
                               terminationCallback);
        }
    }

    /**
     * Continues exploration with clipping metadata on the generated transitions.
     */
    template<typename AbstractionType>
    void resumeClippingExploration(ClippingExplorationInformation<BeliefMdpValueType, BeliefType>& info, TerminalBeliefCallback const& terminalBeliefCallback,
                                   TerminationCallback const& terminationCallback, storm::OptionalRef<std::string const> rewardModelName,
                                   storm::OptionalRef<AbstractionType> abstraction) {
        if (rewardModelName.has_value()) {
            firstStateNextStateGenerator.setRewardModel(rewardModelName.value());
        }
        ClippingDiscoverCallback<BeliefMdpValueType, PomdpType, BeliefType> discoverCallback(info);
        if (abstraction) {
            performExploration(info, firstStateNextStateGenerator.getPostAbstractionHandle(abstraction.value(), discoverCallback), terminalBeliefCallback,
                               terminationCallback);
        } else {
            performExploration(info, firstStateNextStateGenerator.getHandle(discoverCallback), terminalBeliefCallback, terminationCallback);
        }
    }

   private:
    template<typename InfoType, typename NextStateHandleType>
    bool performExploration(InfoType& info, NextStateHandleType&& exploreNextStates, TerminalBeliefCallback const& terminalBeliefCallback,
                            TerminationCallback const& terminationCallback) {
        while (info.queue.hasNext()) {
            // Check if we terminate prematurely
            if ((terminationCallback && terminationCallback()) || storm::utility::resources::isTerminate()) {
                STORM_LOG_ASSERT(storm::utility::resources::isTerminate() || info.queue.getContents() == info.getFrontierBeliefs(),
                                 "Frontier beliefs inconsistent.");
                return false;  // Terminate prematurely
            }

            // Get the next belief to explore and perform some checks
            auto const currentBeliefId = info.queue.popNext();
            STORM_LOG_ASSERT(info.discoveredBeliefs.containsId(currentBeliefId), "Unknown belief id");
            STORM_LOG_ASSERT(info.exploredBeliefs.count(currentBeliefId) == 0, "Belief #" << currentBeliefId << " already explored.");
            STORM_LOG_ASSERT(info.terminalBeliefValues.count(currentBeliefId) == 0, "Belief #" << currentBeliefId << " already found to be terminal.");
            // do not take the current belief as reference since it will be invalidated when collecting more beliefs
            auto const currentBelief = info.discoveredBeliefs.getBeliefFromId(currentBeliefId);
            STORM_LOG_TRACE("Explore belief " << currentBeliefId << " : " << currentBelief.toString());
            // Check if the current belief is terminal
            if (terminalBeliefCallback) {
                if (auto terminal = terminalBeliefCallback(currentBelief); terminal.has_value()) {
                    info.terminalBeliefValues.emplace(currentBeliefId, std::move(terminal.value()));
                    continue;
                }
            }

            // Explore for each action the successors of the current belief with that action. Potentially also add rewards.
            info.exploredBeliefs.emplace(currentBeliefId, info.matrix.groups());
            auto const numActions = firstStateNextStateGenerator.getBeliefNumberOfActions(currentBelief);
            for (uint64_t localActionIndex = 0; localActionIndex < numActions; ++localActionIndex) {
                exploreNextStates(currentBelief, localActionIndex);
                info.matrix.endCurrentRow();
                if (firstStateNextStateGenerator.hasRewardModel()) {
                    info.actionRewards.emplace_back(
                        storm::utility::convertNumber<BeliefMdpValueType>(firstStateNextStateGenerator.getBeliefActionReward(currentBelief, localActionIndex)));
                }
                if (info.generateChoiceLabeling) {
                    info.matrix.choiceLabels.push_back(firstStateNextStateGenerator.getBeliefActionChoiceLabels(currentBelief, localActionIndex));
                }
            }
            info.matrix.endCurrentRowGroup();
        }
        return true;
    }

    FirstStateNextStateGenerator<PomdpType, BeliefType> firstStateNextStateGenerator;
};
}  // namespace storm::pomdp::beliefs
