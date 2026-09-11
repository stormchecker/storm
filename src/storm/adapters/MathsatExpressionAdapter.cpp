#include "MathsatExpressionAdapter.h"

#include <cstdint>
#include <sstream>
#include <string>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/ExpressionEvaluationException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

#ifdef STORM_HAVE_MATHSAT

bool operator==(msat_decl decl1, msat_decl decl2) {
    return decl1.repr == decl2.repr;
}

namespace storm {
namespace adapters {

MathsatExpressionAdapter::MathsatExpressionAdapter(storm::expressions::ExpressionManager& manager, msat_env& env)
    : manager(manager), env(env), variableToDeclarationMapping() {
    // Intentionally left empty.
}

msat_term MathsatExpressionAdapter::translateExpression(storm::expressions::Expression const& expression) {
    additionalConstraints.clear();
    msat_term result = boost::any_cast<msat_term>(expression.getBaseExpression().accept(*this, boost::none));
    if (MSAT_ERROR_TERM(result)) {
        std::string errorMessage(msat_last_error_message(env));
        STORM_LOG_THROW(!MSAT_ERROR_TERM(result), storm::exceptions::ExpressionEvaluationException,
                        "Could not translate expression to MathSAT's format. (Message: " << errorMessage << ").");
    }

    return result;
}

msat_term MathsatExpressionAdapter::translateExpression(storm::expressions::Variable const& variable) {
    STORM_LOG_ASSERT(variable.getManager() == this->manager, "Invalid expression for solver.");

    auto const& variableExpressionPair = variableToDeclarationMapping.find(variable);
    if (variableExpressionPair == variableToDeclarationMapping.end()) {
        return msat_make_constant(env, createVariable(variable));
    }
    return msat_make_constant(env, variableExpressionPair->second);
}

bool MathsatExpressionAdapter::hasAdditionalConstraints() const {
    return !additionalConstraints.empty();
}

std::vector<msat_term> const& MathsatExpressionAdapter::getAdditionalConstraints() const {
    return additionalConstraints;
}

storm::expressions::Variable const& MathsatExpressionAdapter::getVariable(msat_decl msatVariableDeclaration) const {
    auto const& declarationVariablePair = declarationToVariableMapping.find(msatVariableDeclaration);
    STORM_LOG_ASSERT(declarationVariablePair != declarationToVariableMapping.end(), "Unknown variable declaration.");
    return declarationVariablePair->second;
}

std::unordered_map<storm::expressions::Variable, msat_decl> const& MathsatExpressionAdapter::getAllDeclaredVariables() const {
    return variableToDeclarationMapping;
}

boost::any MathsatExpressionAdapter::visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    msat_term leftResult = boost::any_cast<msat_term>(expression.getFirstOperand()->accept(*this, data));
    msat_term rightResult = boost::any_cast<msat_term>(expression.getSecondOperand()->accept(*this, data));

    switch (expression.getOperatorType()) {
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::And:
            return msat_make_and(env, leftResult, rightResult);
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Or:
            return msat_make_or(env, leftResult, rightResult);
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Iff:
            return msat_make_iff(env, leftResult, rightResult);
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Implies:
            return msat_make_or(env, msat_make_not(env, leftResult), rightResult);
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown boolean binary operator '" << static_cast<uint_fast64_t>(expression.getOperatorType())
                                                                                            << "' in expression " << expression << ".");
    }
}

boost::any MathsatExpressionAdapter::visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    msat_term leftResult = boost::any_cast<msat_term>(expression.getFirstOperand()->accept(*this, data));
    msat_term rightResult = boost::any_cast<msat_term>(expression.getSecondOperand()->accept(*this, data));

    msat_term result = leftResult;
    int_fast64_t exponent;
    int_fast64_t modulus;
    storm::expressions::Variable freshAuxiliaryVariable;
    msat_term modVariable;
    msat_term lower;
    msat_term upper;
    typename storm::NumberTraits<storm::GmpRationalNumber>::IntegerType gmpModulus;
    switch (expression.getOperatorType()) {
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Plus:
            return msat_make_plus(env, leftResult, rightResult);
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Minus:
            return msat_make_plus(env, leftResult, msat_make_times(env, msat_make_number(env, "-1"), rightResult));
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Times:
            return msat_make_times(env, leftResult, rightResult);
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Divide:
            return msat_make_divide(env, leftResult, rightResult);
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Min:
            return msat_make_term_ite(env, msat_make_leq(env, leftResult, rightResult), leftResult, rightResult);
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Max:
            return msat_make_term_ite(env, msat_make_leq(env, leftResult, rightResult), rightResult, leftResult);
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Power:
            exponent = expression.getSecondOperand()->evaluateAsInt();
            STORM_LOG_THROW(exponent >= 0, storm::exceptions::ExpressionEvaluationException, "Cannot evaluate expression with negative exponent.");
            --exponent;
            if (exponent > 0) {
                for (; exponent > 0; --exponent) {
                    result = msat_make_times(env, result, leftResult);
                }
            }
            return result;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Modulo:
            modulus = expression.getSecondOperand()->evaluateAsInt();
            STORM_LOG_THROW(modulus > 0, storm::exceptions::ExpressionEvaluationException, "Cannot evaluate expression with negative modulus.");

            freshAuxiliaryVariable = manager.declareFreshVariable(manager.getIntegerType(), true);
            modVariable = msat_make_constant(env, createVariable(freshAuxiliaryVariable));

            gmpModulus = typename storm::NumberTraits<storm::GmpRationalNumber>::IntegerType(static_cast<unsigned>(modulus));

            // Create the constraint that fixes the value of the fresh variable.
            additionalConstraints.push_back(msat_make_int_modular_congruence(env, gmpModulus.get_mpz_t(), modVariable, leftResult));

            // Create the constraint that limits the value of the modulo operation to 0 <= val <= modulus-1.
            lower = msat_make_number(env, "-1");
            upper = msat_make_number(env, std::to_string(modulus - 1).c_str());
            additionalConstraints.push_back(
                msat_make_and(env, msat_make_not(env, msat_make_leq(env, modVariable, lower)), msat_make_leq(env, modVariable, upper)));
            return modVariable;
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown numerical binary operator '" << static_cast<uint_fast64_t>(expression.getOperatorType())
                                                                                              << "' in expression " << expression << ".");
    }
}

boost::any MathsatExpressionAdapter::visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) {
    msat_term leftResult = boost::any_cast<msat_term>(expression.getFirstOperand()->accept(*this, data));
    msat_term rightResult = boost::any_cast<msat_term>(expression.getSecondOperand()->accept(*this, data));

    switch (expression.getRelationType()) {
        case storm::expressions::RelationType::Equal:
            if (expression.getFirstOperand()->getType().isBooleanType() && expression.getSecondOperand()->getType().isBooleanType()) {
                return msat_make_iff(env, leftResult, rightResult);
            } else {
                return msat_make_equal(env, leftResult, rightResult);
            }
        case storm::expressions::RelationType::NotEqual:
            if (expression.getFirstOperand()->getType().isBooleanType() && expression.getSecondOperand()->getType().isBooleanType()) {
                return msat_make_not(env, msat_make_iff(env, leftResult, rightResult));
            } else {
                return msat_make_not(env, msat_make_equal(env, leftResult, rightResult));
            }
        case storm::expressions::RelationType::Less:
            return msat_make_and(env, msat_make_not(env, msat_make_equal(env, leftResult, rightResult)), msat_make_leq(env, leftResult, rightResult));
        case storm::expressions::RelationType::LessOrEqual:
            return msat_make_leq(env, leftResult, rightResult);
        case storm::expressions::RelationType::Greater:
            return msat_make_not(env, msat_make_leq(env, leftResult, rightResult));
        case storm::expressions::RelationType::GreaterOrEqual:
            return msat_make_or(env, msat_make_equal(env, leftResult, rightResult), msat_make_not(env, msat_make_leq(env, leftResult, rightResult)));
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown boolean binary operator '" << static_cast<uint_fast64_t>(expression.getRelationType())
                                                                                            << "' in expression " << expression << ".");
    }
}

boost::any MathsatExpressionAdapter::visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) {
    msat_term conditionResult = boost::any_cast<msat_term>(expression.getCondition()->accept(*this, data));
    msat_term thenResult = boost::any_cast<msat_term>(expression.getThenExpression()->accept(*this, data));
    msat_term elseResult = boost::any_cast<msat_term>(expression.getElseExpression()->accept(*this, data));

    // MathSAT does not allow ite with boolean arguments, so we have to encode it ourselves.
    if (expression.getThenExpression()->hasBooleanType() && expression.getElseExpression()->hasBooleanType()) {
        return msat_make_and(env, msat_make_or(env, msat_make_not(env, conditionResult), thenResult), msat_make_or(env, conditionResult, elseResult));
    } else {
        return msat_make_term_ite(env, conditionResult, thenResult, elseResult);
    }
}

boost::any MathsatExpressionAdapter::visit(storm::expressions::BooleanLiteralExpression const& expression, boost::any const&) {
    return expression.getValue() ? msat_make_true(env) : msat_make_false(env);
}

boost::any MathsatExpressionAdapter::visit(storm::expressions::RationalLiteralExpression const& expression, boost::any const&) {
    std::stringstream fractionStream;
    fractionStream << expression.getValue();
    return msat_make_number(env, fractionStream.str().c_str());
}

boost::any MathsatExpressionAdapter::visit(storm::expressions::IntegerLiteralExpression const& expression, boost::any const&) {
    return msat_make_number(env, std::to_string(static_cast<int>(expression.getValue())).c_str());
}

boost::any MathsatExpressionAdapter::visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    msat_term childResult = boost::any_cast<msat_term>(expression.getOperand()->accept(*this, data));

    switch (expression.getOperatorType()) {
        case storm::expressions::UnaryBooleanFunctionExpression::OperatorType::Not:
            return msat_make_not(env, childResult);
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown boolean unary operator: '" << static_cast<uint_fast64_t>(expression.getOperatorType())
                                                                                            << "' in expression " << expression << ".");
    }
}

boost::any MathsatExpressionAdapter::visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    msat_term childResult = boost::any_cast<msat_term>(expression.getOperand()->accept(*this, data));

    switch (expression.getOperatorType()) {
        case storm::expressions::UnaryNumericalFunctionExpression::OperatorType::Minus:
            return msat_make_times(env, msat_make_number(env, "-1"), childResult);
        case storm::expressions::UnaryNumericalFunctionExpression::OperatorType::Floor:
            return msat_make_floor(env, childResult);
        case storm::expressions::UnaryNumericalFunctionExpression::OperatorType::Ceil:
            // Mathsat does not support ceil... but ceil(x) = -floor(-x)  wheeii \o/
            return msat_make_times(env, msat_make_number(env, "-1"), msat_make_floor(env, msat_make_times(env, msat_make_number(env, "-1"), childResult)));
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown numerical unary operator: '" << static_cast<uint_fast64_t>(expression.getOperatorType())
                                                                                              << "' in expression " << expression << ".");
    }
}

boost::any MathsatExpressionAdapter::visit(storm::expressions::VariableExpression const& expression, boost::any const&) {
    return translateExpression(expression.getVariable());
}

storm::expressions::Expression MathsatExpressionAdapter::translateExpression(msat_term const& term) {
    if (msat_term_is_and(env, term)) {
        return translateExpression(msat_term_get_arg(term, 0)) && translateExpression(msat_term_get_arg(term, 1));
    } else if (msat_term_is_or(env, term)) {
        return translateExpression(msat_term_get_arg(term, 0)) || translateExpression(msat_term_get_arg(term, 1));
    } else if (msat_term_is_iff(env, term)) {
        return storm::expressions::iff(translateExpression(msat_term_get_arg(term, 0)), translateExpression(msat_term_get_arg(term, 1)));
    } else if (msat_term_is_not(env, term)) {
        return !translateExpression(msat_term_get_arg(term, 0));
    } else if (msat_term_is_plus(env, term)) {
        return translateExpression(msat_term_get_arg(term, 0)) + translateExpression(msat_term_get_arg(term, 1));
    } else if (msat_term_is_times(env, term)) {
        return translateExpression(msat_term_get_arg(term, 0)) * translateExpression(msat_term_get_arg(term, 1));
    } else if (msat_term_is_equal(env, term)) {
        return translateExpression(msat_term_get_arg(term, 0)) == translateExpression(msat_term_get_arg(term, 1));
    } else if (msat_term_is_leq(env, term)) {
        return translateExpression(msat_term_get_arg(term, 0)) <= translateExpression(msat_term_get_arg(term, 1));
    } else if (msat_term_is_true(env, term)) {
        return manager.boolean(true);
    } else if (msat_term_is_false(env, term)) {
        return manager.boolean(false);
    } else if (msat_term_is_constant(env, term)) {
        char* name = msat_decl_get_name(msat_term_get_decl(term));
        std::string nameString(name);
        storm::expressions::Expression result = manager.getVariableExpression(nameString.substr(0, nameString.find('/')));
        msat_free(name);
        return result;
    } else if (msat_term_is_number(env, term)) {
        char* termAsCString = msat_term_repr(term);
        std::string termString(termAsCString);
        msat_free(termAsCString);
        if (msat_is_integer_type(env, msat_term_get_type(term))) {
            return manager.integer(std::stoll(msat_term_repr(term)));
        } else if (msat_is_rational_type(env, msat_term_get_type(term))) {
            return manager.rational(storm::utility::convertNumber<storm::RationalNumber>(termString));
        }
    } else if (msat_term_is_term_ite(env, term)) {
        return storm::expressions::ite(translateExpression(msat_term_get_arg(term, 0)), translateExpression(msat_term_get_arg(term, 1)),
                                       translateExpression(msat_term_get_arg(term, 2)));
    }

    // If all other cases did not apply, we cannot represent the term in our expression framework.
    char* termAsCString = msat_term_repr(term);
    std::string termString(termAsCString);
    msat_free(termAsCString);
    STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException, "Cannot translate expression: unknown term: '" << termString << "'.");
}

msat_decl MathsatExpressionAdapter::createVariable(storm::expressions::Variable const& variable) {
    msat_decl msatDeclaration;
    if (variable.getType().isBooleanType()) {
        msatDeclaration = msat_declare_function(env, variable.getName().c_str(), msat_get_bool_type(env));
    } else if (variable.getType().isIntegerType()) {
        msatDeclaration = msat_declare_function(env, variable.getName().c_str(), msat_get_integer_type(env));
    } else if (variable.getType().isBitVectorType()) {
        msatDeclaration = msat_declare_function(env, variable.getName().c_str(), msat_get_bv_type(env, variable.getType().getWidth()));
    } else if (variable.getType().isRationalType()) {
        msatDeclaration = msat_declare_function(env, variable.getName().c_str(), msat_get_rational_type(env));
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException,
                        "Encountered variable '" << variable.getName() << "' with unknown type while trying to create solver variables.");
    }
    variableToDeclarationMapping.insert(std::make_pair(variable, msatDeclaration));
    declarationToVariableMapping.insert(std::make_pair(msatDeclaration, variable));
    return msatDeclaration;
}

}  // namespace adapters
}  // namespace storm

#endif  // STORM_HAVE_MATHSAT
