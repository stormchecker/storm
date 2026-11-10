#pragma once
#include "ObservationBasedFiniteStateController.h"
#include "storm-pomdp/beliefs/storage/Belief.h"

namespace storm {
namespace models::sparse {
template<typename ValueType>
class StandardRewardModel;
template<typename ValueType, typename RewardModelType>
class Mdp;
template<typename ValueType, typename RewardModelType>
class Model;
}  // namespace models::sparse
namespace storage {
template<typename ValueType>
class Scheduler;
}
namespace pomdp::policy {

/**
 * Extracts concrete policy representations from a scheduler for a belief MDP.
 *
 * The extractor can construct an observation-based finite-state controller for the original POMDP or the Markov chain induced by the belief-MDP
 * scheduler. Approximation schedulers, if provided, determine the behavior at truncated belief states. With clipping, FSC export is approximate; see
 * exportPolicyAsFiniteStateController().
 *
 * @tparam PomdpModelType The type of the original POMDP.
 * @tparam BeliefValueType The type used to represent probabilities in beliefs.
 * @tparam BeliefMdpValueType The value type of the belief MDP and its scheduler.
 * @tparam PolicyValueType The type used for probabilities in the extracted finite-state controller.
 */
template<typename PomdpModelType, typename BeliefValueType, typename BeliefMdpValueType, typename PolicyValueType = BeliefMdpValueType>
class PolicyExtractor {
    using BeliefType = storm::pomdp::beliefs::Belief<BeliefValueType>;
    using BeliefMdpType = storm::models::sparse::Mdp<BeliefMdpValueType, storm::models::sparse::StandardRewardModel<BeliefMdpValueType>>;
    using ModelType = storm::models::sparse::Model<BeliefMdpValueType, storm::models::sparse::StandardRewardModel<BeliefMdpValueType>>;

   public:
    /**
     * Creates an extractor for a scheduler on a belief MDP.
     *
     * @param pomdp The original POMDP from which the belief MDP was derived.
     * @param beliefMdp The belief MDP on which the scheduler is defined.
     * @param beliefStateToObservationMap A mapping from belief-MDP state IDs to POMDP observation IDs.
     * @param beliefMdpScheduler The scheduler describing the policy on the belief MDP.
     * @param pomdpApproximationSchedulers Optional observation-based POMDP schedulers used for truncated belief states. Within each scheduler memory
     * state, all POMDP states sharing an observation must have identical distributions over local action indices.
     * @note The POMDP, belief MDP, scheduler, and observation map must outlive the extractor.
     */
    PolicyExtractor(
        PomdpModelType const& pomdp, BeliefMdpType const& beliefMdp, std::unordered_map<uint64_t, uint32_t> const& beliefStateToObservationMap,
        storm::storage::Scheduler<BeliefMdpValueType> const& beliefMdpScheduler,
        std::optional<std::vector<storm::storage::Scheduler<typename PomdpModelType::ValueType>>> const& pomdpApproximationSchedulers = std::nullopt);

    /**
     * Constructs an observation-based finite-state controller from the belief-MDP scheduler.
     *
     * Available observation valuations and choice labels are included as matching metadata when they provide suitable mappings. Observation valuations are
     * exported as typed signatures, so they can be matched against an equivalent POMDP with a different observable-storage order. Approximation schedulers
     * are currently supported only for memoryless policies and encoded as self-looping controller nodes for truncated belief states. Supporting finite-memory
     * cut-off policies requires controller nodes for the scheduler memory states and transitions implementing their memory updates.
     *
     * Clipping is only approximated: probability mass redirected to target or bottom states has no continuation policy. No continuation is reconstructed
     * for this mass, so the FSC may leave actions unspecified for the corresponding outcomes in the original POMDP. Its value on the original POMDP may
     * therefore differ from the reported belief MDP value.
     *
     * @return The extracted finite-state controller.
     * @throws storm::exceptions::IllegalArgumentException if a used approximation scheduler assigns different action distributions to states sharing an
     * observation within the same scheduler memory state.
     */
    ObservationBasedFiniteStateController<PolicyValueType> exportPolicyAsFiniteStateController() const;

    /**
     * Constructs the Markov chain induced by the belief-MDP scheduler.
     *
     * The resulting model retains the belief-MDP labels and adds labels identifying observations and the policies used at truncated states.
     *
     * @return The induced Markov chain as a sparse model.
     */
    std::shared_ptr<ModelType> exportPolicyAsInducedMarkovChain() const;

   private:
    /// Maps and validates the action distributions for one scheduler memory state.
    std::unordered_map<uint64_t, storm::storage::Distribution<PolicyValueType, uint64_t>> pomdpSchedulerToObservationBasedMap(
        storm::storage::Scheduler<typename PomdpModelType::ValueType> const& scheduler, uint64_t memoryState) const;

    PomdpModelType const& pomdp;
    BeliefMdpType const& beliefMdp;
    storm::storage::Scheduler<BeliefMdpValueType> const& beliefMdpScheduler;
    std::unordered_map<uint64_t, uint32_t> const& beliefStateToObservationMap;
    std::optional<std::vector<storm::storage::Scheduler<typename PomdpModelType::ValueType>>> const pomdpApproximationSchedulers;
};
}  // namespace pomdp::policy
}  // namespace storm
