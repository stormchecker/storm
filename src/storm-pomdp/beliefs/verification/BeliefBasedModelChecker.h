#pragma once

#include "BeliefBasedModelCheckerOptions.h"
#include "storm-pomdp/beliefs/verification/PropertyInformation.h"
#include "storm-pomdp/storage/BeliefExplorationBounds.h"

#include <optional>

namespace storm {
class Environment;

namespace pomdp::beliefs {

template<typename PomdpModelType, typename BeliefValueType = typename PomdpModelType::ValueType,
         typename BeliefMdpValueType = typename PomdpModelType::ValueType>
/**
 * Builds and checks a finite belief MDP approximation of a POMDP.
 *
 * The POMDP, belief, and generated MDP may use different value types. Callers provide preprocessing value bounds;
 * these are used to value explicit frontier cut-offs when exploration is incomplete.
 */
class BeliefBasedModelChecker {
   public:
    using PomdpValueType = PomdpModelType::ValueType;

    /** Statistics recorded for the most recently completed checking run. */
    struct RunStatistics {
        bool available = false;
        bool completedExploration = false;
        uint64_t discoveredBeliefs = 0;
        uint64_t exploredBeliefs = 0;
        uint64_t beliefMdpStates = 0;
        uint64_t beliefMdpChoices = 0;
        uint64_t beliefMdpTransitions = 0;
        std::optional<uint64_t> processedMdpStates;
        std::optional<uint64_t> processedMdpChoices;
        std::optional<uint64_t> processedMdpTransitions;
        uint64_t explorationTimeMilliseconds = 0;
        uint64_t beliefMdpBuildTimeMilliseconds = 0;
        uint64_t beliefMdpAnalysisTimeMilliseconds = 0;
    };

    /** Creates a checker for a canonic POMDP. The POMDP must outlive the checker. */
    explicit BeliefBasedModelChecker(PomdpModelType const& pomdp);

    /**
     * Explores the belief MDP by unfolding the belief space. Exploration may stop early and use cut-offs or clipping.
     *
     * @return the value of the constructed MDP at the initial belief and whether exploration completed.
     */
    std::pair<BeliefMdpValueType, bool> checkUnfold(storm::Environment const& env, PropertyInformation const& propertyInformation,
                                                    BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options,
                                                    storage::BeliefExplorationBounds<PomdpValueType> const& valueBounds);

    /**
     * Explores the belief space and discretises beliefs using the Freudenthal triangualtion approximation.
     *
     * @param resolution Grid resolution used for the discretisation.
     * @param useDynamic Selects a per-belief resolution when a coarser grid represents the belief more accurately.
     * @return the value of the constructed MDP at the initial belief and whether exploration completed.
     */
    std::pair<BeliefMdpValueType, bool> checkDiscretize(storm::Environment const& env, PropertyInformation const& propertyInformation,
                                                        storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options,
                                                        uint64_t resolution, bool useDynamic,
                                                        storage::BeliefExplorationBounds<PomdpValueType> const& valueBounds);

    /**
     * Explores a reward-aware belief MDP, splitting beliefs before successor generation by their reward vectors.
     *
     * @param relevantRewardModelNames Reward models whose accumulated rewards become part of the belief observation.
     * @return the value of the constructed MDP at the initial belief and whether exploration completed.
     */
    std::pair<BeliefMdpValueType, bool> checkRewardAwareUnfold(storm::Environment const& env, PropertyInformation const& propertyInformation,
                                                               storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options,
                                                               storage::BeliefExplorationBounds<typename PomdpModelType::ValueType> const& valueBounds,
                                                               std::vector<std::string> const& relevantRewardModelNames = {});

    /**
     * Combines reward-aware exploration with Freudenthal triangulation discretization.
     *
     * @return the value of the constructed MDP at the initial belief and whether exploration completed.
     */
    std::pair<BeliefMdpValueType, bool> checkRewardAwareDiscretize(storm::Environment const& env, PropertyInformation const& propertyInformation,
                                                                   storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options,
                                                                   uint64_t resolution, bool useDynamic,
                                                                   storage::BeliefExplorationBounds<typename PomdpModelType::ValueType> const& valueBounds,
                                                                   std::vector<std::string> const& relevantRewardModelNames = {});

    /** @return statistics for the last checking invocation. */
    RunStatistics const& getLastRunStatistics() const;

   private:
    PomdpModelType const& inputPomdp;
    RunStatistics lastRunStatistics;
};
}  // namespace pomdp::beliefs
}  // namespace storm
