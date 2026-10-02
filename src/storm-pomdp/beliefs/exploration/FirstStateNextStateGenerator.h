#pragma once

#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "storm-pomdp/beliefs/abstraction/NoAbstraction.h"

#include "storm-pomdp/beliefs/storage/BeliefBuilder.h"
#include "storm-pomdp/beliefs/utility/types.h"

#include "storm/utility/constants.h"

namespace storm::pomdp::beliefs {

// Forward declare handle
namespace detail {
template<typename PomdpType, typename BeliefType, typename PreAbstractionType, typename PostAbstractionType, typename DiscoverCallback>
struct NextStateGeneratorHandle;
}

/*!
 * This class implements a first-state-next-state generator interface for exploring the belief MDP of a given POMDP.
 * It provides methods to compute the initial belief, the number of actions available in a belief, and the reward of a given action in a belief.
 *
 * Furthermore, this class can configure a handle for the generation of successor beliefs in the belief MDP.
 * Such a handle can be called with a belief and an action, which will then compute all the successors of the given belief w.r.t. the given action
 * In its simplest form, the handle is configured using a callback function, which will be invoked on any discovered successor.
 * One can additionally provide a pre-abstraction and/or a post-abstraction when configuring the handle.
 * An abstraction is a class that provides a method `abstract` that takes a belief as input and returns a distribution over abstracted beliefs as output.
 * When providing a pre-abstraction, the handle will first apply the abstraction to the given belief before computing the successors of each belief resulting
 * from that abstraction When providing a post-abstraction, the handle will abstract each computed successor and will invoke the discover callback function on
 * any abstracted successor.
 *
 * Further notes on the discover callback:
 * The discover callback is invoked for each calculated (and potentially abstracted) successor belief, using the resulting successor belief and the transition
 * probability to it as arguments (both by rvalue reference). If there is a pre- and/or post-abstraction, the transition probability with which the callback is
 * invoked will be multiplied by the "abstraction weights", i.e., the corresponding probability of the distribution returned by the abstraction.
 *
 * Example:
 * Assume `b` is the current belief and in the belief MDP there would be a transition to a successor belief `c` with probability 0.3 and a transition
 * to a successor belief `d` with probability 0.7. Furthermore, suppose that the handle was configured with a post abstraction that, given `c`, returns the
 * distribution `{0.4: c_1, 0.6: c_2}` and, given `d` returns the distribution `{0.1: d_1, 0.9: d_2}` . Then the discover callback would be invoked four times
 * with the arguments `(c_1, 0.12)`, `(c_2, 0.18)`, `(d_1, 0.07)`, and `(d_2, 0.63)`.
 *
 * Further notes on the abstractions:
 * A pre-abstraction gets as input the current belief (by const reference) and the executed action.
 * A typical application for a pre-abstraction would be to incorporate observations based on a given (belief, action) pair.
 * A post-abstraction gets as input a successor belief and the transition probability to it (both by rvalue reference).
 * A typical application for a post-abstraction would be to discretize the found successor beliefs.
 * For efficiency considerations, the  distributions over abstracted beliefs calculated by the abstractions are not explicitly constructed. Instead, yet another
 * callback is used. For example, instead of returning a map-like object for `{0.1: d_1, 0.9: d_2}`, the
 * abstraction-callback will be invoked twice with the arguments `(d_1, 0.1)` and `(d_2, 0.9)`, respectively.
 *
 * Passing further information to the discover-callback:
 * To forward information to the discover-callback, additional arguments can be provided
 * - when invoking the handle, and
 * - by the pre- and/or post-abstraction when they call the abstraction-callback.
 * This is implemented using parameter packs / variadic templates.
 * The signature of the discover callback must then  consider those additional arguments: first those from the handle invocation, then those from the
 * pre-abstraction, and then those from the post-abstraction.
 *
 * @tparam PomdpType The type of the POMDP model
 * @tparam BeliefType The type of the beliefs of the POMDP
 */
template<typename PomdpType, typename BeliefType>
class FirstStateNextStateGenerator {
   public:
    template<typename PreAbstractionType, typename PostAbstractionType, typename DiscoverCallbackType>
    using Handle = detail::NextStateGeneratorHandle<PomdpType, BeliefType, PreAbstractionType, PostAbstractionType, DiscoverCallbackType>;

    /** Creates a successor generator for @p pomdp. The POMDP must outlive the generator and all of its handles. */
    FirstStateNextStateGenerator(PomdpType const& pomdp);

    /** Selects the reward model whose expected action rewards are queried; an empty name selects the default model. */
    void setRewardModel(std::string const& rewardModelName = "");
    /** @return whether a reward model is currently selected. */
    bool hasRewardModel() const;
    void unsetRewardModel();

    /** @return the initial belief induced by the POMDP's initial-state distribution. */
    BeliefType computeInitialBelief() const;

    /** @return the number of observation-compatible actions in @p belief. */
    uint64_t getBeliefNumberOfActions(BeliefType const& belief) const;

    /** @return the expected selected-model reward of the local action at @p belief. */
    typename PomdpType::ValueType getBeliefActionReward(BeliefType const& belief, uint64_t const& localActionIndex) const;

    /** @return labels of the POMDP action represented by @p localActionIndex at @p belief. */
    std::set<std::string> getBeliefActionChoiceLabels(BeliefType const& belief, uint64_t const& localActionIndex) const;

    /** Returns a handle that generates unabstracted successor beliefs. */
    template<typename DiscoverCallbackType>
    auto getHandle(DiscoverCallbackType& discoverCallback) {
        return Handle<NoAbstractionType const, NoAbstractionType const, DiscoverCallbackType>{this->pomdp, NoAbstraction, NoAbstraction, discoverCallback};
    }

    /** Returns a handle that applies @p preAbstraction before successor generation. */
    template<typename PreAbstractionType, typename DiscoverCallbackType>
    auto getPreAbstractionHandle(PreAbstractionType& preAbstraction, DiscoverCallbackType& discoverCallback) {
        return Handle<PreAbstractionType, NoAbstractionType const, DiscoverCallbackType>{this->pomdp, preAbstraction, NoAbstraction, discoverCallback};
    }

    /** Returns a handle that applies @p postAbstraction to each successor belief. */
    template<typename PostAbstractionType, typename DiscoverCallbackType>
    auto getPostAbstractionHandle(PostAbstractionType& postAbstraction, DiscoverCallbackType& discoverCallback) {
        return Handle<NoAbstractionType const, PostAbstractionType, DiscoverCallbackType>{this->pomdp, NoAbstraction, postAbstraction, discoverCallback};
    }

    /** Returns a handle that applies both abstractions around successor generation. */
    template<typename PreAbstractionType, typename PostAbstractionType, typename DiscoverCallbackType>
    auto getPrePostAbstractionHandle(PreAbstractionType& preAbstraction, PostAbstractionType& postAbstraction, DiscoverCallbackType& discoverCallback) {
        return Handle<PreAbstractionType, PostAbstractionType, DiscoverCallbackType>{this->pomdp, preAbstraction, postAbstraction, discoverCallback};
    }

   private:
    PomdpType const& pomdp;
    std::vector<typename PomdpType::ValueType> actionRewards;
};

namespace detail {

/*!
 * Implementation of the NextStateGeneratorHandle
 * @note as this is heavily templated, the implementation is intentionally put in the header file.
 */
template<typename PomdpType, typename BeliefType, typename PreAbstractionType, typename PostAbstractionType, typename DiscoverCallback>
struct NextStateGeneratorHandle {
    using BeliefValueType = typename BeliefType::ValueType;

    template<typename... CallBackArgs>
    /** Generates all observation-distinguished successors for one belief/action pair. */
    void operator()(BeliefType const& belief, uint64_t localActionIndex, CallBackArgs const&... additionalCallbackArgs) {
        applyPreAbstraction(belief, localActionIndex, std::forward<CallBackArgs const>(additionalCallbackArgs)...);
    }

    PomdpType const& pomdp;
    PreAbstractionType& preAbstraction;
    PostAbstractionType& postAbstraction;
    DiscoverCallback& discoverCallback;

   private:
    template<typename... CallBackArgs>
    void applyPreAbstraction(BeliefType const& belief, uint64_t localActionIndex, CallBackArgs const&... additionalCallbackArgs) {
        if constexpr (isNoAbstraction<PreAbstractionType>) {
            computeSuccessorBeliefs(belief, localActionIndex, storm::utility::one<BeliefValueType>(), DefaultActionObservation, additionalCallbackArgs...);
        } else {
            preAbstraction.abstract(
                belief, localActionIndex,
                [this, &localActionIndex, &additionalCallbackArgs...](
                    BeliefType&& preBel, BeliefValueType&& preVal, BeliefActionObservationType actionObservation, auto const&... additionalPreAbstractionArgs) {
                    this->computeSuccessorBeliefs(preBel, localActionIndex, std::move(preVal), actionObservation,
                                                  std::forward<CallBackArgs const>(additionalCallbackArgs)...,
                                                  std::forward<decltype(additionalPreAbstractionArgs)>(additionalPreAbstractionArgs)...);
                });
        }
    }

    /*!
     * @return the probability we go to each observation when starting in the given belief and performing the given action
     */
    std::unordered_map<BeliefObservationType, BeliefValueType> computeSuccessorObservations(BeliefType const& belief, uint64_t localActionIndex,
                                                                                            BeliefActionObservationType actionObservation) {
        std::unordered_map<BeliefObservationType, BeliefValueType> successorObservations;
        belief.forEach([&localActionIndex, &successorObservations, &actionObservation, this](BeliefStateType const& state, BeliefValueType const& beliefValue) {
            for (auto const& pomdpTransition : pomdp.getTransitionMatrix().getRow(state, localActionIndex)) {
                if (!storm::utility::isZero(pomdpTransition.getValue())) {
                    auto const obs = (pomdp.getNrObservations() * actionObservation) + pomdp.getObservation(pomdpTransition.getColumn());
                    BeliefValueType const val = beliefValue * storm::utility::convertNumber<BeliefValueType>(pomdpTransition.getValue());
                    if (auto [insertionIt, inserted] = successorObservations.emplace(obs, val); !inserted) {
                        insertionIt->second += val;
                    }
                }
            }
        });

        // Adjust the distribution to diminish numerical inaccuracies a bit
        if constexpr (!storm::NumberTraits<BeliefValueType>::IsExact || !storm::NumberTraits<typename PomdpType::ValueType>::IsExact) {
            if (successorObservations.size() == 1) {
                successorObservations.begin()->second = storm::utility::one<BeliefValueType>();
            }
        }
        return successorObservations;
    }

    template<typename... CallBackArgs>
    void computeSuccessorBeliefs(BeliefType const& belief, uint64_t localActionIndex, BeliefValueType const& transitionProbability,
                                 BeliefActionObservationType actionObservation, CallBackArgs const&... additionalCallbackArgs) {
        // For each successor observation we build the successor belief
        auto const successorObservations = computeSuccessorObservations(belief, localActionIndex, actionObservation);
        for (auto const& successorObsValue : successorObservations) {
            BeliefBuilder<BeliefType> builder;
            builder.setObservation(successorObsValue.first);
            belief.forEach([&builder, &localActionIndex, &successorObsValue, this](BeliefStateType const& state, BeliefValueType const& beliefValue) {
                for (auto const& pomdpTransition : pomdp.getTransitionMatrix().getRow(state, localActionIndex)) {
                    if (pomdp.getObservation(pomdpTransition.getColumn()) == (successorObsValue.first % pomdp.getNrObservations())) {
                        BeliefValueType const prob =
                            beliefValue * storm::utility::convertNumber<BeliefValueType>(pomdpTransition.getValue()) / successorObsValue.second;
                        STORM_LOG_ASSERT(prob > storm::utility::zero<BeliefValueType>(), "Invalid belief probability " << prob << ".");
                        builder.addValue(pomdpTransition.getColumn(), prob);
                    }
                }
            });
            applyPostAbstraction(builder.build(), static_cast<BeliefValueType>(successorObsValue.second * transitionProbability),
                                 std::forward<CallBackArgs const>(additionalCallbackArgs)...);
        }
    }

    template<typename... CallBackArgs>
    void applyPostAbstraction(BeliefType&& belief, BeliefValueType&& transitionProbability, CallBackArgs const&... additionalCallbackArgs) {
        if constexpr (isNoAbstraction<PostAbstractionType>) {
            discoverCallback(std::move(belief), std::move(transitionProbability), additionalCallbackArgs...);
        } else {
            postAbstraction.abstract(
                std::move(belief), std::move(transitionProbability),
                [this, &additionalCallbackArgs...](BeliefType&& postBel, BeliefValueType&& postVal, auto&&... additionalPostAbstractionArgs) {
                    discoverCallback(std::move(postBel), std::move(postVal), std::forward<CallBackArgs const>(additionalCallbackArgs)...,
                                     std::forward<decltype(additionalPostAbstractionArgs)>(additionalPostAbstractionArgs)...);
                });
        }
    }
};

}  // namespace detail
}  // namespace storm::pomdp::beliefs
