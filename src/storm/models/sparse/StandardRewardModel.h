#pragma once

#include <optional>
#include <set>
#include <vector>

#include "storm/adapters/RationalFunctionForward.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/OptionalRef.h"
#include "storm/utility/constants.h"

namespace storm {
namespace models {
namespace sparse {
template<typename CValueType>
class StandardRewardModel {
   public:
    typedef CValueType ValueType;

    /*!
     * Constructs a reward model by copying the given data.
     *
     * @param optionalStateRewardVector The reward values associated with the states.
     * @param optionalStateActionRewardVector The reward values associated with state-action pairs.
     * @param optionalTransitionRewardMatrix The reward values associated with the transitions of the model.
     */
    StandardRewardModel(std::optional<std::vector<ValueType>> const& optionalStateRewardVector = std::nullopt,
                        std::optional<std::vector<ValueType>> const& optionalStateActionRewardVector = std::nullopt,
                        std::optional<storm::storage::SparseMatrix<ValueType>> const& optionalTransitionRewardMatrix = std::nullopt);

    /*!
     * Constructs a reward model by moving the given data.
     *
     * @param optionalStateRewardVector The reward values associated with the states.
     * @param optionalStateActionRewardVector The reward values associated with state-action pairs.
     * @param optionalTransitionRewardMatrix The reward values associated with the transitions of the model.
     */
    StandardRewardModel(std::optional<std::vector<ValueType>>&& optionalStateRewardVector,
                        std::optional<std::vector<ValueType>>&& optionalStateActionRewardVector = std::nullopt,
                        std::optional<storm::storage::SparseMatrix<ValueType>>&& optionalTransitionRewardMatrix = std::nullopt);

    StandardRewardModel(StandardRewardModel<ValueType> const& other) = default;
    StandardRewardModel& operator=(StandardRewardModel<ValueType> const& other) = default;

    StandardRewardModel(StandardRewardModel<ValueType>&& other) = default;
    StandardRewardModel& operator=(StandardRewardModel<ValueType>&& other) = default;

    /*!
     * Retrieves whether the reward model has state rewards.
     *
     * @return True iff the reward model has state rewards.
     */
    bool hasStateRewards() const;

    /*!
     * Retrieves whether the reward model only has state rewards (and hence no other rewards).
     *
     * @return True iff the reward model only has state rewards.
     */
    bool hasOnlyStateRewards() const;

    /*!
     * Retrieves the state rewards of the reward model. Note that it is illegal to call this function if the
     * reward model does not have state rewards.
     *
     * @return The state reward vector.
     */
    std::vector<ValueType> const& getStateRewardVector() const;

    /*!
     * Retrieves the state rewards of the reward model. Note that it is illegal to call this function if the
     * reward model does not have state rewards.
     *
     * @return The state reward vector.
     */
    std::vector<ValueType>& getStateRewardVector();

    ValueType const& getStateReward(uint_fast64_t state) const;

    template<typename T>
    void setStateReward(uint_fast64_t state, T const& newReward);

    // template<typename T=ValueType, EnableIf<hasTotalOrder<T>>>
    // ValueType maximalStateReward(uint_fast64_t state) const;

    /*!
     * Retrieves an optional value that contains the state reward vector if there is one.
     *
     * @return The state reward vector if there is one.
     */
    std::optional<std::vector<ValueType>> const& getOptionalStateRewardVector() const;

    /*!
     * Retrieves whether the reward model has state-action rewards.
     *
     * @return True iff the reward model has state-action rewards.
     */
    bool hasStateActionRewards() const;

    /*!
     * Retrieves the state-action rewards of the reward model. Note that it is illegal to call this function
     * if the reward model does not have state-action rewards.
     *
     * @return The state-action reward vector.
     */
    std::vector<ValueType> const& getStateActionRewardVector() const;

    /*!
     * Retrieves the state-action rewards of the reward model. Note that it is illegal to call this function
     * if the reward model does not have state-action rewards.
     *
     * @return The state-action reward vector.
     */
    std::vector<ValueType>& getStateActionRewardVector();

    /*!
     * Retrieves the state-action reward for the given choice.
     */
    ValueType const& getStateActionReward(uint_fast64_t choiceIndex) const;

    /*!
     * Sets the state-action reward for the given choice
     */
    template<typename T>
    void setStateActionReward(uint_fast64_t choiceIndex, T const& newValue);

    /*!
     * Sets all available rewards at this state to zero.
     */
    template<typename MatrixValueType>
    void clearRewardAtState(uint_fast64_t state, storm::storage::SparseMatrix<MatrixValueType> const& transitions);

    /*!
     * Retrieves an optional value that contains the state-action reward vector if there is one.
     *
     * @return The state-action reward vector if there is one.
     */
    std::optional<std::vector<ValueType>> const& getOptionalStateActionRewardVector() const;

    /*!
     * Retrieves whether the reward model has transition rewards.
     *
     * @return True iff the reward model has transition rewards.
     */
    bool hasTransitionRewards() const;

    /*!
     * Retrieves the transition rewards of the reward model. Note that it is illegal to call this function
     * if the reward model does not have transition rewards.
     *
     * @return The transition reward matrix.
     */
    storm::storage::SparseMatrix<ValueType> const& getTransitionRewardMatrix() const;

    /*!
     * Retrieves the transition rewards of the reward model. Note that it is illegal to call this function
     * if the reward model does not have transition rewards.
     *
     * @return The transition reward matrix.
     */
    storm::storage::SparseMatrix<ValueType>& getTransitionRewardMatrix();

    /*!
     * Retrieves an optional value that contains the transition reward matrix if there is one.
     *
     * @return The transition reward matrix if there is one.
     */
    std::optional<storm::storage::SparseMatrix<ValueType>> const& getOptionalTransitionRewardMatrix() const;

    /*!
     * @param choiceIndex The index of the considered choice
     * @param transitionMatrix The matrix that is used to weight the values of the transition reward matrix.
     * @return the sum of the action reward and the weighted transition rewards for the given choice, excluding potential state rewards
     * @note returns zero if there is neither action nor transition reward.
     */
    template<typename MatrixValueType>
    ValueType getStateActionAndTransitionReward(uint_fast64_t choiceIndex, storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix) const;

    /*!
     * Retrieves the total reward for the given state action pair (including (scaled) state rewards, action rewards and transition rewards
     *
     * @param stateIndex The index of the considered state
     * @param choiceIndex The index of the considered choice
     * @param transitionMatrix The matrix that is used to weight the values of the transition reward matrix.
     * @param stateRewardWeight The weight that is used to scale the state reward
     * @param actionRewardWeight The weight that is used to scale the action based rewards (includes action and transition rewards).
     * @return
     */
    template<typename MatrixValueType>
    ValueType getTotalStateActionReward(uint_fast64_t stateIndex, uint_fast64_t choiceIndex,
                                        storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix,
                                        MatrixValueType const& stateRewardWeight = storm::utility::one<MatrixValueType>(),
                                        MatrixValueType const& actionRewardWeight = storm::utility::one<MatrixValueType>()) const;

    /*!
     * Creates a new reward model by restricting the actions of the action-based rewards.
     *
     * @param enabledActions A bit vector representing the enabled actions.
     * @return The restricted reward model.
     */
    StandardRewardModel<ValueType> restrictActions(storm::storage::BitVector const& enabledActions) const;

    /*!
     * Creates a new reward model by permuting the actions.
     * That is, in row i, write the action reward of row inversePermutation[i].
     *
     */
    StandardRewardModel<ValueType> permuteActions(std::vector<uint64_t> const& inversePermutation) const;

    /*!
     * Creates a new reward model by permuting the states.
     * That is, assign to state i, the rewards previously assigned to state inversePermutation[i].
     * @param inversePermutation The inverse permutation that is used to permute the states.
     * @param rowGroupIndices must be given to respect grouped action rewards. If they are not given, it is assumed that each state has exactly one action
     * @param permutation The (non-inverted) permutation. Relevant only if transition rewards are present.
     */
    StandardRewardModel<ValueType> permuteStates(std::vector<uint64_t> const& inversePermutation,
                                                 storm::OptionalRef<std::vector<uint64_t> const> rowGroupIndices = storm::NullRef,
                                                 storm::OptionalRef<std::vector<uint64_t> const> permutation = storm::NullRef) const;

    /*!
     * Reduces the transition-based rewards to state-action rewards by taking the average of each row. If
     * the corresponding flag is set, the state-action rewards and the state rewards are summed so the model
     * only has a state reward vector left. Note that this transformation only  preserves expected rewards,
     * but not all reward-based properties.
     *
     * @param transitionMatrix The transition matrix that is used to weight the rewards in the reward matrix.
     * @param reduceToStateRewards If set, the state-action rewards and the state rewards are summed so the
     * model only has a state reward vector left.
     * @param weights If given and if the reduction to state rewards only is enabled, this vector is used to
     * weight the state-action and transition rewards
     */
    template<typename MatrixValueType>
    void reduceToStateBasedRewards(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix, bool reduceToStateRewards = false,
                                   std::vector<MatrixValueType> const* weights = nullptr);

    /*!
     * Creates a vector representing the complete reward vector based on the state-, state-action- and
     * transition-based rewards in the reward model.
     *
     * @param transitionMatrix The matrix that is used to weight the values of the transition reward matrix.
     * @return The full state-action reward vector.
     */
    template<typename MatrixValueType>
    std::vector<ValueType> getTotalRewardVector(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix) const;

    /*!
     * Creates a vector representing the complete reward vector based on the state-, state-action- and
     * transition-based rewards in the reward model.
     *
     * @param transitionMatrix The matrix that is used to weight the values of the transition reward matrix.
     * @param weights A vector used for scaling the entries of transition and/or state-action rewards (if present).
     * @return The full state-action reward vector.
     */
    template<typename MatrixValueType>
    std::vector<ValueType> getTotalRewardVector(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix,
                                                std::vector<MatrixValueType> const& weights) const;

    /*!
     * Creates a vector representing the complete reward vector based on the state-, state-action- and
     * transition-based rewards in the reward model.
     *
     * @param transitionMatrix The matrix that is used to weight the values of the transition reward matrix.
     * @param numberOfRows The total number of rows of the resulting vector.
     * @param filter A bit vector indicating which row groups to select.
     * @return The full state-action reward vector.
     */
    template<typename MatrixValueType>
    std::vector<ValueType> getTotalRewardVector(uint_fast64_t numberOfRows, storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix,
                                                storm::storage::BitVector const& filter) const;

    /*!
     * Creates a vector representing the complete action-based rewards, i.e., state-action- and
     * transition-based rewards
     *
     * @param numberOfRows The total number of rows of the resulting vector.
     * @param transitionMatrix The matrix that is used to weight the values of the transition reward matrix.
     * @return The state-action reward vector that considers state-action rewards and transition rewards of this reward model.
     */

    template<typename MatrixValueType>
    std::vector<ValueType> getTotalActionRewardVector(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix,
                                                      std::vector<MatrixValueType> const& stateRewardWeights) const;

    /*!
     * Returns the set of states at which a all rewards (state-, action- and transition-rewards) are zero.
     *
     * @param transitionMatrix the transition matrix of the model (used to determine the actions and transitions that belong to a state)
     * @ return a vector representing all states at which the reward is zero
     */
    template<typename MatrixValueType>
    storm::storage::BitVector getStatesWithZeroReward(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix) const;

    /*!
     * Returns the set of states for which all associated rewards (state, action or transition rewards) satisfy the given filter.
     * @param transitionMatrix the transition matrix of the model (used to determine the actions and transitions that belong to a state)
     */
    template<typename MatrixValueType>
    storm::storage::BitVector getStatesWithFilter(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix,
                                                  std::function<bool(ValueType const&)> const& filter) const;

    /*!
     * Returns the set of choices at which all rewards (state-, action- and transition-rewards) are zero.
     *
     * @param transitionMatrix the transition matrix of the model (used to determine the state that belongs to a choice
     * @ return a vector representing all choices at which the reward is zero
     */
    template<typename MatrixValueType>
    storm::storage::BitVector getChoicesWithZeroReward(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix) const;

    /*!
     * Returns the set of choices for which all associated rewards (state, action or transition rewards) satisfy the given filter.
     * @param transitionMatrix the transition matrix of the model (used to determine the actions and transitions that belong to a state)
     */
    template<typename MatrixValueType>
    storm::storage::BitVector getChoicesWithFilter(storm::storage::SparseMatrix<MatrixValueType> const& transitionMatrix,
                                                   std::function<bool(ValueType const&)> const& filter) const;

    /*!
     * Retrieves whether the reward model is empty, i.e. contains no state-, state-action- or transition-based
     * rewards.
     *
     * @return True iff the reward model is empty.
     */
    bool empty() const;

    /*!
     * Retrieves whether every reward defined by this reward model is zero
     *
     * @return True iff every reward defined by this reward model is zero.
     */
    bool isAllZero() const;

    /*!
     * @return Whether the reward model has at least one negative reward value
     */
    bool hasNegativeRewards() const;

    /*!
     * @return Whether the reward model has at least one positive reward value
     */
    bool hasPositiveRewards() const;

    /*!
     * Checks whether the reward model is compatible with key model characteristics.
     *
     * In detail, the method checks
     * - if the state-rewards exist, whether the given number of states corresponds to the size of the vector.
     * - if the state-action-rewards exist, whether the given number of choices corresponds to the size of the vector.
     *
     * @param nrStates The number of states in the model
     * @param nrChoices The number of choices in the model
     */
    bool isCompatible(uint_fast64_t nrStates, uint_fast64_t nrChoices) const;

    std::size_t hash() const;

    template<typename ValueTypePrime>
    friend std::ostream& operator<<(std::ostream& out, StandardRewardModel<ValueTypePrime> const& rewardModel);

   private:
    // An (optional) vector representing the state rewards.
    std::optional<std::vector<ValueType>> optionalStateRewardVector;

    // An (optional) vector representing the state-action rewards.
    std::optional<std::vector<ValueType>> optionalStateActionRewardVector;

    // An (optional) matrix representing the transition rewards.
    std::optional<storm::storage::SparseMatrix<ValueType>> optionalTransitionRewardMatrix;
};

template<typename ValueType>
std::ostream& operator<<(std::ostream& out, StandardRewardModel<ValueType> const& rewardModel);

std::set<storm::RationalFunctionVariable> getRewardModelParameters(StandardRewardModel<storm::RationalFunction> const& rewModel);
}  // namespace sparse
}  // namespace models
}  // namespace storm
