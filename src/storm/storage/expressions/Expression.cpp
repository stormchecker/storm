#include <map>
#include <unordered_map>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/storage/expressions/ChangeManagerVisitor.h"
#include "storm/storage/expressions/CheckIfThenElseGuardVisitor.h"
#include "storm/storage/expressions/CompiledExpression.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/storage/expressions/LinearityCheckVisitor.h"
#include "storm/storage/expressions/OperatorType.h"
#include "storm/storage/expressions/RestrictSyntaxVisitor.h"
#include "storm/storage/expressions/SubstitutionVisitor.h"
#include "storm/storage/expressions/SyntacticalEqualityCheckVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
/*!
 * Checks whether the two expressions share the same expression manager.
 *
 * @param a The first expression.
 * @param b The second expression.
 * @return True iff the expressions share the same manager.
 */
static void assertSameManager(BaseExpression const& a, BaseExpression const& b) {
    STORM_LOG_THROW(a.getManager() == b.getManager(), storm::exceptions::InvalidArgumentException, "Expressions are managed by different manager.");
}

// Spell out destructor explicitly so we can use forward declarations in the header.
Expression::~Expression() {
    // Intentionally left empty.
}

Expression::Expression(std::shared_ptr<BaseExpression const> const& expressionPtr) : expressionPtr(expressionPtr) {
    // Intentionally left empty.
}

Expression::Expression(Variable const& variable) : expressionPtr(std::shared_ptr<BaseExpression>(new VariableExpression(variable))) {
    // Intentionally left empty.
}

Expression Expression::changeManager(ExpressionManager const& newExpressionManager) const {
    ChangeManagerVisitor visitor(newExpressionManager);
    return visitor.changeManager(*this);
}

Expression Expression::substitute(std::map<Variable, Expression> const& identifierToExpressionMap) const {
    return SubstitutionVisitor<std::map<Variable, Expression>>(identifierToExpressionMap).substitute(*this);
}

Expression Expression::substitute(std::unordered_map<Variable, Expression> const& identifierToExpressionMap) const {
    return SubstitutionVisitor<std::unordered_map<Variable, Expression>>(identifierToExpressionMap).substitute(*this);
}

Expression Expression::substituteNonStandardPredicates() const {
    return RestrictSyntaxVisitor().substitute(*this);
}

bool Expression::evaluateAsBool(Valuation const* valuation) const {
    return this->getBaseExpression().evaluateAsBool(valuation);
}

int_fast64_t Expression::evaluateAsInt(Valuation const* valuation) const {
    return this->getBaseExpression().evaluateAsInt(valuation);
}

double Expression::evaluateAsDouble(Valuation const* valuation) const {
    return this->getBaseExpression().evaluateAsDouble(valuation);
}

storm::RationalNumber Expression::evaluateAsRational() const {
    return this->getBaseExpression().evaluateAsRational();
}

Expression Expression::simplify() const {
    return Expression(this->getBaseExpression().simplify());
}

Expression Expression::reduceNesting() const {
    return Expression(this->getBaseExpression().reduceNesting());
}

OperatorType Expression::getOperator() const {
    return this->getBaseExpression().getOperator();
}

bool Expression::isFunctionApplication() const {
    return this->getBaseExpression().isFunctionApplication();
}

uint_fast64_t Expression::getArity() const {
    return this->getBaseExpression().getArity();
}

Expression Expression::getOperand(uint_fast64_t operandIndex) const {
    return Expression(this->getBaseExpression().getOperand(operandIndex));
}

std::string const& Expression::getIdentifier() const {
    return this->getBaseExpression().getIdentifier();
}

bool Expression::containsVariables() const {
    return this->getBaseExpression().containsVariables();
}

bool Expression::isLiteral() const {
    return this->getBaseExpression().isLiteral();
}

bool Expression::isVariable() const {
    return this->getBaseExpression().isVariable();
}

bool Expression::isTrue() const {
    return this->getBaseExpression().isTrue();
}

bool Expression::isFalse() const {
    return this->getBaseExpression().isFalse();
}

bool Expression::areSame(storm::expressions::Expression const& other) const {
    return this->expressionPtr == other.expressionPtr;
}

std::set<storm::expressions::Variable> Expression::getVariables() const {
    std::set<storm::expressions::Variable> result;
    this->getBaseExpression().gatherVariables(result);
    return result;
}

void Expression::gatherVariables(std::set<storm::expressions::Variable>& variables) const {
    this->getBaseExpression().gatherVariables(variables);
}

bool Expression::containsVariable(std::set<storm::expressions::Variable> const& variables) const {
    std::set<storm::expressions::Variable> appearingVariables = this->getVariables();
    for (auto const& v : variables) {
        if (appearingVariables.count(v) > 0) {
            return true;
        }
    }
    return false;
}

bool Expression::containsVariableInITEGuard(std::set<storm::expressions::Variable> const& variables) const {
    CheckIfThenElseGuardVisitor visitor(variables);
    return visitor.check(*this);
}

bool Expression::isRelationalExpression() const {
    if (!this->isFunctionApplication()) {
        return false;
    }

    return this->getOperator() == OperatorType::Equal || this->getOperator() == OperatorType::NotEqual || this->getOperator() == OperatorType::Less ||
           this->getOperator() == OperatorType::LessOrEqual || this->getOperator() == OperatorType::Greater ||
           this->getOperator() == OperatorType::GreaterOrEqual;
}

bool Expression::isLinear() const {
    return LinearityCheckVisitor().check(*this);
}

BaseExpression const& Expression::getBaseExpression() const {
    return *this->expressionPtr;
}

std::shared_ptr<BaseExpression const> const& Expression::getBaseExpressionPointer() const {
    return this->expressionPtr;
}

ExpressionManager const& Expression::getManager() const {
    return this->getBaseExpression().getManager();
}

Type const& Expression::getType() const {
    return this->getBaseExpression().getType();
}

bool Expression::hasNumericalType() const {
    return this->getBaseExpression().hasNumericalType();
}

bool Expression::hasRationalType() const {
    return this->getBaseExpression().hasRationalType();
}

bool Expression::hasBooleanType() const {
    return this->getBaseExpression().hasBooleanType();
}

bool Expression::hasIntegerType() const {
    return this->getBaseExpression().hasIntegerType();
}

bool Expression::hasBitVectorType() const {
    return this->getBaseExpression().hasBitVectorType();
}

boost::any Expression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    return this->getBaseExpression().accept(visitor, data);
}

bool Expression::isInitialized() const {
    if (this->getBaseExpressionPointer()) {
        return true;
    }
    return false;
}

bool Expression::isSyntacticallyEqual(storm::expressions::Expression const& other) const {
    if (this->getBaseExpressionPointer() == other.getBaseExpressionPointer()) {
        return true;
    }
    SyntacticalEqualityCheckVisitor checker;
    return checker.isSyntacticallyEqual(*this, other);
}

bool Expression::hasCompiledExpression() const {
    return static_cast<bool>(compiledExpression);
}

void Expression::setCompiledExpression(std::shared_ptr<CompiledExpression> const& compiledExpression) const {
    this->compiledExpression = compiledExpression;
}

CompiledExpression const& Expression::getCompiledExpression() const {
    return *compiledExpression;
}

std::string Expression::toString() const {
    std::stringstream stream;
    stream << *this;
    return stream.str();
}

std::ostream& operator<<(std::ostream& stream, Expression const& expression) {
    if (expression.isInitialized()) {
        stream << expression.getBaseExpression();
    } else {
        stream << "__storm::notinitialized__";
    }
    return stream;
}

Expression operator+(Expression const& first, Expression const& second) {
    if (!first.isInitialized()) {
        return second;
    } else if (!second.isInitialized()) {
        return first;
    }
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(
        new BinaryNumericalFunctionExpression(first.getManager(), first.getType().plusMinusTimes(second.getType()), first.getBaseExpressionPointer(),
                                              second.getBaseExpressionPointer(), BinaryNumericalFunctionExpression::OperatorType::Plus)));
}

Expression operator+(Expression const& first, int64_t second) {
    return first + Expression(std::shared_ptr<BaseExpression>(new IntegerLiteralExpression(first.getBaseExpression().getManager(), second)));
}

Expression operator+(int64_t first, Expression const& second) {
    return second + first;
}

Expression operator-(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(new BinaryNumericalFunctionExpression(
        first.getBaseExpression().getManager(), first.getType().plusMinusTimes(second.getType()), first.getBaseExpressionPointer(),
        second.getBaseExpressionPointer(), BinaryNumericalFunctionExpression::OperatorType::Minus)));
}

Expression operator-(Expression const& first, int64_t second) {
    return first - Expression(std::shared_ptr<BaseExpression>(new IntegerLiteralExpression(first.getBaseExpression().getManager(), second)));
}

Expression operator-(int64_t first, Expression const& second) {
    return Expression(std::shared_ptr<BaseExpression>(new IntegerLiteralExpression(second.getBaseExpression().getManager(), first))) - second;
}

Expression operator-(Expression const& first) {
    return Expression(std::shared_ptr<BaseExpression>(new UnaryNumericalFunctionExpression(first.getBaseExpression().getManager(), first.getType().minus(),
                                                                                           first.getBaseExpressionPointer(),
                                                                                           UnaryNumericalFunctionExpression::OperatorType::Minus)));
}

Expression operator*(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(new BinaryNumericalFunctionExpression(
        first.getBaseExpression().getManager(), first.getType().plusMinusTimes(second.getType()), first.getBaseExpressionPointer(),
        second.getBaseExpressionPointer(), BinaryNumericalFunctionExpression::OperatorType::Times)));
}

Expression operator/(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(new BinaryNumericalFunctionExpression(
        first.getBaseExpression().getManager(), first.getType().divide(second.getType()), first.getBaseExpressionPointer(), second.getBaseExpressionPointer(),
        BinaryNumericalFunctionExpression::OperatorType::Divide)));
}

Expression operator%(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    // First we compute the remainder
    Expression remainder(std::shared_ptr<BaseExpression>(new BinaryNumericalFunctionExpression(
        first.getBaseExpression().getManager(), first.getType().modulo(second.getType()), first.getBaseExpressionPointer(), second.getBaseExpressionPointer(),
        BinaryNumericalFunctionExpression::OperatorType::Modulo)));

    // Deal with negative `first` in accordance with Prism
    return storm::expressions::ite(first >= 0, remainder, remainder + second);
}

Expression operator&&(Expression const& first, Expression const& second) {
    if (!first.isInitialized()) {
        return second;
    } else if (!second.isInitialized()) {
        return first;
    }
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(new BinaryBooleanFunctionExpression(
        first.getBaseExpression().getManager(), first.getType().logicalConnective(second.getType()), first.getBaseExpressionPointer(),
        second.getBaseExpressionPointer(), BinaryBooleanFunctionExpression::OperatorType::And)));
}

Expression operator||(Expression const& first, Expression const& second) {
    if (!first.isInitialized()) {
        return second;
    } else if (!second.isInitialized()) {
        return first;
    }
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(new BinaryBooleanFunctionExpression(
        first.getBaseExpression().getManager(), first.getType().logicalConnective(second.getType()), first.getBaseExpressionPointer(),
        second.getBaseExpressionPointer(), BinaryBooleanFunctionExpression::OperatorType::Or)));
}

Expression operator!(Expression const& first) {
    return Expression(std::shared_ptr<BaseExpression>(new UnaryBooleanFunctionExpression(first.getBaseExpression().getManager(),
                                                                                         first.getType().logicalConnective(), first.getBaseExpressionPointer(),
                                                                                         UnaryBooleanFunctionExpression::OperatorType::Not)));
}

Expression operator==(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(
        new BinaryRelationExpression(first.getBaseExpression().getManager(), first.getType().numericalComparison(second.getType()),
                                     first.getBaseExpressionPointer(), second.getBaseExpressionPointer(), RelationType::Equal)));
}

Expression operator!=(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    if (first.hasNumericalType() && second.hasNumericalType()) {
        return Expression(std::shared_ptr<BaseExpression>(
            new BinaryRelationExpression(first.getBaseExpression().getManager(), first.getType().numericalComparison(second.getType()),
                                         first.getBaseExpressionPointer(), second.getBaseExpressionPointer(), RelationType::NotEqual)));
    } else {
        return Expression(std::shared_ptr<BaseExpression>(new BinaryBooleanFunctionExpression(
            first.getBaseExpression().getManager(), first.getType().logicalConnective(second.getType()), first.getBaseExpressionPointer(),
            second.getBaseExpressionPointer(), BinaryBooleanFunctionExpression::OperatorType::Xor)));
    }
}

Expression operator>(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(
        new BinaryRelationExpression(first.getBaseExpression().getManager(), first.getType().numericalComparison(second.getType()),
                                     first.getBaseExpressionPointer(), second.getBaseExpressionPointer(), RelationType::Greater)));
}

Expression operator>=(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(
        new BinaryRelationExpression(first.getBaseExpression().getManager(), first.getType().numericalComparison(second.getType()),
                                     first.getBaseExpressionPointer(), second.getBaseExpressionPointer(), RelationType::GreaterOrEqual)));
}

Expression operator<(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(
        new BinaryRelationExpression(first.getBaseExpression().getManager(), first.getType().numericalComparison(second.getType()),
                                     first.getBaseExpressionPointer(), second.getBaseExpressionPointer(), RelationType::Less)));
}

Expression operator<=(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(
        new BinaryRelationExpression(first.getBaseExpression().getManager(), first.getType().numericalComparison(second.getType()),
                                     first.getBaseExpressionPointer(), second.getBaseExpressionPointer(), RelationType::LessOrEqual)));
}

Expression operator<(Expression const& first, int64_t second) {
    return first < Expression(std::shared_ptr<BaseExpression>(new IntegerLiteralExpression(first.getBaseExpression().getManager(), second)));
}

Expression operator>(Expression const& first, int64_t second) {
    return first > Expression(std::shared_ptr<BaseExpression>(new IntegerLiteralExpression(first.getBaseExpression().getManager(), second)));
}

Expression operator<=(Expression const& first, int64_t second) {
    return first <= Expression(std::shared_ptr<BaseExpression>(new IntegerLiteralExpression(first.getBaseExpression().getManager(), second)));
}

Expression operator>=(Expression const& first, int64_t second) {
    return first >= Expression(std::shared_ptr<BaseExpression>(new IntegerLiteralExpression(first.getBaseExpression().getManager(), second)));
}

Expression minimum(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(new BinaryNumericalFunctionExpression(
        first.getBaseExpression().getManager(), first.getType().minimumMaximum(second.getType()), first.getBaseExpressionPointer(),
        second.getBaseExpressionPointer(), BinaryNumericalFunctionExpression::OperatorType::Min)));
}

Expression maximum(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(new BinaryNumericalFunctionExpression(
        first.getBaseExpression().getManager(), first.getType().minimumMaximum(second.getType()), first.getBaseExpressionPointer(),
        second.getBaseExpressionPointer(), BinaryNumericalFunctionExpression::OperatorType::Max)));
}

Expression ite(Expression const& condition, Expression const& thenExpression, Expression const& elseExpression) {
    assertSameManager(condition.getBaseExpression(), thenExpression.getBaseExpression());
    assertSameManager(thenExpression.getBaseExpression(), elseExpression.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(
        new IfThenElseExpression(condition.getBaseExpression().getManager(), condition.getType().ite(thenExpression.getType(), elseExpression.getType()),
                                 condition.getBaseExpressionPointer(), thenExpression.getBaseExpressionPointer(), elseExpression.getBaseExpressionPointer())));
}

Expression implies(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(new BinaryBooleanFunctionExpression(
        first.getBaseExpression().getManager(), first.getType().logicalConnective(second.getType()), first.getBaseExpressionPointer(),
        second.getBaseExpressionPointer(), BinaryBooleanFunctionExpression::OperatorType::Implies)));
}

Expression iff(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(new BinaryBooleanFunctionExpression(
        first.getBaseExpression().getManager(), first.getType().logicalConnective(second.getType()), first.getBaseExpressionPointer(),
        second.getBaseExpressionPointer(), BinaryBooleanFunctionExpression::OperatorType::Iff)));
}

Expression xclusiveor(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(new BinaryBooleanFunctionExpression(
        first.getBaseExpression().getManager(), first.getType().logicalConnective(second.getType()), first.getBaseExpressionPointer(),
        second.getBaseExpressionPointer(), BinaryBooleanFunctionExpression::OperatorType::Xor)));
}

Expression pow(Expression const& base, Expression const& exponent, bool allowIntegerType) {
    assertSameManager(base.getBaseExpression(), exponent.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(new BinaryNumericalFunctionExpression(
        base.getBaseExpression().getManager(), base.getType().power(exponent.getType(), allowIntegerType), base.getBaseExpressionPointer(),
        exponent.getBaseExpressionPointer(), BinaryNumericalFunctionExpression::OperatorType::Power)));
}

Expression floor(Expression const& first) {
    STORM_LOG_THROW(first.hasNumericalType(), storm::exceptions::InvalidTypeException, "Operator 'floor' requires numerical operand.");
    return Expression(std::shared_ptr<BaseExpression>(new UnaryNumericalFunctionExpression(first.getBaseExpression().getManager(), first.getType().floorCeil(),
                                                                                           first.getBaseExpressionPointer(),
                                                                                           UnaryNumericalFunctionExpression::OperatorType::Floor)));
}

Expression ceil(Expression const& first) {
    STORM_LOG_THROW(first.hasNumericalType(), storm::exceptions::InvalidTypeException, "Operator 'ceil' requires numerical operand.");
    return Expression(std::shared_ptr<BaseExpression>(new UnaryNumericalFunctionExpression(first.getBaseExpression().getManager(), first.getType().floorCeil(),
                                                                                           first.getBaseExpressionPointer(),
                                                                                           UnaryNumericalFunctionExpression::OperatorType::Ceil)));
}

Expression round(Expression const& first) {
    STORM_LOG_THROW(first.hasNumericalType(), storm::exceptions::InvalidTypeException, "Operator 'round' requires numerical operand.");
    return floor(first + first.getManager().rational(0.5));
}

Expression abs(Expression const& first) {
    STORM_LOG_THROW(first.hasNumericalType(), storm::exceptions::InvalidTypeException, "Abs is only defined for numerical operands.");
    return ite(first < 0, -first, first);
}

Expression sign(Expression const& first) {
    STORM_LOG_THROW(first.hasNumericalType(), storm::exceptions::InvalidTypeException, "Sign is only defined for numerical operands.");
    return ite(first > 0, first.getManager().integer(1), ite(first < 0, first.getManager().integer(0), first.getManager().integer(0)));
}

Expression truncate(Expression const& first) {
    STORM_LOG_THROW(first.hasNumericalType(), storm::exceptions::InvalidTypeException, "Truncate is only defined for numerical operands.");
    return ite(first < 0, floor(first), ceil(first));
}

Expression atLeastOneOf(std::vector<Expression> const& expressions) {
    STORM_LOG_THROW(expressions.size() > 0, storm::exceptions::InvalidArgumentException, "AtLeastOneOf requires arguments.");
    std::vector<std::shared_ptr<BaseExpression const>> baseexpressions;
    for (auto const& expr : expressions) {
        baseexpressions.push_back(expr.getBaseExpressionPointer());
    }
    return Expression(
        std::shared_ptr<BaseExpression>(new PredicateExpression(expressions.front().getManager(), expressions.front().getManager().getBooleanType(),
                                                                baseexpressions, PredicateExpression::PredicateType::AtLeastOneOf)));
}

Expression atMostOneOf(std::vector<Expression> const& expressions) {
    STORM_LOG_THROW(expressions.size() > 0, storm::exceptions::InvalidArgumentException, "AtMostOneOf requires arguments.");
    std::vector<std::shared_ptr<BaseExpression const>> baseexpressions;
    for (auto const& expr : expressions) {
        baseexpressions.push_back(expr.getBaseExpressionPointer());
    }
    return Expression(
        std::shared_ptr<BaseExpression>(new PredicateExpression(expressions.front().getManager(), expressions.front().getManager().getBooleanType(),
                                                                baseexpressions, PredicateExpression::PredicateType::AtMostOneOf)));
}

Expression exactlyOneOf(std::vector<Expression> const& expressions) {
    STORM_LOG_THROW(expressions.size() > 0, storm::exceptions::InvalidArgumentException, "ExactlyOneOf requires arguments.");
    std::vector<std::shared_ptr<BaseExpression const>> baseexpressions;
    for (auto const& expr : expressions) {
        baseexpressions.push_back(expr.getBaseExpressionPointer());
    }
    return Expression(
        std::shared_ptr<BaseExpression>(new PredicateExpression(expressions.front().getManager(), expressions.front().getManager().getBooleanType(),
                                                                baseexpressions, PredicateExpression::PredicateType::ExactlyOneOf)));
}

Expression disjunction(std::vector<storm::expressions::Expression> const& expressions) {
    return applyAssociative(expressions, [](Expression const& e1, Expression const& e2) { return e1 || e2; });
}

Expression conjunction(std::vector<storm::expressions::Expression> const& expressions) {
    return applyAssociative(expressions, [](Expression const& e1, Expression const& e2) { return e1 && e2; });
}

Expression sum(std::vector<storm::expressions::Expression> const& expressions) {
    return applyAssociative(expressions, [](Expression const& e1, Expression const& e2) { return e1 + e2; });
}

Expression modulo(Expression const& first, Expression const& second) {
    return first % second;
}

Expression logarithm(Expression const& first, Expression const& second) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(new BinaryNumericalFunctionExpression(
        first.getBaseExpression().getManager(), first.getType().logarithm(second.getType()), first.getBaseExpressionPointer(),
        second.getBaseExpressionPointer(), BinaryNumericalFunctionExpression::OperatorType::Logarithm)));
}

Expression cos(Expression const& first) {
    STORM_LOG_THROW(first.hasNumericalType(), storm::exceptions::InvalidTypeException, "Operator 'cos' requires numerical operand.");
    return Expression(std::shared_ptr<BaseExpression>(new UnaryNumericalFunctionExpression(first.getBaseExpression().getManager(),
                                                                                           first.getType().trigonometric(), first.getBaseExpressionPointer(),
                                                                                           UnaryNumericalFunctionExpression::OperatorType::Cos)));
}

Expression sin(Expression const& first) {
    STORM_LOG_THROW(first.hasNumericalType(), storm::exceptions::InvalidTypeException, "Operator 'sin' requires numerical operand.");
    return Expression(std::shared_ptr<BaseExpression>(new UnaryNumericalFunctionExpression(first.getBaseExpression().getManager(),
                                                                                           first.getType().trigonometric(), first.getBaseExpressionPointer(),
                                                                                           UnaryNumericalFunctionExpression::OperatorType::Sin)));
}

Expression apply(std::vector<storm::expressions::Expression> const& expressions,
                 std::function<Expression(Expression const&, Expression const&)> const& function) {
    STORM_LOG_THROW(!expressions.empty(), storm::exceptions::InvalidArgumentException, "Cannot build function application of empty expression list.");
    auto it = expressions.begin();
    auto ite = expressions.end();
    Expression result = *it;
    ++it;

    for (; it != ite; ++it) {
        result = function(result, *it);
    }

    return result;
}

Expression makeBinaryRelationExpression(Expression const& first, Expression const& second, RelationType const& reltype) {
    assertSameManager(first.getBaseExpression(), second.getBaseExpression());
    return Expression(std::shared_ptr<BaseExpression>(
        new BinaryRelationExpression(first.getBaseExpression().getManager(), first.getType().numericalComparison(second.getType()),
                                     first.getBaseExpressionPointer(), second.getBaseExpressionPointer(), reltype)));
}

Expression applyAssociative(std::vector<storm::expressions::Expression> const& expressions,
                            std::function<Expression(Expression const&, Expression const&)> const& function) {
    STORM_LOG_THROW(!expressions.empty(), storm::exceptions::InvalidArgumentException, "Cannot build function application of empty expression list.");

    // Balance the syntax tree if there are enough operands
    if (expressions.size() >= 4) {
        // we treat operands and literals seperately so that a subsequent call to simplify() is more successful (see, e.g., (a + 1) + (b + 1))
        std::vector<std::vector<storm::expressions::Expression>> operandTypes(2);
        auto& nonliterals = operandTypes[0];
        auto& literals = operandTypes[1];
        for (auto const& expression : expressions) {
            if (expression.isLiteral()) {
                literals.push_back(expression);
            } else {
                nonliterals.push_back(expression);
            }
        }

        for (auto& operands : operandTypes) {
            auto opIt = operands.begin();
            while (operands.size() > 1) {
                if (opIt == operands.end() || opIt == operands.end() - 1) {
                    opIt = operands.begin();
                }
                *opIt = function(*opIt, operands.back());
                operands.pop_back();
                ++opIt;
            }
        }
        if (nonliterals.empty()) {
            return literals.front();
        } else if (literals.empty()) {
            return nonliterals.front();
        } else {
            return function(literals.front(), nonliterals.front());
        }
    } else {
        return apply(expressions, function);
    }
}
}  // namespace expressions
}  // namespace storm
