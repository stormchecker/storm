#include "storm-pomdp/beliefs/verification/BeliefBasedModelChecker.h"

#include "storm-pomdp/beliefs/abstraction/ClippingBeliefAbstraction.h"
#include "storm-pomdp/beliefs/abstraction/FreudenthalTriangulationBeliefAbstraction.h"
#include "storm-pomdp/beliefs/abstraction/RewardBoundedBeliefSplitter.h"
#include "storm-pomdp/beliefs/exploration/BeliefExploration.h"
#include "storm-pomdp/beliefs/exploration/BeliefMdpBuilder.h"
#include "storm-pomdp/beliefs/storage/Belief.h"
#include "storm-pomdp/beliefs/verification/BeliefBasedModelCheckerOptions.h"
#include "storm/api/verification.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/transformer/GoalStateMerger.h"
#include "storm/transformer/TransitionToActionRewardTransformer.h"
#include "storm/utility/OptionalRef.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/constants.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

#include <sstream>

namespace storm::pomdp::beliefs {

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMdpValueType>
BeliefBasedModelChecker<PomdpModelType, BeliefValueType, BeliefMdpValueType>::BeliefBasedModelChecker(PomdpModelType const& pomdp) : inputPomdp(pomdp) {
    STORM_LOG_ERROR_COND(inputPomdp.isCanonic(), "Input Pomdp is not known to be canonic. This might lead to unexpected verification results.");
}

/** Creates the callback that ends exploration and turns the queued beliefs into the frontier. */
template<typename PomdpModelType, typename BeliefType, typename BeliefMdpValueType, typename InfoType>
typename BeliefExploration<BeliefMdpValueType, PomdpModelType, BeliefType>::TerminationCallback getTerminationCallback(
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options, InfoType& info, storm::utility::Stopwatch& swExplore) {
    switch (options.getTerminationCriterion()) {
        case MAX_EXPLORATION_SIZE:
            return [&info, maxSize = options.maxExplorationSize.value()]() { return info.discoveredBeliefs.getNumberOfBeliefIds() > maxSize; };
        case MAX_EXPLORATION_TIME:
            return [&swExplore, maxDuration = options.maxExplorationTime.value()]() { return swExplore.getTimeInSeconds() > static_cast<double>(maxDuration); };
        case MAX_EXPLORATION_SIZE_AND_TIME:
            return [&info, &swExplore, maxSize = options.maxExplorationSize.value(), maxDuration = options.maxExplorationTime.value()]() {
                return info.discoveredBeliefs.getNumberOfBeliefIds() > maxSize || swExplore.getTimeInSeconds() > static_cast<double>(maxDuration);
            };
        case NONE:
            // Unlimited unfolding (useful for known finite belief MDPs)
            return []() { return false; };
        default:
            STORM_LOG_ERROR("Unknown termination criterion for belief exploration.");
            return []() { return false; };
    }
}

/**
 * Creates a callback that recognizes property targets and optional small-gap cut-offs as terminal beliefs.
 *
 * Terminal beliefs receive their fixed value directly; all other unfinished beliefs remain explicit frontier states.
 */
template<typename PomdpModelType, typename BeliefType, typename BeliefMdpValueType>
typename BeliefExploration<BeliefMdpValueType, PomdpModelType, BeliefType>::TerminalBeliefCallback getTerminalBeliefCallback(
    PropertyInformation const& propertyInformation, storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options,
    storm::pomdp::storage::PreprocessingPomdpValueBounds<typename PomdpModelType::ValueType> const& valueBounds) {
    using PomdpValueType = PomdpModelType::ValueType;
    if (propertyInformation.kind == PropertyInformation::Kind::ExpectedTotalReachabilityReward) {
        if (options.maxGapToCut.has_value()) {
            // Terminate if the gap is small enough
            auto const maxGapToCut = storm::utility::convertNumber<PomdpValueType>(options.maxGapToCut.value());
            return [&propertyInformation, &valueBounds, maxGapToCut](BeliefType const& belief) -> std::optional<BeliefMdpValueType> {
                if (propertyInformation.targetObservations.contains(belief.observation())) {
                    return storm::utility::zero<BeliefMdpValueType>();
                } else {
                    auto smallestUpper = storm::utility::infinity<PomdpValueType>();
                    for (auto const& valueList : valueBounds.upper) {
                        smallestUpper = std::min(smallestUpper, belief.template getWeightedSum<PomdpValueType>(valueList));
                    }
                    PomdpValueType largestLower = -storm::utility::infinity<PomdpValueType>();
                    for (auto const& valueList : valueBounds.lower) {
                        largestLower = storm::utility::max(largestLower, belief.template getWeightedSum<PomdpValueType>(valueList));
                    }
                    if (storm::utility::abs<PomdpValueType>(smallestUpper - largestLower) <= maxGapToCut) {
                        if constexpr (std::is_same_v<PomdpValueType, BeliefMdpValueType>) {
                            return propertyInformation.dir == solver::OptimizationDirection::Maximize ? largestLower : smallestUpper;
                        } else {
                            return storm::utility::convertNumber<BeliefMdpValueType>(
                                propertyInformation.dir == solver::OptimizationDirection::Maximize ? largestLower : smallestUpper);
                        }
                    }
                    return std::nullopt;
                }
            };
        } else {
            return [&propertyInformation](BeliefType const& belief) -> std::optional<BeliefMdpValueType> {
                if (propertyInformation.targetObservations.contains(belief.observation())) {
                    return storm::utility::zero<BeliefMdpValueType>();
                } else {
                    return std::nullopt;
                }
            };
        }
    } else if (propertyInformation.kind == PropertyInformation::Kind::RewardBoundedReachabilityProbability) {
        return [](BeliefType const& belief) -> std::optional<BeliefMdpValueType> {
            // For reward-bounded properties, we cannot be sure that a target belief is terminal as we are not bound-aware at this point
            return std::nullopt;
        };
    } else if (options.maxGapToCut.has_value()) {
        // Terminate if the gap is small enough
        auto const maxGapToCut = storm::utility::convertNumber<PomdpValueType>(options.maxGapToCut.value());
        return [&propertyInformation, &valueBounds, maxGapToCut](BeliefType const& belief) -> std::optional<BeliefMdpValueType> {
            if (propertyInformation.targetObservations.contains(belief.observation())) {
                return storm::utility::one<BeliefMdpValueType>();
            } else {
                auto smallestUpper = storm::utility::infinity<PomdpValueType>();
                for (auto const& valueList : valueBounds.upper) {
                    smallestUpper = std::min(smallestUpper, belief.template getWeightedSum<PomdpValueType>(valueList));
                }
                PomdpValueType largestLower = -storm::utility::infinity<PomdpValueType>();
                for (auto const& valueList : valueBounds.lower) {
                    largestLower = storm::utility::max(largestLower, belief.template getWeightedSum<PomdpValueType>(valueList));
                }
                if (storm::utility::abs<PomdpValueType>(smallestUpper - largestLower) <= maxGapToCut) {
                    if constexpr (std::is_same_v<PomdpValueType, BeliefMdpValueType>) {
                        return propertyInformation.dir == solver::OptimizationDirection::Maximize ? largestLower : smallestUpper;
                    } else {
                        return storm::utility::convertNumber<BeliefMdpValueType>(
                            propertyInformation.dir == solver::OptimizationDirection::Maximize ? largestLower : smallestUpper);
                    }
                }
                return std::nullopt;
            }
        };
    } else {
        return [&propertyInformation](BeliefType const& belief) -> std::optional<BeliefMdpValueType> {
            if (propertyInformation.targetObservations.contains(belief.observation())) {
                return storm::utility::one<BeliefMdpValueType>();
            } else {
                return std::nullopt;
            };
        };
    }
}

/**
 * Converts exploration information into a finite MDP.
 *
 * Every frontier belief becomes an explicit state with one selectable cut-off action per preprocessing bound.
 */
template<typename PomdpModelType, typename BeliefType, typename BeliefMdpValueType, typename InfoType>
std::pair<std::shared_ptr<models::sparse::Mdp<BeliefMdpValueType>>, std::unordered_map<uint64_t, BeliefId>> buildBeliefMdpFromInfo(
    PropertyInformation const& propertyInformation, storm::pomdp::storage::PreprocessingPomdpValueBounds<typename PomdpModelType::ValueType> const& valueBounds,
    InfoType const& info) {
    using PomdpValueType = PomdpModelType::ValueType;
    std::function<std::unordered_map<std::string, BeliefMdpValueType>(BeliefType const&)> computeCutOffValueMap =
        [&valueBounds, &propertyInformation](BeliefType const& belief) {
            // Add addtional cut-off sources here
            uint64_t const nrCutoffPolicies =
                propertyInformation.dir == storm::OptimizationDirection::Minimize ? valueBounds.upper.size() : valueBounds.lower.size();
            std::unordered_map<std::string, BeliefMdpValueType> result;
            for (uint64_t i = 0; i < nrCutoffPolicies; ++i) {
                auto val = belief.template getWeightedSum<PomdpValueType>(
                    propertyInformation.dir == storm::OptimizationDirection::Minimize ? valueBounds.upper.at(i) : valueBounds.lower.at(i));
                if constexpr (std::is_same_v<PomdpValueType, BeliefMdpValueType>) {
                    result["__sched_" + std::to_string(i)] = val;
                } else {
                    result["__sched_" + std::to_string(i)] = storm::utility::convertNumber<BeliefMdpValueType>(val);
                }
            }
            return result;
        };
    return buildBeliefMdp(info, propertyInformation, computeCutOffValueMap);
}

template<typename PomdpModelType, typename BeliefType, typename BeliefMdpValueType, typename AbstractionType,
         typename InfoType = StandardExplorationInformation<BeliefMdpValueType, BeliefType>>
std::pair<BeliefMdpValueType, bool> checkUnfoldOrDiscretize(
    storm::Environment const& env, PomdpModelType const& pomdp, PropertyInformation const& propertyInformation,
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options,
    storage::BeliefExplorationBounds<typename PomdpModelType::ValueType> const& valueBounds, storm::OptionalRef<AbstractionType> abstraction = {},
    typename BeliefBasedModelChecker<PomdpModelType, typename BeliefType::ValueType, BeliefMdpValueType>::RunStatistics* statistics = nullptr) {
    STORM_LOG_ASSERT(propertyInformation.kind == PropertyInformation::Kind::ReachabilityProbability ||
                         propertyInformation.kind == PropertyInformation::Kind::ExpectedTotalReachabilityReward,
                     "Unexpected kind of property.");

    STORM_LOG_INFO("Exploring the belief space.");

    // First, explore the beliefs and its successors
    using BeliefExplorationType = BeliefExploration<BeliefMdpValueType, PomdpModelType, BeliefType>;
    storm::utility::Stopwatch swExplore(true);
    BeliefExplorationType exploration(pomdp);

    auto info = exploration.template initializeExploration<InfoType>(pomdp.getNrObservations(), options.explorationQueueOrder);
    info.generateChoiceLabeling = options.buildChoiceLabeling;

    // Determine terminationCallback based on options
    typename BeliefExplorationType::TerminationCallback terminationCallback =
        getTerminationCallback<PomdpModelType, BeliefType, BeliefMdpValueType>(options, info, swExplore);

    // Determine terminalBeliefCallback based on options
    typename BeliefExplorationType::TerminalBeliefCallback terminalBeliefCallback =
        getTerminalBeliefCallback<PomdpModelType, BeliefType, BeliefMdpValueType>(propertyInformation, options, *valueBounds.preprocessingBounds);

    if constexpr (std::is_same_v<InfoType, ClippingExplorationInformation<BeliefMdpValueType, BeliefType>>) {
        STORM_LOG_ASSERT(options.useClipping, "Clipping exploration information requires clipping to be enabled.");
        if (propertyInformation.kind == PropertyInformation::Kind::ExpectedTotalReachabilityReward) {
            exploration.resumeClippingExploration(info, terminalBeliefCallback, terminationCallback, propertyInformation.rewardModelName.value(), abstraction);
        } else {
            exploration.resumeClippingExploration(info, terminalBeliefCallback, terminationCallback, storm::NullRef, abstraction);
        }
        STORM_LOG_TRACE("Starting clipping phase.");
        STORM_LOG_ASSERT(options.clippingResolutions.has_value(), "Clipping requested, but no resolution vector given.");
        std::vector<uint64_t> resolutions(options.clippingResolutions.value());
        if (propertyInformation.kind == PropertyInformation::Kind::ExpectedTotalReachabilityReward) {
            STORM_LOG_ASSERT(valueBounds.extremeBounds.has_value(),
                             "Clipping for expected total reachability reward requires extreme value bounds to be given.");
            ClippingBeliefAbstraction<BeliefType> clippingAbstraction(
                env, std::move(resolutions), std::move(valueBounds.extremeBounds->template copyValues<typename BeliefType::ValueType>()));
            exploration.resumeClippingExploration(
                info, terminalBeliefCallback, []() { return false; }, propertyInformation.rewardModelName.value(), storm::OptionalRef(clippingAbstraction));
        } else {
            ClippingBeliefAbstraction<BeliefType> clippingAbstraction(env, std::move(resolutions));
            exploration.resumeClippingExploration(
                info, terminalBeliefCallback, []() { return false; }, storm::NullRef, storm::OptionalRef(clippingAbstraction));
        }
        STORM_LOG_TRACE("Finished clipping phase.");
    } else {
        if (propertyInformation.kind == PropertyInformation::Kind::ExpectedTotalReachabilityReward) {
            exploration.resumeExploration(info, terminalBeliefCallback, terminationCallback, propertyInformation.rewardModelName.value(), abstraction);
        } else {
            exploration.resumeExploration(info, terminalBeliefCallback, terminationCallback, storm::NullRef, abstraction);
        }
    }
    swExplore.stop();
    bool earlyExplorationStop = info.queue.hasNext();
    if (statistics) {
        statistics->available = true;
        statistics->completedExploration = !earlyExplorationStop;
        statistics->discoveredBeliefs = info.discoveredBeliefs.getNumberOfBeliefIds();
        statistics->exploredBeliefs = info.exploredBeliefs.size();
        statistics->explorationTimeMilliseconds = swExplore.getTimeInMilliseconds();
    }
    if (earlyExplorationStop) {
        STORM_LOG_INFO("Exploration stopped before all beliefs were explored. " << info.discoveredBeliefs.getNumberOfBeliefIds() << " beliefs discovered. "
                                                                                << info.exploredBeliefs.size() << " beliefs explored.");
    }

    // Second, build the Belief MDP from the exploration information
    STORM_LOG_INFO("Constructing the belief MDP.");
    storm::utility::Stopwatch swBuild(true);
    auto [beliefMdp, stateToBeliefMap] =
        buildBeliefMdpFromInfo<PomdpModelType, BeliefType, BeliefMdpValueType, InfoType>(propertyInformation, *valueBounds.preprocessingBounds, info);
    swBuild.stop();
    {
        std::stringstream stream;
        beliefMdp->printModelInformationToStream(stream);
        STORM_LOG_INFO("Constructed belief MDP:\n" << stream.str());
    }
    if (statistics) {
        statistics->beliefMdpStates = beliefMdp->getNumberOfStates();
        statistics->beliefMdpChoices = beliefMdp->getNumberOfChoices();
        statistics->beliefMdpTransitions = beliefMdp->getNumberOfTransitions();
        statistics->beliefMdpBuildTimeMilliseconds = swBuild.getTimeInMilliseconds();
    }

    // Finally, perform model checking on the belief MDP.
    storm::utility::Stopwatch swCheck(true);
    auto formula = createFormulaForBeliefMdp(propertyInformation);
    storm::modelchecker::CheckTask<storm::logic::Formula, BeliefMdpValueType> task(*formula, true);
    std::unique_ptr<storm::modelchecker::CheckResult> res(storm::api::verifyWithSparseEngine<BeliefMdpValueType>(env, beliefMdp, task));
    swCheck.stop();
    if (statistics) {
        statistics->beliefMdpAnalysisTimeMilliseconds = swCheck.getTimeInMilliseconds();
    }
    STORM_LOG_INFO("Time for exploring beliefs: " << swExplore << ".");
    STORM_LOG_INFO("Time for building the belief MDP: " << swBuild << ".");
    STORM_LOG_INFO("Time for analyzing the belief MDP: " << swCheck << ".");
    STORM_LOG_ASSERT(res, "Model checking of belief MDP did not return any result.");
    STORM_LOG_ASSERT(res->isExplicitQuantitativeCheckResult(), "Model checking of belief MDP did not return result of expected type.");
    STORM_LOG_ASSERT(beliefMdp->getInitialStates().getNumberOfSetBits() == 1, "Unexpected number of initial states for belief Mdp.");
    auto const initState = beliefMdp->getInitialStates().getNextSetIndex(0);
    return {res->asExplicitQuantitativeCheckResult<BeliefMdpValueType>()[initState], !earlyExplorationStop};
}

template<typename PomdpModelType, typename BeliefType, typename BeliefMdpValueType>
std::pair<BeliefMdpValueType, bool> checkRewardAwareUnfoldOrDiscretize(
    storm::Environment const& env, PomdpModelType const& pomdp, PropertyInformation const& propertyInformation,
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options,
    storage::BeliefExplorationBounds<typename PomdpModelType::ValueType> const& valueBounds,
    RewardBoundedBeliefSplitter<BeliefMdpValueType, PomdpModelType, BeliefType>& rewardSplitter,
    storm::OptionalRef<FreudenthalTriangulationBeliefAbstraction<BeliefType>> abstraction = {},
    typename BeliefBasedModelChecker<PomdpModelType, typename BeliefType::ValueType, BeliefMdpValueType>::RunStatistics* statistics = nullptr) {
    STORM_LOG_ASSERT(propertyInformation.kind == PropertyInformation::Kind::RewardBoundedReachabilityProbability, "Unexpected kind of property.");
    STORM_LOG_ASSERT(rewardSplitter.getNumberOfSetRewardModels() != 0, "rewardSplitter must have a reward model set for reward-aware belief MDP construction.");

    STORM_LOG_INFO("Exploring the belief space.");

    // First, explore the beliefs and its successors
    using BeliefExplorationType = BeliefExploration<BeliefMdpValueType, PomdpModelType, BeliefType>;
    storm::utility::Stopwatch swExplore(true);
    BeliefExplorationType exploration(pomdp);
    using InfoType = RewardAwareExplorationInformation<BeliefMdpValueType, BeliefType>;
    auto info = exploration.template initializeExploration<InfoType>(pomdp.getNrObservations(), options.explorationQueueOrder);

    // Determine terminationCallback based on options
    typename BeliefExplorationType::TerminationCallback terminationCallback =
        getTerminationCallback<PomdpModelType, BeliefType, BeliefMdpValueType>(options, info, swExplore);

    // Determine terminalBeliefCallback based on options
    typename BeliefExplorationType::TerminalBeliefCallback terminalBeliefCallback =
        getTerminalBeliefCallback<PomdpModelType, BeliefType, BeliefMdpValueType>(propertyInformation, options, *valueBounds.preprocessingBounds);

    exploration.resumeRewardAwareExploration(info, terminalBeliefCallback, terminationCallback, rewardSplitter, abstraction);
    swExplore.stop();
    bool const earlyExplorationStop = info.queue.hasNext();
    if (statistics) {
        statistics->available = true;
        statistics->completedExploration = !earlyExplorationStop;
        statistics->discoveredBeliefs = info.discoveredBeliefs.getNumberOfBeliefIds();
        statistics->exploredBeliefs = info.exploredBeliefs.size();
        statistics->explorationTimeMilliseconds = swExplore.getTimeInMilliseconds();
    }
    if (earlyExplorationStop) {
        STORM_LOG_INFO("Exploration stopped before all beliefs were explored. " << info.discoveredBeliefs.getNumberOfBeliefIds() << " beliefs discovered. "
                                                                                << info.exploredBeliefs.size() << " beliefs explored.");
    }

    // Second, build the Belief MDP from the exploration information
    STORM_LOG_INFO("Constructing the belief MDP.");
    storm::utility::Stopwatch swBuild(true);
    auto [beliefMdp, stateToBeliefMap] =
        buildBeliefMdpFromInfo<PomdpModelType, BeliefType, BeliefMdpValueType, InfoType>(propertyInformation, *valueBounds.preprocessingBounds, info);
    swBuild.stop();
    {
        std::stringstream stream;
        beliefMdp->printModelInformationToStream(stream);
        STORM_LOG_INFO("Constructed belief MDP:\n" << stream.str());
    }
    if (statistics) {
        statistics->beliefMdpStates = beliefMdp->getNumberOfStates();
        statistics->beliefMdpChoices = beliefMdp->getNumberOfChoices();
        statistics->beliefMdpTransitions = beliefMdp->getNumberOfTransitions();
        statistics->beliefMdpBuildTimeMilliseconds = swBuild.getTimeInMilliseconds();
    }

    // Finally, perform model checking on the belief MDP.
    auto formula = createFormulaForBeliefMdp(propertyInformation);
    STORM_LOG_INFO("Analyzing property '" << *formula << "' on the belief MDP.");
    storm::utility::Stopwatch swCheck(true);
    std::shared_ptr<storm::models::sparse::Mdp<BeliefMdpValueType>> processedMdp = beliefMdp;
    if (propertyInformation.kind == PropertyInformation::Kind::RewardBoundedReachabilityProbability) {
        std::vector<std::string> rewardModelNames;
        for (auto const& bnd : propertyInformation.rewardBounds) {
            rewardModelNames.push_back(bnd.rewardModelName);
        }
        processedMdp = storm::transformer::transformTransitionToActionRewards<BeliefMdpValueType>(processedMdp, rewardModelNames)
                           .model->template as<storm::models::sparse::Mdp<BeliefMdpValueType>>();
        double increase = static_cast<double>(processedMdp->getNumberOfStates()) / static_cast<double>(beliefMdp->getNumberOfStates());
        STORM_LOG_INFO("Transformation of transition rewards resulted in a model with " << processedMdp->getNumberOfStates() << " states. " << increase
                                                                                        << " times more states than the original belief MDP.");

        // Cut away states that can not reach the target
        auto targetStates = processedMdp->getStateLabeling().getStates("target");
        storm::storage::BitVector allStates(targetStates.size(), true);
        storm::storage::BitVector probGreaterZeroStates;
        if (storm::solver::maximize(propertyInformation.dir)) {
            probGreaterZeroStates = storm::utility::graph::performProbGreater0E(processedMdp->getBackwardTransitions(), allStates, targetStates);
        } else {
            probGreaterZeroStates =
                storm::utility::graph::performProbGreater0A(processedMdp->getTransitionMatrix(), processedMdp->getTransitionMatrix().getRowGroupIndices(),
                                                            processedMdp->getBackwardTransitions(), allStates, targetStates);
        }
        auto mergingResult = storm::transformer::GoalStateMerger(*processedMdp)
                                 .mergeTargetAndSinkStates(probGreaterZeroStates, ~probGreaterZeroStates, ~allStates, rewardModelNames);
        processedMdp = mergingResult.model;
        STORM_LOG_INFO("Merging of sink states resulted in a model with " << processedMdp->getNumberOfStates() << " states.");
    }
    if (statistics && processedMdp != beliefMdp) {
        statistics->processedMdpStates = processedMdp->getNumberOfStates();
        statistics->processedMdpChoices = processedMdp->getNumberOfChoices();
        statistics->processedMdpTransitions = processedMdp->getNumberOfTransitions();
    }
    storm::modelchecker::CheckTask<storm::logic::Formula, BeliefMdpValueType> task(*formula, true);
    std::unique_ptr<storm::modelchecker::CheckResult> res(storm::api::verifyWithSparseEngine<BeliefMdpValueType>(env, processedMdp, task));
    swCheck.stop();
    if (statistics) {
        statistics->beliefMdpAnalysisTimeMilliseconds = swCheck.getTimeInMilliseconds();
    }
    STORM_LOG_INFO("Time for exploring beliefs: " << swExplore << ".");
    STORM_LOG_INFO("Time for building the belief MDP: " << swBuild << ".");
    STORM_LOG_INFO("Time for analyzing the belief MDP: " << swCheck << ".");
    STORM_LOG_ASSERT(res, "Model checking of belief MDP did not return any result.");
    STORM_LOG_ASSERT(res->isExplicitQuantitativeCheckResult(), "Model checking of belief MDP did not return result of expected type.");
    STORM_LOG_ASSERT(processedMdp->getInitialStates().getNumberOfSetBits() == 1, "Unexpected number of initial states for (processed) belief Mdp.");
    auto const initState = processedMdp->getInitialStates().getNextSetIndex(0);
    return {res->asExplicitQuantitativeCheckResult<BeliefMdpValueType>()[initState], !earlyExplorationStop};
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMdpValueType>
std::pair<BeliefMdpValueType, bool> BeliefBasedModelChecker<PomdpModelType, BeliefValueType, BeliefMdpValueType>::checkUnfold(
    storm::Environment const& env, PropertyInformation const& propertyInformation,
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options,
    storm::pomdp::storage::BeliefExplorationBounds<typename PomdpModelType::ValueType> const& valueBounds) {
    lastRunStatistics = RunStatistics();
    if (options.useClipping) {
        return checkUnfoldOrDiscretize<PomdpModelType, Belief<BeliefValueType>, BeliefMdpValueType, NoAbstractionType,
                                       ClippingExplorationInformation<BeliefMdpValueType, Belief<BeliefValueType>>>(
            env, inputPomdp, propertyInformation, options, valueBounds, {}, &lastRunStatistics);
    } else {
        return checkUnfoldOrDiscretize<PomdpModelType, Belief<BeliefValueType>, BeliefMdpValueType, NoAbstractionType>(
            env, inputPomdp, propertyInformation, options, valueBounds, {}, &lastRunStatistics);
    }
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMdpValueType>
std::pair<BeliefMdpValueType, bool> BeliefBasedModelChecker<PomdpModelType, BeliefValueType, BeliefMdpValueType>::checkDiscretize(
    storm::Environment const& env, PropertyInformation const& propertyInformation,
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options, uint64_t resolution, bool useDynamic,
    storage::BeliefExplorationBounds<typename PomdpModelType::ValueType> const& valueBounds) {
    lastRunStatistics = RunStatistics();
    auto mode = useDynamic ? FreudenthalTriangulationMode::Dynamic : FreudenthalTriangulationMode::Static;
    FreudenthalTriangulationBeliefAbstraction<Belief<BeliefValueType>> abstraction(storm::utility::convertNumber<BeliefValueType>(resolution), mode);
    return checkUnfoldOrDiscretize<PomdpModelType, Belief<BeliefValueType>, BeliefMdpValueType>(env, inputPomdp, propertyInformation, options, valueBounds,
                                                                                                storm::OptionalRef(abstraction), &lastRunStatistics);
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMdpValueType>
std::pair<BeliefMdpValueType, bool> BeliefBasedModelChecker<PomdpModelType, BeliefValueType, BeliefMdpValueType>::checkRewardAwareUnfold(
    storm::Environment const& env, PropertyInformation const& propertyInformation,
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options,
    storage::BeliefExplorationBounds<typename PomdpModelType::ValueType> const& valueBounds, std::vector<std::string> const& relevantRewardModelNames) {
    lastRunStatistics = RunStatistics();
    RewardBoundedBeliefSplitter<BeliefMdpValueType, PomdpModelType, Belief<BeliefValueType>> rewardBoundedBeliefSplitter(inputPomdp);
    if (relevantRewardModelNames.empty()) {
        rewardBoundedBeliefSplitter.setRewardModel();
    } else {
        rewardBoundedBeliefSplitter.setRewardModels(relevantRewardModelNames);
    }
    return checkRewardAwareUnfoldOrDiscretize<PomdpModelType, Belief<BeliefValueType>, BeliefMdpValueType>(
        env, inputPomdp, propertyInformation, options, valueBounds, rewardBoundedBeliefSplitter, {}, &lastRunStatistics);
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMdpValueType>
std::pair<BeliefMdpValueType, bool> BeliefBasedModelChecker<PomdpModelType, BeliefValueType, BeliefMdpValueType>::checkRewardAwareDiscretize(
    storm::Environment const& env, PropertyInformation const& propertyInformation,
    storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options, uint64_t resolution, bool useDynamic,
    storage::BeliefExplorationBounds<typename PomdpModelType::ValueType> const& valueBounds, std::vector<std::string> const& relevantRewardModelNames) {
    lastRunStatistics = RunStatistics();
    auto mode = useDynamic ? FreudenthalTriangulationMode::Dynamic : FreudenthalTriangulationMode::Static;
    FreudenthalTriangulationBeliefAbstraction<Belief<BeliefValueType>> abstraction(storm::utility::convertNumber<BeliefValueType>(resolution), mode);
    RewardBoundedBeliefSplitter<BeliefMdpValueType, PomdpModelType, Belief<BeliefValueType>> rewardBoundedBeliefSplitter(inputPomdp);
    if (relevantRewardModelNames.empty()) {
        rewardBoundedBeliefSplitter.setRewardModel();
    } else {
        rewardBoundedBeliefSplitter.setRewardModels(relevantRewardModelNames);
    }
    return checkRewardAwareUnfoldOrDiscretize<PomdpModelType, Belief<BeliefValueType>, BeliefMdpValueType>(
        env, inputPomdp, propertyInformation, options, valueBounds, rewardBoundedBeliefSplitter, abstraction, &lastRunStatistics);
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMdpValueType>
typename BeliefBasedModelChecker<PomdpModelType, BeliefValueType, BeliefMdpValueType>::RunStatistics const&
BeliefBasedModelChecker<PomdpModelType, BeliefValueType, BeliefMdpValueType>::getLastRunStatistics() const {
    return lastRunStatistics;
}

template class BeliefBasedModelChecker<storm::models::sparse::Pomdp<double>, double, double>;
template class BeliefBasedModelChecker<storm::models::sparse::Pomdp<double>, storm::RationalNumber, double>;
template class BeliefBasedModelChecker<storm::models::sparse::Pomdp<storm::RationalNumber>, storm::RationalNumber, storm::RationalNumber>;
template class BeliefBasedModelChecker<storm::models::sparse::Pomdp<storm::RationalNumber>, storm::RationalNumber, double>;
template class BeliefBasedModelChecker<storm::models::sparse::Pomdp<double>, double, storm::RationalNumber>;
// Currently we don't consider this combination as having rational numbers in models, but floats in beliefs does not really help us
//  template class BeliefBasedModelChecker<storm::models::sparse::Pomdp<storm::RationalNumber>, double, storm::RationalNumber>;
}  // namespace storm::pomdp::beliefs
