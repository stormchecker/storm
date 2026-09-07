#include "AtomicLabelFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/logic/AtomicExpressionFormula.h"
#include "storm/logic/FormulaVisitor.h"

namespace storm {
namespace logic {
AtomicLabelFormula::AtomicLabelFormula(std::string const& label) : label(label) {
    // Intentionally left empty.
}

bool AtomicLabelFormula::isAtomicLabelFormula() const {
    return true;
}

boost::any AtomicLabelFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

std::string const& AtomicLabelFormula::getLabel() const {
    return label;
}

void AtomicLabelFormula::gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicExpressionFormulas) const {
    atomicExpressionFormulas.push_back(std::dynamic_pointer_cast<AtomicLabelFormula const>(this->shared_from_this()));
}

std::ostream& AtomicLabelFormula::writeToStream(std::ostream& out, bool /*allowParentheses*/) const {
    // No parentheses necessary
    out << "\"" << label << "\"";
    return out;
}
}  // namespace logic
}  // namespace storm
