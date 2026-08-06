#include "storm/logic/EventuallyFormula.h"
#include <boost/any.hpp>
#include <ostream>
#include "storm/logic/FormulaVisitor.h"

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
EventuallyFormula::EventuallyFormula(std::shared_ptr<Formula const> const& subformula, FormulaContext context,
                                     boost::optional<RewardAccumulation> rewardAccumulation)
    : UnaryPathFormula(subformula), context(context), rewardAccumulation(rewardAccumulation) {
    STORM_LOG_THROW(context == FormulaContext::Probability || context == FormulaContext::Reward || context == FormulaContext::Time,
                    storm::exceptions::InvalidPropertyException, "Invalid context for formula.");
    STORM_LOG_THROW(context != FormulaContext::Probability || !rewardAccumulation.is_initialized(), storm::exceptions::InvalidPropertyException,
                    "Reward accumulations should only be given for time- and reward formulas.");
}

FormulaContext const& EventuallyFormula::getContext() const {
    return context;
}

bool EventuallyFormula::isEventuallyFormula() const {
    return true;
}

bool EventuallyFormula::isReachabilityProbabilityFormula() const {
    return context == FormulaContext::Probability;
}

bool EventuallyFormula::isReachabilityRewardFormula() const {
    return context == FormulaContext::Reward;
}

bool EventuallyFormula::isReachabilityTimeFormula() const {
    return context == FormulaContext::Time;
}

bool EventuallyFormula::isProbabilityPathFormula() const {
    return this->isReachabilityProbabilityFormula();
}

bool EventuallyFormula::isRewardPathFormula() const {
    return this->isReachabilityRewardFormula();
}

bool EventuallyFormula::isTimePathFormula() const {
    return this->isReachabilityTimeFormula();
}

bool EventuallyFormula::hasRewardAccumulation() const {
    return rewardAccumulation.is_initialized();
}

RewardAccumulation const& EventuallyFormula::getRewardAccumulation() const {
    return rewardAccumulation.get();
}

boost::any EventuallyFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

std::ostream& EventuallyFormula::writeToStream(std::ostream& out, bool allowParentheses) const {
    if (allowParentheses) {
        out << "(";
    }
    out << "F ";
    if (hasRewardAccumulation()) {
        out << "[" << getRewardAccumulation() << "]";
    }
    this->getSubformula().writeToStream(out, !this->getSubformula().isUnaryFormula());
    if (allowParentheses) {
        out << ")";
    }
    return out;
}
}  // namespace logic
}  // namespace storm
