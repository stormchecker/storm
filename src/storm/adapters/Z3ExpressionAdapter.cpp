#include "storm/adapters/Z3ExpressionAdapter.h"

#include <cstdint>

#include "storm/exceptions/ExpressionEvaluationException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace adapters {

#ifdef STORM_HAVE_Z3

typedef int64_t Z3_SIGNED_INTEGER;
typedef uint64_t Z3_UNSIGNED_INTEGER;

Z3ExpressionAdapter::Z3ExpressionAdapter(storm::expressions::ExpressionManager& manager, z3::context& context)
    : manager(manager), context(context), additionalAssertions(), variableToExpressionMapping() {
    // Intentionally left empty.
}

z3::expr Z3ExpressionAdapter::translateExpression(storm::expressions::Expression const& expression) {
    STORM_LOG_ASSERT(expression.getManager() == this->manager, "Invalid expression for solver.");

    z3::expr result = boost::any_cast<z3::expr>(expression.getBaseExpression().accept(*this, boost::none));
    expressionCache.clear();

    for (z3::expr const& assertion : additionalAssertions) {
        result = result && assertion;
    }
    additionalAssertions.clear();
    return result.simplify();
}

z3::expr Z3ExpressionAdapter::translateExpression(storm::expressions::Variable const& variable) {
    STORM_LOG_ASSERT(variable.getManager() == this->manager, "Invalid expression for solver.");

    auto const& variableExpressionPair = variableToExpressionMapping.find(variable);
    if (variableExpressionPair == variableToExpressionMapping.end()) {
        return createVariable(variable);
    }

    return variableExpressionPair->second;
}

storm::expressions::Variable const& Z3ExpressionAdapter::getVariable(z3::func_decl z3Declaration) {
    auto const& declarationVariablePair = declarationToVariableMapping.find(z3Declaration);
    STORM_LOG_ASSERT(declarationVariablePair != declarationToVariableMapping.end(), "Unable to find declaration.");
    return declarationVariablePair->second;
}

storm::expressions::Expression Z3ExpressionAdapter::translateExpression(z3::expr const& expr) {
    if (expr.is_app()) {
        switch (expr.decl().decl_kind()) {
            case Z3_OP_TRUE:
                return manager.boolean(true);
            case Z3_OP_FALSE:
                return manager.boolean(false);
            case Z3_OP_EQ:
                return this->translateExpression(expr.arg(0)) == this->translateExpression(expr.arg(1));
            case Z3_OP_DISTINCT: {
                unsigned args = expr.num_args();
                STORM_LOG_THROW(args != 0, storm::exceptions::ExpressionEvaluationException,
                                "Failed to convert Z3 expression. DISTINCT (mutual != ) operator with 0-arity is assumed to be an error.");
                if (args == 1) {
                    return manager.boolean(true);
                } else {
                    storm::expressions::Expression retVal = this->translateExpression(expr.arg(0)) != this->translateExpression(expr.arg(1));
                    for (unsigned arg2 = 2; arg2 < args; ++arg2) {
                        retVal = retVal && (this->translateExpression(expr.arg(0)) != this->translateExpression(expr.arg(arg2)));
                    }
                    for (unsigned arg1 = 1; arg1 < args; ++arg1) {
                        for (unsigned arg2 = arg1 + 1; arg2 < args; ++arg2) {
                            retVal = retVal && (this->translateExpression(expr.arg(arg1)) != this->translateExpression(expr.arg(arg2)));
                        }
                    }
                    return retVal;
                }
            }
            case Z3_OP_ITE:
                return storm::expressions::ite(this->translateExpression(expr.arg(0)), this->translateExpression(expr.arg(1)),
                                               this->translateExpression(expr.arg(2)));
            case Z3_OP_AND: {
                unsigned args = expr.num_args();
                STORM_LOG_THROW(args != 0, storm::exceptions::ExpressionEvaluationException,
                                "Failed to convert Z3 expression. 0-ary AND is assumed to be an error.");
                if (args == 1) {
                    return this->translateExpression(expr.arg(0));
                } else {
                    storm::expressions::Expression retVal = this->translateExpression(expr.arg(0));
                    for (unsigned i = 1; i < args; i++) {
                        retVal = retVal && this->translateExpression(expr.arg(i));
                    }
                    return retVal;
                }
            }
            case Z3_OP_OR: {
                unsigned args = expr.num_args();
                STORM_LOG_THROW(args != 0, storm::exceptions::ExpressionEvaluationException,
                                "Failed to convert Z3 expression. 0-ary OR is assumed to be an error.");
                if (args == 1) {
                    return this->translateExpression(expr.arg(0));
                } else {
                    storm::expressions::Expression retVal = this->translateExpression(expr.arg(0));
                    for (unsigned i = 1; i < args; i++) {
                        retVal = retVal || this->translateExpression(expr.arg(i));
                    }
                    return retVal;
                }
            }
            case Z3_OP_IFF:
                return storm::expressions::iff(this->translateExpression(expr.arg(0)), this->translateExpression(expr.arg(1)));
            case Z3_OP_XOR:
                return storm::expressions::xclusiveor(this->translateExpression(expr.arg(0)), this->translateExpression(expr.arg(1)));
            case Z3_OP_NOT:
                return !this->translateExpression(expr.arg(0));
            case Z3_OP_IMPLIES:
                return storm::expressions::implies(this->translateExpression(expr.arg(0)), this->translateExpression(expr.arg(1)));
            case Z3_OP_LE:
                return this->translateExpression(expr.arg(0)) <= this->translateExpression(expr.arg(1));
            case Z3_OP_GE:
                return this->translateExpression(expr.arg(0)) >= this->translateExpression(expr.arg(1));
            case Z3_OP_LT:
                return this->translateExpression(expr.arg(0)) < this->translateExpression(expr.arg(1));
            case Z3_OP_GT:
                return this->translateExpression(expr.arg(0)) > this->translateExpression(expr.arg(1));
            case Z3_OP_ADD:
                return this->translateExpression(expr.arg(0)) + this->translateExpression(expr.arg(1));
            case Z3_OP_SUB:
                return this->translateExpression(expr.arg(0)) - this->translateExpression(expr.arg(1));
            case Z3_OP_UMINUS:
                return -this->translateExpression(expr.arg(0));
            case Z3_OP_MUL:
                return this->translateExpression(expr.arg(0)) * this->translateExpression(expr.arg(1));
            case Z3_OP_DIV:
                return this->translateExpression(expr.arg(0)) / this->translateExpression(expr.arg(1));
            case Z3_OP_IDIV:
                return this->translateExpression(expr.arg(0)) / this->translateExpression(expr.arg(1));
            case Z3_OP_ANUM:
                // Arithmetic numeral.
                if (expr.is_int() && expr.is_const()) {
                    Z3_SIGNED_INTEGER value;
                    if (Z3_get_numeral_int64(expr.ctx(), expr, &value)) {
                        return manager.integer(value);
                    } else {
                        STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                                        "Failed to convert Z3 expression. Expression is constant integer and value does not fit into 64-bit integer.");
                    }
                } else {
                    STORM_LOG_ASSERT(expr.is_real() && expr.is_const(), "Cannot handle numerical expression.");
                    Z3_SIGNED_INTEGER num;
                    Z3_SIGNED_INTEGER den;
                    if (Z3_get_numeral_rational_int64(expr.ctx(), expr, &num, &den)) {
                        return manager.rational(storm::utility::convertNumber<storm::RationalNumber>(static_cast<int_fast64_t>(num)) /
                                                storm::utility::convertNumber<storm::RationalNumber>(static_cast<int_fast64_t>(den)));
                    } else {
                        return manager.rational(storm::utility::convertNumber<storm::RationalNumber>(std::string(Z3_get_numeral_string(expr.ctx(), expr))));
                    }
                }
            case Z3_OP_AGNUM:
                STORM_LOG_WARN("Interpreting algebraic numbers as rational numbers. This is not an exact computation.");
                // Get the value of an algebraic number, converted to a rational, with precision 1/10^16.
                return manager.rational(storm::utility::convertNumber<storm::RationalNumber>(
                    std::string(Z3_get_numeral_string(expr.ctx(), Z3_get_algebraic_number_lower(expr.ctx(), expr, 16)))));
            case Z3_OP_UNINTERPRETED:
                // Currently, we only support uninterpreted constant functions.
                STORM_LOG_THROW(expr.is_const(), storm::exceptions::ExpressionEvaluationException,
                                "Failed to convert Z3 expression. Encountered non-constant uninterpreted function.");
                return manager.getVariable(expr.decl().name().str()).getExpression();
            default:
                STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                                "Failed to convert Z3 expression. Encountered unhandled Z3_decl_kind " << expr.decl().decl_kind() << ".");
                break;
        }
    } else {
        STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException, "Failed to convert Z3 expression. Encountered unknown expression type.");
    }
}

boost::any Z3ExpressionAdapter::visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    z3::expr leftResult = boost::any_cast<z3::expr>(expression.getFirstOperand()->accept(*this, data));
    z3::expr rightResult = boost::any_cast<z3::expr>(expression.getSecondOperand()->accept(*this, data));

    z3::expr result(context);
    switch (expression.getOperatorType()) {
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::And:
            result = leftResult && rightResult;
            break;
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Or:
            result = leftResult || rightResult;
            break;
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Xor:
            result = z3::expr(context, Z3_mk_xor(context, leftResult, rightResult));
            break;
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Implies:
            result = z3::expr(context, Z3_mk_implies(context, leftResult, rightResult));
            break;
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Iff:
            result = z3::expr(context, Z3_mk_iff(context, leftResult, rightResult));
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown boolean binary operator '" << static_cast<int>(expression.getOperatorType())
                                                                                            << "' in expression " << expression << ".");
    }

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Z3ExpressionAdapter::visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    z3::expr leftResult = boost::any_cast<z3::expr>(expression.getFirstOperand()->accept(*this, data));
    z3::expr rightResult = boost::any_cast<z3::expr>(expression.getSecondOperand()->accept(*this, data));

    z3::expr result(context);
    switch (expression.getOperatorType()) {
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Plus:
            result = leftResult + rightResult;
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Minus:
            result = leftResult - rightResult;
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Times:
            result = leftResult * rightResult;
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Divide:
            if (leftResult.is_int() && rightResult.is_int()) {
                leftResult = z3::expr(context, Z3_mk_int2real(context, leftResult));
            }
            result = leftResult / rightResult;
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Min:
            result = ite(leftResult <= rightResult, leftResult, rightResult);
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Max:
            result = ite(leftResult >= rightResult, leftResult, rightResult);
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Power:
            result = pw(leftResult, rightResult);
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown numerical binary operator '" << static_cast<int>(expression.getOperatorType())
                                                                                              << "' in expression " << expression << ".");
    }

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Z3ExpressionAdapter::visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    z3::expr leftResult = boost::any_cast<z3::expr>(expression.getFirstOperand()->accept(*this, data));
    z3::expr rightResult = boost::any_cast<z3::expr>(expression.getSecondOperand()->accept(*this, data));

    z3::expr result(context);
    switch (expression.getRelationType()) {
        case storm::expressions::RelationType::Equal:
            result = leftResult == rightResult;
            break;
        case storm::expressions::RelationType::NotEqual:
            result = leftResult != rightResult;
            break;
        case storm::expressions::RelationType::Less:
            result = leftResult < rightResult;
            break;
        case storm::expressions::RelationType::LessOrEqual:
            result = leftResult <= rightResult;
            break;
        case storm::expressions::RelationType::Greater:
            result = leftResult > rightResult;
            break;
        case storm::expressions::RelationType::GreaterOrEqual:
            result = leftResult >= rightResult;
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown boolean binary operator '" << static_cast<int>(expression.getRelationType())
                                                                                            << "' in expression " << expression << ".");
    }

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Z3ExpressionAdapter::visit(storm::expressions::BooleanLiteralExpression const& expression, boost::any const&) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    z3::expr result = context.bool_val(expression.getValue());

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Z3ExpressionAdapter::visit(storm::expressions::RationalLiteralExpression const& expression, boost::any const&) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    std::stringstream fractionStream;
    fractionStream << expression.getValue();
    z3::expr result = context.real_val(fractionStream.str().c_str());

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Z3ExpressionAdapter::visit(storm::expressions::IntegerLiteralExpression const& expression, boost::any const&) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    z3::expr result = context.int_val(static_cast<Z3_SIGNED_INTEGER>(expression.getValue()));

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Z3ExpressionAdapter::visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    z3::expr result = boost::any_cast<z3::expr>(expression.getOperand()->accept(*this, data));

    switch (expression.getOperatorType()) {
        case storm::expressions::UnaryBooleanFunctionExpression::OperatorType::Not:
            result = !result;
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown boolean binary operator '" << static_cast<int>(expression.getOperatorType())
                                                                                            << "' in expression " << expression << ".");
    }

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Z3ExpressionAdapter::visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    z3::expr result = boost::any_cast<z3::expr>(expression.getOperand()->accept(*this, data));

    switch (expression.getOperatorType()) {
        case storm::expressions::UnaryNumericalFunctionExpression::OperatorType::Minus:
            result = 0 - result;
            break;
        case storm::expressions::UnaryNumericalFunctionExpression::OperatorType::Floor: {
            storm::expressions::Variable freshAuxiliaryVariable = manager.declareFreshVariable(manager.getIntegerType(), true);
            z3::expr floorVariable = context.int_const(freshAuxiliaryVariable.getName().c_str());
            additionalAssertions.push_back(z3::expr(context, Z3_mk_int2real(context, floorVariable)) <= result &&
                                           result < (z3::expr(context, Z3_mk_int2real(context, floorVariable)) + 1));
            result = floorVariable;
            break;
        }
        case storm::expressions::UnaryNumericalFunctionExpression::OperatorType::Ceil: {
            storm::expressions::Variable freshAuxiliaryVariable = manager.declareFreshVariable(manager.getIntegerType(), true);
            z3::expr ceilVariable = context.int_const(freshAuxiliaryVariable.getName().c_str());
            additionalAssertions.push_back(z3::expr(context, Z3_mk_int2real(context, ceilVariable)) - 1 < result &&
                                           result <= z3::expr(context, Z3_mk_int2real(context, ceilVariable)));
            result = ceilVariable;
            break;
        }
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown numerical unary operator '" << static_cast<int>(expression.getOperatorType()) << "'.");
    }

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Z3ExpressionAdapter::visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    z3::expr conditionResult = boost::any_cast<z3::expr>(expression.getCondition()->accept(*this, data));
    z3::expr thenResult = boost::any_cast<z3::expr>(expression.getThenExpression()->accept(*this, data));
    z3::expr elseResult = boost::any_cast<z3::expr>(expression.getElseExpression()->accept(*this, data));
    z3::expr result = z3::expr(context, Z3_mk_ite(context, conditionResult, thenResult, elseResult));

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Z3ExpressionAdapter::visit(storm::expressions::VariableExpression const& expression, boost::any const&) {
    return this->translateExpression(expression.getVariable());
}

z3::expr Z3ExpressionAdapter::createVariable(storm::expressions::Variable const& variable) {
    z3::expr z3Variable(context);
    if (variable.getType().isBooleanType()) {
        z3Variable = context.bool_const(variable.getName().c_str());
    } else if (variable.getType().isIntegerType()) {
        z3Variable = context.int_const(variable.getName().c_str());
    } else if (variable.getType().isBitVectorType()) {
        z3Variable = context.bv_const(variable.getName().c_str(), variable.getType().getWidth());
    } else if (variable.getType().isRationalType()) {
        z3Variable = context.real_const(variable.getName().c_str());
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException,
                        "Encountered variable '" << variable.getName() << "' with unknown type while trying to create solver variables.");
    }
    variableToExpressionMapping.insert(std::make_pair(variable, z3Variable));
    declarationToVariableMapping.insert(std::make_pair(z3Variable.decl(), variable));
    return z3Variable;
}

#endif
}  // namespace adapters
}  // namespace storm
