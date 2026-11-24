#include "storm/logic/WeakUntilFormula.h"
#include <boost/any.hpp>
#include <ostream>

#include "storm/logic/FormulaVisitor.h"

namespace storm {
namespace logic {
WeakUntilFormula::WeakUntilFormula(std::shared_ptr<Formula const> const& leftSubformula, std::shared_ptr<Formula const> const& rightSubformula)
    : BinaryPathFormula(leftSubformula, rightSubformula) {
    // Intentionally left empty.
}

bool WeakUntilFormula::isWeakUntilFormula() const {
    return true;
}

bool WeakUntilFormula::isProbabilityPathFormula() const {
    return true;
}

boost::any WeakUntilFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

std::ostream& WeakUntilFormula::writeToStream(std::ostream& out, bool allowParentheses) const {
    if (allowParentheses) {
        out << "(";
    }
    this->getLeftSubformula().writeToStream(out, true);
    out << " W ";
    this->getRightSubformula().writeToStream(out, true);
    if (allowParentheses) {
        out << ")";
    }
    return out;
}
}  // namespace logic
}  // namespace storm
