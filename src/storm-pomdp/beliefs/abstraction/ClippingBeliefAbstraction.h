#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "storm-pomdp/beliefs/utility/types.h"
#include "storm/solver/LpSolver.h"
#include "storm/utility/constants.h"

namespace storm {
class Environment;
}
namespace storm::pomdp::beliefs {

/**
 * Approximates a belief by clipping it to an observation-specific grid point.
 * @see 10.1007/978-3-030-99527-0_2
 *
 * Clipping preserves the probability mass represented by the grid belief and reports the removed mass separately to
 * the callback. For expected rewards, optional extremal state values yield the reward correction for removed mass.
 */
template<typename BeliefType>
class ClippingBeliefAbstraction {
   public:
    using BeliefValueType = typename BeliefType::ValueType;

    /** Result of clipping one belief to one grid. */
    struct BeliefClipping {
        bool isClippable;
        BeliefType targetBelief;
        BeliefValueType delta;
        BeliefFlatMap<BeliefValueType> deltaValues;
        bool onGrid = false;
    };

    /** Creates clipping grids with one resolution per POMDP observation. */
    explicit ClippingBeliefAbstraction(storm::Environment const& env, std::vector<uint64_t>&& observationResolutions);

    /** Creates clipping grids and enables expected-reward corrections using an extremal value for every POMDP state. */
    ClippingBeliefAbstraction(storm::Environment const& env, std::vector<uint64_t>&& observationResolutions,
                              std::vector<BeliefValueType>&& extremalRewardValues);

    /**
     * Clips a belief and passes its representation to @p callback.
     *
     * The callback receives the target grid belief, retained transition probability, optional removed probability,
     * and optional reward adjustment, respectively.
     */
    template<typename AbstractCallback>
    void abstract(BeliefType&& belief, BeliefValueType&& probabilityFactor, AbstractCallback const& callback) {
        BeliefClipping clipping = clipBeliefToGrid(belief, observationResolutions[belief.observation()]);
        if (clipping.isClippable) {
            BeliefValueType a = (storm::utility::one<BeliefValueType>() - clipping.delta) * probabilityFactor;
            BeliefValueType b = clipping.delta * probabilityFactor;
            if (extremalRewardValues.has_value()) {
                // We compute the reward adjustment necessary for clipping (see https://doi.org/10.48550/arXiv.2201.08772)
                // Because we don't add clipped beliefs into the abstraction MDP, we compute the influence of the clipping transition (based on the expected
                // reward value in the implicit clipped belief) here such that we can simply add the value to the state-action reward of the transition
                auto rewardAdjustment = storm::utility::zero<BeliefValueType>();
                for (auto const& [state, deltaValue] : clipping.deltaValues) {
                    rewardAdjustment += deltaValue * extremalRewardValues->at(state);
                }
                rewardAdjustment = probabilityFactor * rewardAdjustment;
                callback(std::move(clipping.targetBelief), std::move(a), std::move(b), std::move(rewardAdjustment));
            } else {
                callback(std::move(clipping.targetBelief), std::move(a), std::move(b), std::nullopt);
            }
        } else {
            // Belief on Grid
            callback(std::move(belief), std::move(probabilityFactor), std::nullopt, std::nullopt);
        }
    }

    /** @return the grid clipping of @p belief at the given resolution. */
    BeliefClipping clipBeliefToGrid(BeliefType const& belief, uint64_t resolution);

   private:
    std::vector<uint64_t> observationResolutions;
    std::shared_ptr<storm::solver::LpSolver<BeliefValueType>> lpSolver;
    std::optional<std::vector<BeliefValueType>> extremalRewardValues = std::nullopt;
};

}  // namespace storm::pomdp::beliefs
