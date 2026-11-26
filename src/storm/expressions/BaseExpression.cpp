#include "storm/expressions/BaseExpression.h"
#include <boost/any.hpp>
#include "storm/exceptions/InvalidAccessException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/expressions/ExpressionManager.h"
#include "storm/utility/macros.h"

#include "storm/expressions/Expressions.h"
#include "storm/expressions/ReduceNestingVisitor.h"
#include "storm/expressions/ToRationalNumberVisitor.h"

namespace storm {
namespace expressions {
BaseExpression::BaseExpression(ExpressionManager const& manager, Type const& type) : manager(manager), type(type) {
    // Intentionally left empty.
}

void BaseExpression::gatherVariablesCached(std::set<storm::expressions::Variable>& variables, VariableGatheringCache& cache) const {
    auto self = weak_from_this().lock();
    if (self.get() == nullptr) {
        // Direct gatherVariables() calls also support nodes not owned by a shared_ptr.
        gatherVariablesImpl(variables, cache);
        return;
    }
    auto it = cache.find(self);
    if (it == cache.end()) {
        std::set<storm::expressions::Variable> freeVariables;
        gatherVariablesImpl(freeVariables, cache);
        it = cache.emplace(std::move(self), std::move(freeVariables)).first;
    }
    variables.insert(it->second.begin(), it->second.end());
}

void BaseExpression::gatherVariablesImpl(std::set<storm::expressions::Variable>& variables, VariableGatheringCache&) const {
    // Leaves and custom expression classes retain their existing implementation.
    gatherVariables(variables);
}

std::shared_ptr<BaseExpression const> BaseExpression::simplify() const {
    SimplificationCache cache;
    return simplify(cache);
}

std::shared_ptr<BaseExpression const> BaseExpression::simplify(SimplificationCache& cache) const {
    auto self = getSharedPointer();
    auto it = cache.find(self);
    if (it != cache.end()) {
        return it->second;
    }
    auto result = simplifyImpl(cache);
    // Own the key as well as the result: simplification can discard input subexpressions.
    cache.emplace(std::move(self), result);
    return result;
}

Expression BaseExpression::toExpression() const {
    return Expression(shared_from_this());
}

Type const& BaseExpression::getType() const {
    return this->type;
}

bool BaseExpression::hasIntegerType() const {
    return this->getType().isIntegerType();
}

bool BaseExpression::hasBitVectorType() const {
    return this->getType().isBitVectorType();
}

bool BaseExpression::hasNumericalType() const {
    return this->getType().isNumericalType();
}

bool BaseExpression::hasBooleanType() const {
    return this->getType().isBooleanType();
}

bool BaseExpression::hasRationalType() const {
    return this->getType().isRationalType();
}

int_fast64_t BaseExpression::evaluateAsInt(Valuation const*) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Unable to evaluate expression as integer.");
}

bool BaseExpression::evaluateAsBool(Valuation const*) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Unable to evaluate expression as boolean.");
}

double BaseExpression::evaluateAsDouble(Valuation const*) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Unable to evaluate expression as double.");
}

storm::RationalNumber BaseExpression::evaluateAsRational() const {
    ToRationalNumberVisitor<storm::RationalNumber> v;
    return v.toRationalNumber(this->toExpression());
}

uint_fast64_t BaseExpression::getArity() const {
    return 0;
}

std::shared_ptr<BaseExpression const> BaseExpression::getOperand(uint_fast64_t operandIndex) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException,
                    "Unable to access operand " << operandIndex << " in expression '" << *this << "' of arity 0.");
}

std::string const& BaseExpression::getIdentifier() const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException, "Unable to access identifier of non-constant, non-variable expression.");
}

OperatorType BaseExpression::getOperator() const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidAccessException, "Unable to access operator of non-function application expression.");
}

std::shared_ptr<BaseExpression const> BaseExpression::reduceNesting() const {
    ReduceNestingVisitor v;
    return v.reduceNesting(this->toExpression()).getBaseExpressionPointer();
}

bool BaseExpression::containsVariables() const {
    return false;
}

bool BaseExpression::isLiteral() const {
    return false;
}

bool BaseExpression::isVariable() const {
    return false;
}

bool BaseExpression::isTrue() const {
    return false;
}

bool BaseExpression::isFalse() const {
    return false;
}

bool BaseExpression::isFunctionApplication() const {
    return false;
}

ExpressionManager const& BaseExpression::getManager() const {
    return manager;
}

std::shared_ptr<BaseExpression const> BaseExpression::getSharedPointer() const {
    return this->shared_from_this();
}

bool BaseExpression::isIfThenElseExpression() const {
    return false;
}

IfThenElseExpression const& BaseExpression::asIfThenElseExpression() const {
    return static_cast<IfThenElseExpression const&>(*this);
}

bool BaseExpression::isBinaryBooleanFunctionExpression() const {
    return false;
}

BinaryBooleanFunctionExpression const& BaseExpression::asBinaryBooleanFunctionExpression() const {
    return static_cast<BinaryBooleanFunctionExpression const&>(*this);
}

bool BaseExpression::isBinaryNumericalFunctionExpression() const {
    return false;
}

BinaryNumericalFunctionExpression const& BaseExpression::asBinaryNumericalFunctionExpression() const {
    return static_cast<BinaryNumericalFunctionExpression const&>(*this);
}

bool BaseExpression::isBinaryRelationExpression() const {
    return false;
}

BinaryRelationExpression const& BaseExpression::asBinaryRelationExpression() const {
    return static_cast<BinaryRelationExpression const&>(*this);
}

bool BaseExpression::isBooleanLiteralExpression() const {
    return false;
}

BooleanLiteralExpression const& BaseExpression::asBooleanLiteralExpression() const {
    return static_cast<BooleanLiteralExpression const&>(*this);
}

bool BaseExpression::isIntegerLiteralExpression() const {
    return false;
}

IntegerLiteralExpression const& BaseExpression::asIntegerLiteralExpression() const {
    return static_cast<IntegerLiteralExpression const&>(*this);
}

bool BaseExpression::isRationalLiteralExpression() const {
    return false;
}

RationalLiteralExpression const& BaseExpression::asRationalLiteralExpression() const {
    return static_cast<RationalLiteralExpression const&>(*this);
}

bool BaseExpression::isUnaryBooleanFunctionExpression() const {
    return false;
}

UnaryBooleanFunctionExpression const& BaseExpression::asUnaryBooleanFunctionExpression() const {
    return static_cast<UnaryBooleanFunctionExpression const&>(*this);
}

bool BaseExpression::isUnaryNumericalFunctionExpression() const {
    return false;
}

UnaryNumericalFunctionExpression const& BaseExpression::asUnaryNumericalFunctionExpression() const {
    return static_cast<UnaryNumericalFunctionExpression const&>(*this);
}

bool BaseExpression::isVariableExpression() const {
    return false;
}

VariableExpression const& BaseExpression::asVariableExpression() const {
    return static_cast<VariableExpression const&>(*this);
}

bool BaseExpression::isPredicateExpression() const {
    return false;
}

PredicateExpression const& BaseExpression::asPredicateExpression() const {
    return static_cast<PredicateExpression const&>(*this);
}

std::ostream& operator<<(std::ostream& stream, BaseExpression const& expression) {
    expression.printToStream(stream);
    return stream;
}
}  // namespace expressions
}  // namespace storm
