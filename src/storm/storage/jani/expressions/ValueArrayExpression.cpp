#include "ValueArrayExpression.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/visitor/JaniExpressionVisitor.h"

namespace storm {
namespace expressions {

ValueArrayExpression::ValueArrayExpression(ExpressionManager const& manager, Type const& type,
                                           std::vector<std::shared_ptr<BaseExpression const>> const& elements)
    : ArrayExpression(manager, type), elements(elements) {
    // Intentionally left empty
}

void ValueArrayExpression::gatherVariables(std::set<storm::expressions::Variable>& variables) const {
    for (auto const& e : elements) {
        e->gatherVariables(variables);
    }
}

bool ValueArrayExpression::containsVariables() const {
    for (auto const& e : elements) {
        if (e->containsVariables()) {
            return true;
        }
    }
    return false;
}

std::shared_ptr<BaseExpression const> ValueArrayExpression::simplify() const {
    std::vector<std::shared_ptr<BaseExpression const>> simplifiedElements;
    simplifiedElements.reserve(elements.size());
    for (auto const& e : elements) {
        simplifiedElements.push_back(e->simplify());
    }
    return std::shared_ptr<BaseExpression const>(new ValueArrayExpression(getManager(), getType(), simplifiedElements));
}

boost::any ValueArrayExpression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    auto janiVisitor = dynamic_cast<JaniExpressionVisitor*>(&visitor);
    STORM_LOG_THROW(janiVisitor != nullptr, storm::exceptions::UnexpectedException, "Visitor of jani expression should be of type JaniVisitor.");
    return janiVisitor->visit(*this, data);
}

void ValueArrayExpression::printToStream(std::ostream& stream) const {
    stream << "array[ ";
    bool first = true;
    for (auto const& e : elements) {
        if (!first) {
            stream << " , ";
        }
        first = false;
        stream << *e;
    }
    stream << " ]";
}

std::shared_ptr<BaseExpression const> ValueArrayExpression::size() const {
    return getManager().integer(elements.size()).getBaseExpressionPointer();
}

std::shared_ptr<BaseExpression const> ValueArrayExpression::at(uint64_t i) const {
    STORM_LOG_THROW(i < elements.size(), storm::exceptions::InvalidArgumentException,
                    "Tried to access the element with index " << i << " of an array of size " << elements.size() << ".");
    return elements[i];
}

}  // namespace expressions
}  // namespace storm