#include "storm/logic/TimeOperatorFormula.h"

#include <boost/any.hpp>
#include <ostream>
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/EventuallyFormula.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
TimeOperatorFormula::TimeOperatorFormula(std::shared_ptr<Formula const> const& subformula, OperatorInformation const& operatorInformation)
    : OperatorFormula(subformula, operatorInformation) {
    STORM_LOG_ASSERT(subformula->isTimePathFormula(), "Expected time path formula.");
    // Intentionally left empty.
}

bool TimeOperatorFormula::isTimeOperatorFormula() const {
    return true;
}

boost::any TimeOperatorFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

std::ostream& TimeOperatorFormula::writeToStream(std::ostream& out, bool /* allowParentheses */) const {
    // No parentheses necessary
    out << "T";
    OperatorFormula::writeToStream(out);
    return out;
}
}  // namespace logic
}  // namespace storm
