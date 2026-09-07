#include "StateActionReward.h"

#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace prism {
StateActionReward::StateActionReward(uint_fast64_t actionIndex, std::string const& actionName, storm::expressions::Expression const& statePredicateExpression,
                                     storm::expressions::Expression const& rewardValueExpression, std::string const& filename, uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber),
      actionIndex(actionIndex),
      actionName(actionName),
      labeled(actionName != ""),
      statePredicateExpression(statePredicateExpression),
      rewardValueExpression(rewardValueExpression) {
    // Nothing to do here.
}

std::string const& StateActionReward::getActionName() const {
    return this->actionName;
}

uint_fast64_t StateActionReward::getActionIndex() const {
    return this->actionIndex;
}

storm::expressions::Expression const& StateActionReward::getStatePredicateExpression() const {
    return this->statePredicateExpression;
}

storm::expressions::Expression const& StateActionReward::getRewardValueExpression() const {
    return this->rewardValueExpression;
}

bool StateActionReward::isLabeled() const {
    return labeled;
}

StateActionReward StateActionReward::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    return StateActionReward(this->getActionIndex(), this->getActionName(), this->getStatePredicateExpression().substitute(substitution),
                             this->getRewardValueExpression().substitute(substitution), this->getFilename(), this->getLineNumber());
}

std::ostream& operator<<(std::ostream& stream, StateActionReward const& stateActionReward) {
    stream << "\t[" << stateActionReward.getActionName() << "] " << stateActionReward.getStatePredicateExpression() << ": "
           << stateActionReward.getRewardValueExpression() << ";";
    return stream;
}

}  // namespace prism
}  // namespace storm
