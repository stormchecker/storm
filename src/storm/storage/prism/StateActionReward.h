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
class StateActionReward : public LocatedInformation {
   public:
    /*!
     * Creates a transition reward for the transitions with the given name emanating from states satisfying the
     * given expression with the value given by another expression.
     *
     * @param actionIndex The index of the action.
     * @param actionName The name of the command that obtains this reward.
     * @param statePredicateExpression The predicate that needs to hold before taking a transition with the previously
     * specified name in order to obtain the reward.
     * @param rewardValueExpression An expression specifying the values of the rewards to attach to the transitions.
     * @param filename The filename in which the transition reward is defined.
     * @param lineNumber The line number in which the transition reward is defined.
     */
    StateActionReward(uint_fast64_t actionIndex, std::string const& actionName, storm::expressions::Expression const& statePredicateExpression,
                      storm::expressions::Expression const& rewardValueExpression, std::string const& filename = "", uint_fast64_t lineNumber = 0);

    // Create default implementations of constructors/assignment.
    StateActionReward() = default;
    StateActionReward(StateActionReward const& other) = default;
    StateActionReward& operator=(StateActionReward const& other) = default;
    StateActionReward(StateActionReward&& other) = default;
    StateActionReward& operator=(StateActionReward&& other) = default;

    /*!
     * Retrieves the action name that is associated with this transition reward.
     *
     * @return The action name that is associated with this transition reward.
     */
    std::string const& getActionName() const;

    /*!
     * Retrieves the action index of the action associated with this transition reward (if any).
     *
     * @return The action index of the transition reward.
     */
    uint_fast64_t getActionIndex() const;

    /*!
     * Retrieves the state predicate expression that is associated with this state reward.
     *
     * @return The state predicate expression that is associated with this state reward.
     */
    storm::expressions::Expression const& getStatePredicateExpression() const;

    /*!
     * Retrieves the reward value expression associated with this state reward.
     *
     * @return The reward value expression associated with this state reward.
     */
    storm::expressions::Expression const& getRewardValueExpression() const;

    /*!
     * Retrieves whether the transition reward has an action label.
     *
     * @return True iff the transition reward has an action label.
     */
    bool isLabeled() const;

    /*!
     * Substitutes all identifiers in the transition reward according to the given map.
     *
     * @param substitution The substitution to perform.
     * @return The resulting transition reward.
     */
    StateActionReward substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;

    friend std::ostream& operator<<(std::ostream& stream, StateActionReward const& stateActionReward);

   private:
    // The index of the action name.
    uint_fast64_t actionIndex;

    // The name of the command this transition-based reward is attached to.
    std::string actionName;

    // A flag that stores whether the transition reward has an action label.
    bool labeled;

    // A predicate that needs to be satisfied by states for the reward to be obtained (by taking
    // a corresponding command transition).
    storm::expressions::Expression statePredicateExpression;

    // The expression specifying the value of the reward obtained along the transitions.
    storm::expressions::Expression rewardValueExpression;
};

}  // namespace prism
}  // namespace storm
