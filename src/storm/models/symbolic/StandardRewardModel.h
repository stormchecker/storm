#pragma once

#include <boost/optional.hpp>
#include <set>

#include "storm/storage/dd/DdType.h"

namespace storm {
namespace dd {
template<storm::dd::DdType Type, typename ValueType>
class Add;

template<storm::dd::DdType Type>
class Bdd;
}  // namespace dd

namespace expressions {
class Variable;
}

namespace models {
namespace symbolic {

template<storm::dd::DdType Type, typename ValueType>
class StandardRewardModel {
   public:
    /*!
     * Builds a reward model by copying with the given reward structures.
     *
     * @param stateRewardVector The state reward vector.
     * @param stateActionRewardVector The vector of state-action rewards.
     * @param transitionRewardMatrix The matrix of transition rewards.
     */
    explicit StandardRewardModel(boost::optional<storm::dd::Add<Type, ValueType>> const& stateRewardVector,
                                 boost::optional<storm::dd::Add<Type, ValueType>> const& stateActionRewardVector,
                                 boost::optional<storm::dd::Add<Type, ValueType>> const& transitionRewardMatrix);

    /*!
     * Retrieves whether the reward model is empty.
     *
     * @return True iff the reward model is empty.
     */
    bool empty() const;

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
    storm::dd::Add<Type, ValueType> const& getStateRewardVector() const;

    /*!
     * Retrieves the state rewards of the reward model. Note that it is illegal to call this function if the
     * reward model does not have state rewards.
     *
     * @return The state reward vector.
     */
    storm::dd::Add<Type, ValueType>& getStateRewardVector();

    /*!
     * Retrieves an optional value that contains the state reward vector if there is one.
     *
     * @return The state reward vector if there is one.
     */
    boost::optional<storm::dd::Add<Type, ValueType>> const& getOptionalStateRewardVector() const;

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
    storm::dd::Add<Type, ValueType> const& getStateActionRewardVector() const;

    /*!
     * Retrieves the state-action rewards of the reward model. Note that it is illegal to call this function
     * if the reward model does not have state-action rewards.
     *
     * @return The state-action reward vector.
     */
    storm::dd::Add<Type, ValueType>& getStateActionRewardVector();

    /*!
     * Retrieves an optional value that contains the state-action reward vector if there is one.
     *
     * @return The state-action reward vector if there is one.
     */
    boost::optional<storm::dd::Add<Type, ValueType>> const& getOptionalStateActionRewardVector() const;

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
    storm::dd::Add<Type, ValueType> const& getTransitionRewardMatrix() const;

    /*!
     * Retrieves the transition rewards of the reward model. Note that it is illegal to call this function
     * if the reward model does not have transition rewards.
     *
     * @return The transition reward matrix.
     */
    storm::dd::Add<Type, ValueType>& getTransitionRewardMatrix();

    /*!
     * Retrieves an optional value that contains the transition reward matrix if there is one.
     *
     * @return The transition reward matrix if there is one.
     */
    boost::optional<storm::dd::Add<Type, ValueType>> const& getOptionalTransitionRewardMatrix() const;

    /*!
     * Creates a vector representing the complete reward vector based on the state-, state-action- and
     * transition-based rewards in the reward model.
     *
     * @param transitionMatrix The matrix that is used to weight the values of the transition reward matrix.
     * @param columnVariables The column variables of the model.
     * @return The full state-action reward vector.
     */
    storm::dd::Add<Type, ValueType> getTotalRewardVector(storm::dd::Add<Type, ValueType> const& transitionMatrix,
                                                         std::set<storm::expressions::Variable> const& columnVariables) const;

    /*!
     * Creates a vector representing the complete reward vector based on the state-, state-action- and
     * transition-based rewards in the reward model. The state- and state-action rewards are filtered with
     * the given filter DD.
     *
     * @param filterAdd The DD used for filtering the rewards.
     * @param transitionMatrix The matrix that is used to weight the values of the transition reward matrix.
     * @param columnVariables The column variables of the model.
     * @return The full state-action reward vector.
     */
    storm::dd::Add<Type, ValueType> getTotalRewardVector(storm::dd::Add<Type, ValueType> const& filterAdd,
                                                         storm::dd::Add<Type, ValueType> const& transitionMatrix,
                                                         std::set<storm::expressions::Variable> const& columnVariables) const;

    /*!
     * Creates a vector representing the complete reward vector based on the state-, state-action- and
     * transition-based rewards in the reward model. The state- and state-action rewards are filtered with
     * the given filter DD.
     *
     * @param stateFilterAdd The DD used for filtering the state rewards.
     * @param choiceFilterAdd The DD used for filtering the state action rewards.
     * @param transitionMatrix The matrix that is used to weight the values of the transition reward matrix.
     * @param columnVariables The column variables of the model.
     * @return The full state-action reward vector.
     */
    storm::dd::Add<Type, ValueType> getTotalRewardVector(storm::dd::Add<Type, ValueType> const& stateFilterAdd,
                                                         storm::dd::Add<Type, ValueType> const& choiceFilterAdd,
                                                         storm::dd::Add<Type, ValueType> const& transitionMatrix,
                                                         std::set<storm::expressions::Variable> const& columnVariables) const;

    /*!
     * Creates a vector representing the complete reward vector based on the state-, state-action- and
     * transition-based rewards in the reward model.
     *
     * @param transitionMatrix The matrix that is used to weight the values of the transition reward matrix.
     * @param columnVariables The column variables of the model.
     * @param weights The weights used to scale the transition rewards and/or state-action rewards.
     * @param scaleTransAndActions If true both transition rewards and state-action rewards are scaled by the
     * weights. Otherwise, only the state-action rewards are scaled.
     * @return The full state-action reward vector.
     */
    storm::dd::Add<Type, ValueType> getTotalRewardVector(storm::dd::Add<Type, ValueType> const& transitionMatrix,
                                                         std::set<storm::expressions::Variable> const& columnVariables,
                                                         storm::dd::Add<Type, ValueType> const& weights, bool scaleTransAndActions) const;

    /*!
     * Multiplies all components of the reward model with the given DD.
     *
     * @param filter The filter with which to multiply
     */
    StandardRewardModel<Type, ValueType>& operator*=(storm::dd::Add<Type, ValueType> const& filter);

    /*!
     * Divides the state reward vector of the reward model by the given divisor.
     *
     * @param divisor The vector to divide by.
     * @return The resulting reward model.
     */
    StandardRewardModel<Type, ValueType> divideStateRewardVector(storm::dd::Add<Type, ValueType> const& divisor) const;

    /*!
     * Reduces the transition-based rewards to state-action rewards by taking the average of each row. If
     * the corresponding flag is set, the state-action rewards and the state rewards are summed so the model
     * only has a state reward vector left. Note that this transformation only  preserves expected rewards,
     * but not all reward-based properties.
     *
     * @param transitionMatrix The transition matrix that is used to weight the rewards in the reward matrix.
     */
    void reduceToStateBasedRewards(storm::dd::Add<Type, ValueType> const& transitionMatrix, std::set<storm::expressions::Variable> const& rowVariables,
                                   std::set<storm::expressions::Variable> const& columnVariables, bool reduceToStateRewards);

    template<typename NewValueType>
    StandardRewardModel<Type, NewValueType> toValueType() const;

   private:
    // The state reward vector.
    boost::optional<storm::dd::Add<Type, ValueType>> optionalStateRewardVector;

    // A vector of state-action-based rewards.
    boost::optional<storm::dd::Add<Type, ValueType>> optionalStateActionRewardVector;

    // A matrix of transition rewards.
    boost::optional<storm::dd::Add<Type, ValueType>> optionalTransitionRewardMatrix;
};

}  // namespace symbolic
}  // namespace models
}  // namespace storm
