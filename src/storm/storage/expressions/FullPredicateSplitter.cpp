#include "FullPredicateSplitter.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {

std::vector<storm::expressions::Expression>& getAtomicExpressions(boost::any const& data) {
    return *boost::any_cast<std::vector<storm::expressions::Expression>*>(data);
}

std::vector<storm::expressions::Expression> FullPredicateSplitter::split(storm::expressions::Expression const& expression) {
    STORM_LOG_THROW(expression.hasBooleanType(), storm::exceptions::InvalidArgumentException, "Expected predicate of boolean type.");

    // Gather all atoms.
    std::vector<storm::expressions::Expression> atomicExpressions;
    expression.accept(*this, &atomicExpressions);

    // Remove all boolean literals from the atoms.
    std::vector<storm::expressions::Expression> atomsToKeep;
    for (auto const& atom : atomicExpressions) {
        if (!atom.isTrue() && !atom.isFalse()) {
            atomsToKeep.push_back(atom);
        }
    }
    atomicExpressions = std::move(atomsToKeep);

    return atomicExpressions;
}

boost::any FullPredicateSplitter::visit(IfThenElseExpression const& expression, boost::any const& data) {
    getAtomicExpressions(data).push_back(expression.toExpression());
    return data;
}

boost::any FullPredicateSplitter::visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    expression.getFirstOperand()->accept(*this, data);
    expression.getSecondOperand()->accept(*this, data);
    return data;
}

boost::any FullPredicateSplitter::visit(BinaryNumericalFunctionExpression const&, boost::any const& data) {
    return data;
}

boost::any FullPredicateSplitter::visit(BinaryRelationExpression const& expression, boost::any const& data) {
    getAtomicExpressions(data).push_back(expression.toExpression());
    return data;
}

boost::any FullPredicateSplitter::visit(VariableExpression const& expression, boost::any const& data) {
    if (expression.hasBooleanType()) {
        getAtomicExpressions(data).push_back(expression.toExpression());
    }
    return data;
}

boost::any FullPredicateSplitter::visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    expression.getOperand()->accept(*this, data);
    return data;
}

boost::any FullPredicateSplitter::visit(UnaryNumericalFunctionExpression const&, boost::any const& data) {
    return data;
}

boost::any FullPredicateSplitter::visit(BooleanLiteralExpression const&, boost::any const& data) {
    return data;
}

boost::any FullPredicateSplitter::visit(IntegerLiteralExpression const&, boost::any const& data) {
    return data;
}

boost::any FullPredicateSplitter::visit(RationalLiteralExpression const&, boost::any const& data) {
    return data;
}

}  // namespace expressions
}  // namespace storm
