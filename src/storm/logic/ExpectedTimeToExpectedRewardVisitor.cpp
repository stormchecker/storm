#include "ExpectedTimeToExpectedRewardVisitor.h"

#include <boost/any.hpp>

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/Formulas.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {

ExpectedTimeToExpectedRewardVisitor::ExpectedTimeToExpectedRewardVisitor(std::string const& timeRewardModelName) : timeRewardModelName(timeRewardModelName) {
    // Intentionally left empty
}

std::shared_ptr<Formula> ExpectedTimeToExpectedRewardVisitor::substitute(Formula const& f) const {
    boost::any result = f.accept(*this, boost::any());
    return boost::any_cast<std::shared_ptr<Formula>>(result);
}

boost::any ExpectedTimeToExpectedRewardVisitor::visit(TimeOperatorFormula const& f, boost::any const& data) const {
    STORM_LOG_THROW(f.getSubformula().isEventuallyFormula(), storm::exceptions::InvalidPropertyException,
                    "Expected eventually formula within time operator. Got " << f << " instead.");
    std::shared_ptr<Formula> subsubformula =
        boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().asEventuallyFormula().getSubformula().accept(*this, data));
    STORM_LOG_THROW(f.getSubformula().isReachabilityTimeFormula(), storm::exceptions::InvalidPropertyException,
                    "Expected time path formula within time operator. Got " << f << " instead.");
    std::shared_ptr<Formula> subformula = std::make_shared<EventuallyFormula>(subsubformula, storm::logic::FormulaContext::Reward);
    return std::static_pointer_cast<Formula>(std::make_shared<RewardOperatorFormula>(subformula, timeRewardModelName, f.getOperatorInformation()));
}
}  // namespace logic
}  // namespace storm
