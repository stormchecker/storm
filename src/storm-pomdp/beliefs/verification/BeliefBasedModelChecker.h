#pragma once

#include "BeliefBasedModelCheckerOptions.h"
#include "BeliefBasedModelCheckerResult.h"
#include "storm-pomdp/beliefs/verification/PropertyInformation.h"
#include "storm-pomdp/storage/BeliefExplorationBounds.h"

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
    using PomdpValueType = typename PomdpModelType::ValueType;
    /** Creates a checker for a canonic POMDP. The POMDP must outlive the checker. */
    explicit BeliefBasedModelChecker(PomdpModelType const& pomdp);

    /**
     * Explores the belief MDP by unfolding the belief space. Exploration may stop early and use cut-offs or clipping.
     *
     * @return the value, completion status, and statistics produced by the checking run.
     */
    BeliefBasedModelCheckerResult<BeliefMdpValueType> checkUnfold(storm::Environment const& env, PropertyInformation const& propertyInformation,
                                                                  BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options,
                                                                  storage::BeliefExplorationBounds<PomdpValueType> const& valueBounds);

    /**
     * Explores the belief space and discretises beliefs using the Freudenthal triangualtion approximation.
     *
     * @param resolution Grid resolution used for the discretisation.
     * @param useDynamic Selects a per-belief resolution when a coarser grid represents the belief more accurately.
     * @return the value, completion status, and statistics produced by the checking run.
     */
    BeliefBasedModelCheckerResult<BeliefMdpValueType> checkDiscretize(storm::Environment const& env, PropertyInformation const& propertyInformation,
                                                                      storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options,
                                                                      uint64_t resolution, bool useDynamic,
                                                                      storage::BeliefExplorationBounds<PomdpValueType> const& valueBounds);

    /**
     * Explores a reward-aware belief MDP, splitting beliefs before successor generation by their reward vectors.
     *
     * @param relevantRewardModelNames Reward models whose accumulated rewards become part of the belief observation.
     * @return the value, completion status, and statistics produced by the checking run.
     */
    BeliefBasedModelCheckerResult<BeliefMdpValueType> checkRewardAwareUnfold(
        storm::Environment const& env, PropertyInformation const& propertyInformation,
        storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options,
        storage::BeliefExplorationBounds<typename PomdpModelType::ValueType> const& valueBounds, std::vector<std::string> const& relevantRewardModelNames = {});

    /**
     * Combines reward-aware exploration with Freudenthal triangulation discretization.
     *
     * @return the value, completion status, and statistics produced by the checking run.
     */
    BeliefBasedModelCheckerResult<BeliefMdpValueType> checkRewardAwareDiscretize(
        storm::Environment const& env, PropertyInformation const& propertyInformation,
        storm::pomdp::beliefs::BeliefBasedModelCheckerOptions<BeliefMdpValueType> const& options, uint64_t resolution, bool useDynamic,
        storage::BeliefExplorationBounds<typename PomdpModelType::ValueType> const& valueBounds, std::vector<std::string> const& relevantRewardModelNames = {});

   private:
    PomdpModelType const& inputPomdp;
};
}  // namespace pomdp::beliefs
}  // namespace storm
