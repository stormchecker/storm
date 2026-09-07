#include "TransitionReward.h"

#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace prism {
TransitionReward::TransitionReward(uint_fast64_t actionIndex, std::string const& actionName,
                                   storm::expressions::Expression const& sourceStatePredicateExpression,
                                   storm::expressions::Expression const& targetStatePredicateExpression,
                                   storm::expressions::Expression const& rewardValueExpression, std::string const& filename, uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber),
      actionIndex(actionIndex),
      actionName(actionName),
      labeled(actionName != ""),
      sourceStatePredicateExpression(sourceStatePredicateExpression),
      targetStatePredicateExpression(targetStatePredicateExpression),
      rewardValueExpression(rewardValueExpression) {
    // Nothing to do here.
}

std::string const& TransitionReward::getActionName() const {
    return this->actionName;
}

uint_fast64_t TransitionReward::getActionIndex() const {
    return this->actionIndex;
}

storm::expressions::Expression const& TransitionReward::getSourceStatePredicateExpression() const {
    return this->sourceStatePredicateExpression;
}

storm::expressions::Expression const& TransitionReward::getTargetStatePredicateExpression() const {
    return this->targetStatePredicateExpression;
}

storm::expressions::Expression const& TransitionReward::getRewardValueExpression() const {
    return this->rewardValueExpression;
}

bool TransitionReward::isLabeled() const {
    return labeled;
}

TransitionReward TransitionReward::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    return TransitionReward(this->getActionIndex(), this->getActionName(), this->getSourceStatePredicateExpression().substitute(substitution),
                            this->getTargetStatePredicateExpression().substitute(substitution), this->getRewardValueExpression().substitute(substitution),
                            this->getFilename(), this->getLineNumber());
}

std::ostream& operator<<(std::ostream& stream, TransitionReward const& transitionReward) {
    stream << "\t[" << transitionReward.getActionName() << "] " << transitionReward.getSourceStatePredicateExpression() << " -> "
           << transitionReward.getTargetStatePredicateExpression() << ": " << transitionReward.getRewardValueExpression() << ";";
    return stream;
}

}  // namespace prism
}  // namespace storm
