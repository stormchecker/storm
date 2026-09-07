#include "NextFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/logic/FormulaVisitor.h"

namespace storm {
namespace logic {
NextFormula::NextFormula(std::shared_ptr<Formula const> const& subformula) : UnaryPathFormula(subformula) {
    // Intentionally left empty.
}

bool NextFormula::isNextFormula() const {
    return true;
}

bool NextFormula::isProbabilityPathFormula() const {
    return true;
}

boost::any NextFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

std::ostream& NextFormula::writeToStream(std::ostream& out, bool allowParentheses) const {
    if (allowParentheses) {
        out << "(";
    }
    out << "X ";
    this->getSubformula().writeToStream(out, !this->getSubformula().isUnaryFormula());
    if (allowParentheses) {
        out << ")";
    }
    return out;
}
}  // namespace logic
}  // namespace storm
