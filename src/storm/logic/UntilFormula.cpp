#include "UntilFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/logic/FormulaVisitor.h"

namespace storm {
namespace logic {
UntilFormula::UntilFormula(std::shared_ptr<Formula const> const& leftSubformula, std::shared_ptr<Formula const> const& rightSubformula)
    : BinaryPathFormula(leftSubformula, rightSubformula) {
    // Intentionally left empty.
}

bool UntilFormula::isUntilFormula() const {
    return true;
}

bool UntilFormula::isProbabilityPathFormula() const {
    return true;
}

boost::any UntilFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

std::ostream& UntilFormula::writeToStream(std::ostream& out, bool allowParentheses) const {
    if (allowParentheses) {
        out << "(";
    }
    this->getLeftSubformula().writeToStream(out, true);
    out << " U ";
    this->getRightSubformula().writeToStream(out, true);
    if (allowParentheses) {
        out << ")";
    }
    return out;
}
}  // namespace logic
}  // namespace storm
