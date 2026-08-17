#pragma once

#include <map>

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/prism/LocatedInformation.h"

namespace storm {
namespace storage {
namespace expressions {
class Variable;
}
}  // namespace storage
}  // namespace storm

namespace storm {
namespace prism {
class StateReward : public LocatedInformation {
   public:
    /*!
     * Creates a state reward for the states satisfying the given expression with the value given by a second
     * expression.
     *
     * @param statePredicateExpression The predicate that states earning this state-based reward need to satisfy.
     * @param rewardValueExpression An expression specifying the values of the rewards to attach to the states.
     * @param filename The filename in which the state reward is defined.
     * @param lineNumber The line number in which the state reward is defined.
     */
    StateReward(storm::expressions::Expression const& statePredicateExpression, storm::expressions::Expression const& rewardValueExpression,
                std::string const& filename = "", uint_fast64_t lineNumber = 0);

    // Create default implementations of constructors/assignment.
    StateReward() = default;
    StateReward(StateReward const& other) = default;
    StateReward& operator=(StateReward const& other) = default;
    StateReward(StateReward&& other) = default;
    StateReward& operator=(StateReward&& other) = default;

    /*!
     * Retrieves the state predicate that is associated with this state reward.
     *
     * @return The state predicate that is associated with this state reward.
     */
    storm::expressions::Expression const& getStatePredicateExpression() const;

    /*!
     * Retrieves the reward value associated with this state reward.
     *
     * @return The reward value associated with this state reward.
     */
    storm::expressions::Expression const& getRewardValueExpression() const;

    /*!
     * Substitutes all identifiers in the state reward according to the given map.
     *
     * @param substitution The substitution to perform.
     * @return The resulting state reward.
     */
    StateReward substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;

    friend std::ostream& operator<<(std::ostream& stream, StateReward const& stateReward);

   private:
    // The predicate that characterizes the states that obtain this reward.
    storm::expressions::Expression statePredicateExpression;

    // The expression that specifies the value of the reward obtained.
    storm::expressions::Expression rewardValueExpression;
};

}  // namespace prism
}  // namespace storm
