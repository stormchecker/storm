#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <set>

#include "storm/models/sparse/Pomdp.h"
#include "storm/models/sparse/StandardRewardModel.h"

namespace storm::pomdp::transformer {

template<typename ValueType>
class ToStateBasedObservationTransformer {
   public:
    using ObservationType = uint32_t;
    using StateIdType = uint64_t;
    using ActionIdType = uint64_t;  // Action indices are local, i.e. 0 is always the first action of any state
    using TransitionObservationFunction = std::function<ObservationType(StateIdType, ActionIdType, StateIdType)>;

    /*!
     * Creates a POMDP (with state-based observations) out of an MDP and a transition-based observation function.
     * @param mdp The input MDP. If this is actually a POMDP, its observation function will be ignored.
     * @param transitionObservationFunction An observation function mapping transitions to observations O : S x Act x S -> Z
     * @param initialObservation The observation used for the initial state(s) of the POMDP
     * @param dropUnreachableStates Remove states unreachable from the initial states (default: true). In this mode, the observation
     * callback is only invoked for nonzero transitions from reachable states, using original state IDs and local action indices.
     * @return A POMDP with state-based observations that is equivalent to the input MDP with the given observation function.
     * Specifically, the observation assigned to the state is equal to the transition-observation with which the state has been entered.
     * This might require to create copies of states that can be entered with different observations.
     * Initial states retain @p initialObservation.
     * If unreachable states are retained, non-initial states without incoming transitions retain one copy with @p initialObservation.
     * This fallback can produce incompatible action counts within the initial observation class.
     */
    static std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> transform(storm::models::sparse::Mdp<ValueType> const& mdp,
                                                                              TransitionObservationFunction const& transitionObservationFunction,
                                                                              ObservationType initialObservation, bool dropUnreachableStates = true);

    /**
     * Makes rewards observable by extending observations with the state and state-action reward values of the selected models.
     *
     * The input POMDP must have exactly one initial state and no transition rewards in the selected reward models.
     * @param dropUnreachableStates Remove states unreachable from the initial state (default: true), as in transform().
     */
    static std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> transformRewardAware(storm::models::sparse::Pomdp<ValueType> const& pomdp,
                                                                                         std::set<std::string> const& observableRewardModels,
                                                                                         bool dropUnreachableStates = true);
};

}  // namespace storm::pomdp::transformer
