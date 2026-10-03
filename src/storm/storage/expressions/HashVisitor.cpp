#include "storm/storage/expressions/HashVisitor.h"

#include <boost/functional/hash.hpp>

#include "storm/storage/expressions/Expressions.h"

namespace storm {
namespace expressions {

namespace {

// A distinct seed for every expression type, so that two expressions of different types with equal
// operator and operands do not collide. Collisions would be harmless for correctness, but avoiding
// the obvious ones keeps the distribution reasonable.
std::size_t const TypeIfThenElse = 0xa1;
std::size_t const TypeBinaryBooleanFunction = 0xb2;
std::size_t const TypeBinaryNumericalFunction = 0xb3;
std::size_t const TypeBinaryRelation = 0xb4;
std::size_t const TypeVariable = 0xc5;
std::size_t const TypeUnaryBooleanFunction = 0xd6;
std::size_t const TypeUnaryNumericalFunction = 0xd7;
std::size_t const TypeBooleanLiteral = 0xe8;
std::size_t const TypeIntegerLiteral = 0xe9;
std::size_t const TypeRationalLiteral = 0xea;
std::size_t const TypePredicate = 0xfb;

template<typename T>
std::size_t hashValue(T const& value) {
    return boost::hash<T>{}(value);
}

}  // namespace

std::size_t HashVisitor::hash(storm::expressions::Expression const& expression) {
    return boost::any_cast<std::size_t>(expression.accept(*this, boost::any{}));
}

std::size_t HashVisitor::hashExpression(BaseExpression const& expression) {
    return boost::any_cast<std::size_t>(expression.accept(*this, boost::any{}));
}

boost::any HashVisitor::visit(IfThenElseExpression const& expression, boost::any const& data) {
    std::size_t result = TypeIfThenElse;
    boost::hash_combine(result, hashExpression(*expression.getCondition()));
    boost::hash_combine(result, hashExpression(*expression.getThenExpression()));
    boost::hash_combine(result, hashExpression(*expression.getElseExpression()));
    return result;
}

boost::any HashVisitor::visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    std::size_t result = TypeBinaryBooleanFunction;
    boost::hash_combine(result, hashValue(expression.getOperatorType()));
    boost::hash_combine(result, hashExpression(*expression.getFirstOperand()));
    boost::hash_combine(result, hashExpression(*expression.getSecondOperand()));
    return result;
}

boost::any HashVisitor::visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    std::size_t result = TypeBinaryNumericalFunction;
    boost::hash_combine(result, hashValue(expression.getOperatorType()));
    boost::hash_combine(result, hashExpression(*expression.getFirstOperand()));
    boost::hash_combine(result, hashExpression(*expression.getSecondOperand()));
    return result;
}

boost::any HashVisitor::visit(BinaryRelationExpression const& expression, boost::any const& data) {
    std::size_t result = TypeBinaryRelation;
    boost::hash_combine(result, hashValue(expression.getRelationType()));
    boost::hash_combine(result, hashExpression(*expression.getFirstOperand()));
    boost::hash_combine(result, hashExpression(*expression.getSecondOperand()));
    return result;
}

boost::any HashVisitor::visit(VariableExpression const& expression, boost::any const& data) {
    std::size_t result = TypeVariable;
    // The variable's index identifies it within its manager, which is all that the syntactical
    // equality check uses as well.
    boost::hash_combine(result, hashValue(expression.getVariable().getIndex()));
    return result;
}

boost::any HashVisitor::visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    std::size_t result = TypeUnaryBooleanFunction;
    boost::hash_combine(result, hashValue(expression.getOperatorType()));
    boost::hash_combine(result, hashExpression(*expression.getOperand()));
    return result;
}

boost::any HashVisitor::visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    std::size_t result = TypeUnaryNumericalFunction;
    boost::hash_combine(result, hashValue(expression.getOperatorType()));
    boost::hash_combine(result, hashExpression(*expression.getOperand()));
    return result;
}

boost::any HashVisitor::visit(BooleanLiteralExpression const& expression, boost::any const& data) {
    std::size_t result = TypeBooleanLiteral;
    boost::hash_combine(result, hashValue(expression.getValue()));
    return result;
}

boost::any HashVisitor::visit(IntegerLiteralExpression const& expression, boost::any const& data) {
    std::size_t result = TypeIntegerLiteral;
    boost::hash_combine(result, hashValue(expression.getValue()));
    return result;
}

boost::any HashVisitor::visit(RationalLiteralExpression const& expression, boost::any const& data) {
    std::size_t result = TypeRationalLiteral;
    boost::hash_combine(result, hashValue(expression.getValue()));
    return result;
}

boost::any HashVisitor::visit(PredicateExpression const& expression, boost::any const& data) {
    std::size_t result = TypePredicate;
    boost::hash_combine(result, hashValue(expression.getPredicateType()));
    for (uint_fast64_t i = 0; i < expression.getArity(); ++i) {
        boost::hash_combine(result, hashExpression(*expression.getOperand(i)));
    }
    return result;
}

}  // namespace expressions
}  // namespace storm
