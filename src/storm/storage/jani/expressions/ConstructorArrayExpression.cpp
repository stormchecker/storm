#include "ConstructorArrayExpression.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"
#include "storm/storage/jani/visitor/JaniExpressionVisitor.h"

namespace storm {
namespace expressions {

ConstructorArrayExpression::ConstructorArrayExpression(ExpressionManager const& manager, Type const& type, std::shared_ptr<BaseExpression const> const& size,
                                                       storm::expressions::Variable indexVar, std::shared_ptr<BaseExpression const> const& elementExpression)
    : ArrayExpression(manager, type), sizeExpression(size), indexVar(indexVar), elementExpression(elementExpression) {
    // Intentionally left empty
}

void ConstructorArrayExpression::gatherVariables(std::set<storm::expressions::Variable>& variables) const {
    // The indexVar should not be gathered (unless it is already contained).
    bool indexVarContained = variables.find(indexVar) != variables.end();
    sizeExpression->gatherVariables(variables);
    elementExpression->gatherVariables(variables);
    if (!indexVarContained) {
        variables.erase(indexVar);
    }
}

bool ConstructorArrayExpression::containsVariables() const {
    if (sizeExpression->containsVariables()) {
        return true;
    }
    // The index variable should not count
    std::set<storm::expressions::Variable> variables;
    elementExpression->gatherVariables(variables);
    variables.erase(indexVar);
    return !variables.empty();
}

std::shared_ptr<BaseExpression const> ConstructorArrayExpression::simplify() const {
    return std::shared_ptr<BaseExpression const>(
        new ConstructorArrayExpression(getManager(), getType(), sizeExpression->simplify(), indexVar, elementExpression->simplify()));
}

boost::any ConstructorArrayExpression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    auto janiVisitor = dynamic_cast<JaniExpressionVisitor*>(&visitor);
    STORM_LOG_THROW(janiVisitor != nullptr, storm::exceptions::UnexpectedException, "Visitor of jani expression should be of type JaniVisitor.");
    return janiVisitor->visit(*this, data);
}

void ConstructorArrayExpression::printToStream(std::ostream& stream) const {
    stream << "array[ " << *elementExpression << " | " << indexVar.getExpression() << " < " << *sizeExpression << " ]";
}

std::shared_ptr<BaseExpression const> ConstructorArrayExpression::size() const {
    return sizeExpression;
}

std::shared_ptr<BaseExpression const> ConstructorArrayExpression::at(uint64_t i) const {
    std::map<storm::expressions::Variable, storm::expressions::Expression> substitution;
    substitution.emplace(indexVar, this->getManager().integer(i));
    return storm::jani::substituteJaniExpression(elementExpression->toExpression(), substitution, false).getBaseExpressionPointer();
}

std::shared_ptr<BaseExpression const> const& ConstructorArrayExpression::getElementExpression() const {
    return elementExpression;
}

storm::expressions::Variable const& ConstructorArrayExpression::getIndexVar() const {
    return indexVar;
}

}  // namespace expressions
}  // namespace storm