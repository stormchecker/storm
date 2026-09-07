#include "AtomicExpressionFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/logic/FormulaVisitor.h"

namespace storm {
namespace logic {
AtomicExpressionFormula::AtomicExpressionFormula(storm::expressions::Expression const& expression) : expression(expression) {
    // Intentionally left empty.
}

bool AtomicExpressionFormula::isAtomicExpressionFormula() const {
    return true;
}

boost::any AtomicExpressionFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

storm::expressions::Expression const& AtomicExpressionFormula::getExpression() const {
    return expression;
}

void AtomicExpressionFormula::gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const {
    atomicExpressionFormulas.push_back(std::dynamic_pointer_cast<AtomicExpressionFormula const>(this->shared_from_this()));
}

void AtomicExpressionFormula::gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const {
    expression.gatherVariables(usedVariables);
}

std::ostream& AtomicExpressionFormula::writeToStream(std::ostream& out, bool allowParentheses) const {
    bool parentheses = allowParentheses & (this->expression.isLiteral() || this->expression.isVariable());
    if (parentheses) {
        out << "(";
    }
    out << expression;
    if (parentheses) {
        out << ")";
    }
    return out;
}
}  // namespace logic
}  // namespace storm
