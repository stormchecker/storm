#include "ProbabilityOperatorFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
ProbabilityOperatorFormula::ProbabilityOperatorFormula(std::shared_ptr<Formula const> const& subformula, OperatorInformation const& operatorInformation)
    : OperatorFormula(subformula, operatorInformation) {
    // Intentionally left empty.
}

bool ProbabilityOperatorFormula::isProbabilityOperatorFormula() const {
    return true;
}

boost::any ProbabilityOperatorFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

std::ostream& ProbabilityOperatorFormula::writeToStream(std::ostream& out, bool /* allowParentheses */) const {
    // No parentheses necessary
    out << "P";
    OperatorFormula::writeToStream(out, false);
    return out;
}
}  // namespace logic
}  // namespace storm
