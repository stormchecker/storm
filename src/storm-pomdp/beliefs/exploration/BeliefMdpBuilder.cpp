#include "storm-pomdp/beliefs/exploration/BeliefMdpBuilder.h"

#include "storm-pomdp/beliefs/storage/Belief.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/ModelComponents.h"

#include "storm/exceptions/UnexpectedException.h"

namespace storm::pomdp::beliefs {

std::shared_ptr<storm::logic::Formula const> createFormulaForBeliefMdp(PropertyInformation const& propertyInformation) {
    STORM_LOG_ASSERT(propertyInformation.kind == PropertyInformation::Kind::ReachabilityProbability ||
                         propertyInformation.kind == PropertyInformation::Kind::ExpectedTotalReachabilityReward ||
                         propertyInformation.kind == PropertyInformation::Kind::RewardBoundedReachabilityProbability,
                     "Unexpected kind of property.");
    switch (propertyInformation.kind) {
        case PropertyInformation::Kind::ReachabilityProbability: {
            auto target = std::make_shared<storm::logic::AtomicLabelFormula const>("target");
            auto eventuallyTarget = std::make_shared<storm::logic::EventuallyFormula const>(target, storm::logic::FormulaContext::Probability);
            return std::make_shared<storm::logic::ProbabilityOperatorFormula const>(eventuallyTarget,
                                                                                    storm::logic::OperatorInformation(propertyInformation.dir));
        }
        case PropertyInformation::Kind::ExpectedTotalReachabilityReward: {
            auto bottom = std::make_shared<storm::logic::AtomicLabelFormula const>("target");
            auto eventuallyBottom = std::make_shared<storm::logic::EventuallyFormula const>(bottom, storm::logic::FormulaContext::Reward,
                                                                                            storm::logic::RewardAccumulation(true, false, false));
            return std::make_shared<storm::logic::RewardOperatorFormula const>(eventuallyBottom, propertyInformation.rewardModelName.value(),
                                                                               storm::logic::OperatorInformation(propertyInformation.dir));
        }
        case PropertyInformation::Kind::RewardBoundedReachabilityProbability: {
            auto target = std::make_shared<storm::logic::AtomicLabelFormula const>("target");
            auto trueFormula = std::make_shared<storm::logic::BooleanLiteralFormula const>(true);

            std::vector<boost::optional<logic::TimeBound>> lowerBounds;
            std::vector<boost::optional<logic::TimeBound>> upperBounds;
            std::vector<logic::TimeBoundReference> timeBoundReferences;

            for (auto const& rewardBound : propertyInformation.rewardBounds) {
                if (rewardBound.rewardModelName.empty()) {
                    timeBoundReferences.emplace_back();
                } else {
                    timeBoundReferences.emplace_back(rewardBound.rewardModelName);
                }
                if (rewardBound.lowerBound.has_value()) {
                    lowerBounds.emplace_back(rewardBound.lowerBound.value());
                } else {
                    lowerBounds.emplace_back(boost::none);
                }
                if (rewardBound.upperBound.has_value()) {
                    upperBounds.emplace_back(rewardBound.upperBound.value());
                } else {
                    upperBounds.emplace_back(boost::none);
                }
            }
            auto eventuallyTarget =
                std::make_shared<storm::logic::BoundedUntilFormula const>(trueFormula, target, lowerBounds, upperBounds, timeBoundReferences);
            return std::make_shared<storm::logic::ProbabilityOperatorFormula const>(eventuallyTarget,
                                                                                    storm::logic::OperatorInformation(propertyInformation.dir));
        }
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unhandled case.");
}

template<typename BeliefMdpValueType, typename BeliefType, typename... ExtraTransitionData>
std::pair<std::shared_ptr<models::sparse::Mdp<BeliefMdpValueType>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    ExplorationInformation<BeliefMdpValueType, BeliefType, ExtraTransitionData...> const& explorationInformation,
    PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, BeliefMdpValueType>(BeliefType const&)> const& computeCutOffValueMap) {
    bool const isReachProb = propertyInformation.kind == PropertyInformation::Kind::ReachabilityProbability;
    bool const isTotRew = propertyInformation.kind == PropertyInformation::Kind::ExpectedTotalReachabilityReward;
    bool const isRewBndReachProb = propertyInformation.kind == PropertyInformation::Kind::RewardBoundedReachabilityProbability;
    STORM_LOG_ASSERT(isReachProb || isTotRew || isRewBndReachProb, "Unexpected kind of property.");

    bool constexpr extraDataCompatibleWithRewardAwareness =
        sizeof...(ExtraTransitionData) == 1 && (std::is_same_v<std::vector<BeliefMdpValueType>, ExtraTransitionData> || ...);

    // First gather all cut-off information
    uint64_t nrCutOffChoices = 0ull;
    std::unordered_map<BeliefId, std::unordered_map<std::string, BeliefMdpValueType>> cutOffInformationMap;
    for (auto const& frontierBeliefId : explorationInformation.getFrontierBeliefs()) {
        auto const& frontierBelief = explorationInformation.discoveredBeliefs.getBeliefFromId(frontierBeliefId);
        cutOffInformationMap[frontierBeliefId] = computeCutOffValueMap(frontierBelief);
        nrCutOffChoices += cutOffInformationMap[frontierBeliefId].size();
    }

    constexpr bool clippingUsed = std::is_same_v<ExplorationInformation<BeliefMdpValueType, BeliefType, ExtraTransitionData...>,
                                                 ClippingExplorationInformation<BeliefMdpValueType, BeliefType>>;

    // (unbounded) reachability probabilities get a dedicated target state as an extra state
    // This is not done for reward-bounded reachability probabilities because target states are not terminal for those (e.g. because of lower reward bounds)
    // Possible optimisation: check if bottom is really needed for clipping
    uint64_t const numBottomTargetStates = isReachProb || clippingUsed ? 2ull : 1ull;
    uint64_t const numExtraStates = numBottomTargetStates + explorationInformation.getFrontierBeliefs().size();
    uint64_t const numStates = explorationInformation.matrix.groups() + numExtraStates;
    uint64_t const numChoices = explorationInformation.matrix.rows() + numBottomTargetStates + nrCutOffChoices;
    uint64_t const targetState = numStates - numBottomTargetStates;
    uint64_t const bottomState = numStates - 1;
    std::optional<models::sparse::ChoiceLabeling> optionalChoiceLabeling;
    if (explorationInformation.matrix.hasChoiceLabels()) {
        optionalChoiceLabeling = models::sparse::ChoiceLabeling(numChoices);
    }

    std::vector<BeliefMdpValueType> actionRewards;
    if (isTotRew) {
        actionRewards.reserve(numChoices);
        actionRewards.insert(actionRewards.end(), explorationInformation.actionRewards.begin(), explorationInformation.actionRewards.end());
        // Insert 0 for all cut-off choices and bottom state
        actionRewards.insert(actionRewards.end(), nrCutOffChoices + numBottomTargetStates, storm::utility::zero<BeliefMdpValueType>());
        STORM_LOG_ASSERT(numChoices == actionRewards.size(),
                         "Unexpected size of action rewards: Expected " << numChoices << " got " << actionRewards.size() << ".");
    }

    std::unordered_map<BeliefId, uint64_t> frontierBeliefToStateMap;
    std::unordered_map<uint64_t, BeliefId> stateToFrontierBeliefMap;
    uint64_t nextStateId = numStates - numExtraStates;

    std::vector<storm::storage::SparseMatrixBuilder<BeliefMdpValueType>> transitionRewardBuilderVector;

    if constexpr (extraDataCompatibleWithRewardAwareness) {
        if (isRewBndReachProb) {
            for (uint64_t i = 0; i < propertyInformation.rewardBounds.size(); ++i) {
                transitionRewardBuilderVector.emplace_back(numChoices, numStates, 0, true, true, numStates);
            }
        }
    }

    storm::storage::SparseMatrixBuilder<BeliefMdpValueType> transitionBuilder(numChoices, numStates, 0, true, true, numStates);
    // Treat explored beliefs
    for (uint64_t state = 0; state < numStates - numExtraStates; ++state) {
        uint64_t choice = explorationInformation.matrix.rowGroupIndices[state];
        transitionBuilder.newRowGroup(choice);
        for (auto& transitionRewardBuilder : transitionRewardBuilderVector) {
            transitionRewardBuilder.newRowGroup(choice);
        }
        for (uint64_t const groupEnd = explorationInformation.matrix.rowGroupIndices[state + 1]; choice < groupEnd; ++choice) {
            auto probabilityToBottom = storm::utility::zero<BeliefMdpValueType>();
            auto probabilityToTarget = storm::utility::zero<BeliefMdpValueType>();
            for (uint64_t entryIndex = explorationInformation.matrix.rowIndications[choice];
                 entryIndex < explorationInformation.matrix.rowIndications[choice + 1]; ++entryIndex) {
                auto const& entry = explorationInformation.matrix.transitions[entryIndex];
                if (auto explIt = explorationInformation.exploredBeliefs.find(entry.targetBelief); explIt != explorationInformation.exploredBeliefs.end()) {
                    // Transition to explored belief
                    transitionBuilder.addNextValue(choice, explIt->second, entry.probability);
                    if constexpr (extraDataCompatibleWithRewardAwareness) {
                        if (isRewBndReachProb) {
                            for (uint64_t i = 0; i < propertyInformation.rewardBounds.size(); ++i) {
                                if (!storm::utility::isZero(entry.data[i])) {
                                    transitionRewardBuilderVector.at(i).addNextValue(choice, explIt->second, entry.data[i]);
                                }
                            }
                        }
                    }
                    if constexpr (clippingUsed) {
                        // In case of clipping exploration, we have extra data that indicates whether the transition is a clipping transition
                        auto const& clippingProbability = std::get<0>(entry.data);
                        auto const& rewardPenalty = std::get<1>(entry.data);

                        if (clippingProbability) {
                            if (isReachProb) {
                                probabilityToBottom += *clippingProbability;
                            } else if (rewardPenalty) {
                                if (storm::utility::isInfinity(*rewardPenalty)) {
                                    /* Infinite reward on transitions is not correctly handled by the model checker. Therefore, we treat it by adding a
                                     * transition to the bottom state which due to the semantics of expected reward until reaching a target has infinite
                                     * expected reward.This causes the expected reward for the transition to become infinite. */
                                    probabilityToBottom += *clippingProbability;
                                } else {
                                    actionRewards[choice] += *rewardPenalty;
                                    probabilityToTarget += *clippingProbability;
                                }
                            } else {
                                probabilityToTarget += *clippingProbability;
                            }
                        }
                    }
                } else {
                    // Transition to unexplored belief (either terminal or cut-off)
                    BeliefMdpValueType successorValue;
                    if (auto terminalIt = explorationInformation.terminalBeliefValues.find(entry.targetBelief);
                        // Transition to terminal belief
                        terminalIt != explorationInformation.terminalBeliefValues.end()) {
                        successorValue = entry.probability * terminalIt->second;  // terminal value determined during exploration
                        if (isReachProb) {
                            probabilityToTarget += successorValue;
                            probabilityToBottom += entry.probability - successorValue;
                        } else {
                            probabilityToTarget += entry.probability;
                            actionRewards[choice] += successorValue;
                        }
                    } else {
                        // Transition to frontier belief
                        auto [insertIterator, inserted] = frontierBeliefToStateMap.insert({entry.targetBelief, nextStateId});
                        if (inserted) {
                            stateToFrontierBeliefMap[nextStateId] = entry.targetBelief;
                            ++nextStateId;
                        }
                        transitionBuilder.addNextValue(choice, insertIterator->second, entry.probability);
                        if constexpr (extraDataCompatibleWithRewardAwareness) {
                            if (isRewBndReachProb) {
                                for (uint64_t i = 0; i < propertyInformation.rewardBounds.size(); ++i) {
                                    if (!storm::utility::isZero(entry.data[i])) {
                                        transitionRewardBuilderVector.at(i).addNextValue(choice, insertIterator->second, entry.data[i]);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            // Add transition to bottom/target state if necessary
            if (!storm::utility::isZero(probabilityToTarget)) {
                transitionBuilder.addNextValue(choice, targetState, probabilityToTarget);
            }
            if (!storm::utility::isZero(probabilityToBottom)) {
                transitionBuilder.addNextValue(choice, bottomState, probabilityToBottom);
            }
            if (optionalChoiceLabeling.has_value()) {
                for (auto const& label : explorationInformation.matrix.choiceLabels.at(choice)) {
                    if (!optionalChoiceLabeling.value().containsLabel(label)) {
                        optionalChoiceLabeling.value().addLabel(label);
                    }
                    optionalChoiceLabeling.value().addLabelToChoice(label, choice);
                }
            }
        }
    }
    // Treat frontier beliefs
    uint64_t choice = explorationInformation.matrix.rows();
    for (uint64_t state = numStates - numExtraStates; state < numStates - numBottomTargetStates; ++state) {
        transitionBuilder.newRowGroup(choice);
        for (auto& transitionRewardBuilder : transitionRewardBuilderVector) {
            transitionRewardBuilder.newRowGroup(choice);
        }
        std::unordered_map<std::string, BeliefMdpValueType> cutOffInformationForBelief = cutOffInformationMap.at(stateToFrontierBeliefMap.at(state));
        for (auto const& entry : cutOffInformationForBelief) {
            if (isReachProb) {
                transitionBuilder.addNextValue(choice, targetState, entry.second);
                transitionBuilder.addNextValue(choice, bottomState, storm::utility::one<BeliefMdpValueType>() - entry.second);
            } else {
                transitionBuilder.addNextValue(choice, targetState, storm::utility::one<BeliefMdpValueType>());
                if (isTotRew) {
                    actionRewards[choice] += entry.second;
                }
            }
            if (optionalChoiceLabeling.has_value()) {
                if (!optionalChoiceLabeling.value().containsLabel(entry.first)) {
                    optionalChoiceLabeling.value().addLabel(entry.first);
                }
                optionalChoiceLabeling.value().addLabelToChoice(entry.first, choice);
            }
            ++choice;
        }
    }

    // Treat extra states
    transitionBuilder.newRowGroup(numChoices - numBottomTargetStates);
    if (optionalChoiceLabeling.has_value()) {
        if (!optionalChoiceLabeling.value().containsLabel("__loop__")) {
            optionalChoiceLabeling.value().addLabel("__loop__");
        }
        optionalChoiceLabeling.value().addLabelToChoice("__loop__", numChoices - numBottomTargetStates);
    }
    transitionBuilder.addNextValue(numChoices - numBottomTargetStates, targetState, storm::utility::one<BeliefMdpValueType>());
    for (auto& transitionRewardBuilder : transitionRewardBuilderVector) {
        transitionRewardBuilder.newRowGroup(numChoices - 1);
    }
    if (isReachProb || clippingUsed) {
        transitionBuilder.newRowGroup(numChoices - 1);
        transitionBuilder.addNextValue(numChoices - 1, bottomState, storm::utility::one<BeliefMdpValueType>());
        if (optionalChoiceLabeling.has_value()) {
            if (!optionalChoiceLabeling.value().containsLabel("__loop__")) {
                optionalChoiceLabeling.value().addLabel("__loop__");
            }
            optionalChoiceLabeling.value().addLabelToChoice("__loop__", numChoices - 1);
        }
    }

    storm::models::sparse::StateLabeling stateLabeling(numStates);
    stateLabeling.addLabel("target");
    if (isRewBndReachProb) {
        for (auto const& [belId, state] : explorationInformation.exploredBeliefs) {
            if (propertyInformation.targetObservations.count(explorationInformation.discoveredBeliefs.getBeliefFromId(belId).observation() %
                                                             explorationInformation.nrObservationsInPomdp) > 0) {
                stateLabeling.addLabelToState("target", state);
            }
        }
        for (auto const& belId : explorationInformation.getFrontierBeliefs()) {
            if (propertyInformation.targetObservations.count(explorationInformation.discoveredBeliefs.getBeliefFromId(belId).observation() %
                                                             explorationInformation.nrObservationsInPomdp) > 0) {
                stateLabeling.addLabelToState("target", frontierBeliefToStateMap.at(belId));
            }
        }
    } else {
        stateLabeling.addLabelToState("target", targetState);
    }
    stateLabeling.addLabel("init");
    stateLabeling.addLabelToState("init", explorationInformation.exploredBeliefs.at(explorationInformation.initialBeliefId));
    stateLabeling.addLabel("truncated");
    for (uint64_t state = numStates - numExtraStates; state < numStates - numBottomTargetStates; ++state) {
        stateLabeling.addLabelToState("truncated", state);
    }

    if (isReachProb || isRewBndReachProb || clippingUsed) {
        stateLabeling.addLabel("bottom");
        stateLabeling.addLabelToState("bottom", bottomState);
    }
    storm::storage::sparse::ModelComponents<BeliefMdpValueType> components(transitionBuilder.build(), std::move(stateLabeling));

    if (isTotRew) {
        storm::models::sparse::StandardRewardModel<BeliefMdpValueType> rewardModel(std::nullopt, std::move(actionRewards));
        components.rewardModels.emplace(propertyInformation.rewardModelName.value(), std::move(rewardModel));
    } else if (isRewBndReachProb) {
        uint64_t i = 0ul;
        for (auto& transitionRewardBuilder : transitionRewardBuilderVector) {
            storm::models::sparse::StandardRewardModel<BeliefMdpValueType> rewardModel(std::nullopt, std::nullopt, transitionRewardBuilder.build());
            components.rewardModels.emplace(propertyInformation.rewardBounds.at(i).rewardModelName, std::move(rewardModel));
            ++i;
        }
    }
    if (optionalChoiceLabeling.has_value()) {
        components.choiceLabeling = optionalChoiceLabeling.value();
    }

    // If requested, populate the stateToBeliefMap for the generic buildBeliefMdp variant
    std::unordered_map<uint64_t, BeliefId> stateToBeliefMap;
    // Explored beliefs
    for (auto const& [beliefId, stateIndex] : explorationInformation.exploredBeliefs) {
        stateToBeliefMap[stateIndex] = beliefId;
    }
    // Frontier beliefs: mapping was built as stateToFrontierBeliefMap
    for (auto const& [stateIndex, beliefId] : stateToFrontierBeliefMap) {
        stateToBeliefMap[stateIndex] = beliefId;
    }

    return std::make_pair(std::make_shared<storm::models::sparse::Mdp<BeliefMdpValueType>>(std::move(components)), std::move(stateToBeliefMap));
}

template std::pair<std::shared_ptr<models::sparse::Mdp<double>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    ExplorationInformation<double, Belief<double>> const& explorationInformation, PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, double>(Belief<double> const&)> const& computeCutOffValueMap);

template std::pair<std::shared_ptr<models::sparse::Mdp<storm::RationalNumber>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    ExplorationInformation<storm::RationalNumber, Belief<double>> const& explorationInformation, PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, storm::RationalNumber>(Belief<double> const&)> const& computeCutOffValueMap);

template std::pair<std::shared_ptr<models::sparse::Mdp<double>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    ExplorationInformation<double, Belief<storm::RationalNumber>> const& explorationInformation, PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, double>(Belief<storm::RationalNumber> const&)> const& computeCutOffValueMap);

template std::pair<std::shared_ptr<models::sparse::Mdp<storm::RationalNumber>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    ExplorationInformation<storm::RationalNumber, Belief<storm::RationalNumber>> const& explorationInformation, PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, storm::RationalNumber>(Belief<storm::RationalNumber> const&)> const& computeCutOffValueMap);

template std::pair<std::shared_ptr<models::sparse::Mdp<double>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    ClippingExplorationInformation<double, Belief<double>> const& explorationInformation, PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, double>(Belief<double> const&)> const& computeCutOffValueMap);

template std::pair<std::shared_ptr<models::sparse::Mdp<storm::RationalNumber>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    ClippingExplorationInformation<storm::RationalNumber, Belief<double>> const& explorationInformation, PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, storm::RationalNumber>(Belief<double> const&)> const& computeCutOffValueMap);

template std::pair<std::shared_ptr<models::sparse::Mdp<double>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    ClippingExplorationInformation<double, Belief<storm::RationalNumber>> const& explorationInformation, PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, double>(Belief<storm::RationalNumber> const&)> const& computeCutOffValueMap);

template std::pair<std::shared_ptr<models::sparse::Mdp<storm::RationalNumber>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    ClippingExplorationInformation<storm::RationalNumber, Belief<storm::RationalNumber>> const& explorationInformation,
    PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, storm::RationalNumber>(Belief<storm::RationalNumber> const&)> const& computeCutOffValueMap);

template std::pair<std::shared_ptr<models::sparse::Mdp<double>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    RewardAwareExplorationInformation<double, Belief<double>> const& explorationInformation, PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, double>(Belief<double> const&)> const& computeCutOffValueMap);

template std::pair<std::shared_ptr<models::sparse::Mdp<storm::RationalNumber>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    RewardAwareExplorationInformation<storm::RationalNumber, Belief<double>> const& explorationInformation, PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, storm::RationalNumber>(Belief<double> const&)> const& computeCutOffValueMap);

template std::pair<std::shared_ptr<models::sparse::Mdp<double>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    RewardAwareExplorationInformation<double, Belief<storm::RationalNumber>> const& explorationInformation, PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, double>(Belief<storm::RationalNumber> const&)> const& computeCutOffValueMap);

template std::pair<std::shared_ptr<models::sparse::Mdp<storm::RationalNumber>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdp(
    RewardAwareExplorationInformation<storm::RationalNumber, Belief<storm::RationalNumber>> const& explorationInformation,
    PropertyInformation const& propertyInformation,
    std::function<std::unordered_map<std::string, storm::RationalNumber>(Belief<storm::RationalNumber> const&)> const& computeCutOffValueMap);
}  // namespace storm::pomdp::beliefs
