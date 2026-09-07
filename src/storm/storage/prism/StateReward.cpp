#include "StateReward.h"

#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace prism {
StateReward::StateReward(storm::expressions::Expression const& statePredicateExpression, storm::expressions::Expression const& rewardValueExpression,
                         std::string const& filename, uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber), statePredicateExpression(statePredicateExpression), rewardValueExpression(rewardValueExpression) {
    // Nothing to do here.
}

storm::expressions::Expression const& StateReward::getStatePredicateExpression() const {
    return this->statePredicateExpression;
}

storm::expressions::Expression const& StateReward::getRewardValueExpression() const {
    return this->rewardValueExpression;
}

StateReward StateReward::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    return StateReward(this->getStatePredicateExpression().substitute(substitution), this->getRewardValueExpression().substitute(substitution),
                       this->getFilename(), this->getLineNumber());
}

std::ostream& operator<<(std::ostream& stream, StateReward const& stateReward) {
    stream << "\t" << stateReward.getStatePredicateExpression() << ": " << stateReward.getRewardValueExpression() << ";";
    return stream;
}
}  // namespace prism
}  // namespace storm
