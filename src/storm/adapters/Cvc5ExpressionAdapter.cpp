#include "storm/adapters/Cvc5ExpressionAdapter.h"

#include <cstdint>
#include <functional>
#include <string>

#include "storm/exceptions/ExpressionEvaluationException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace adapters {

#ifdef STORM_HAVE_CVC5

namespace {

/*!
 * Translates a numerical constant of CVC5 (given in its string representation) back to a Storm literal.
 *
 * CVC5 prints numerical constants as "42", "2.0", "(- 42)", "(/ 7 3)", or "(/ (- 3) 4)", i.e., the SMT-LIB notation
 * where negative numbers and fractions are wrapped in parentheses.
 *
 * @param manager The Storm expression manager that is used to build the literal.
 * @param stringRepresentation The string representation of the term.
 * @param isInteger Whether the term is of integer (as opposed to real) type.
 * @return The corresponding Storm literal.
 */
storm::expressions::Expression translateNumericalConstant(storm::expressions::ExpressionManager& manager, std::string const& stringRepresentation,
                                                          bool isInteger) {
    // Tokenize the string representation, e.g., "(- (/ 3 4))" results in the tokens "(-", "(/", "3", "4", ")", ")".
    std::vector<std::string> tokens;
    std::string currentToken;
    auto const flushToken = [&tokens, &currentToken]() {
        if (!currentToken.empty()) {
            tokens.push_back(currentToken);
            currentToken.clear();
        }
    };
    for (size_t i = 0; i < stringRepresentation.size(); ++i) {
        char const c = stringRepresentation[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            flushToken();
        } else if (c == '(') {
            flushToken();
            // The operators for negative numbers and fractions are two-character symbols, i.e., "(-" and "(/".
            if (i + 1 < stringRepresentation.size() && (stringRepresentation[i + 1] == '-' || stringRepresentation[i + 1] == '/')) {
                tokens.push_back(stringRepresentation.substr(i, 2));
                ++i;
            } else {
                tokens.push_back("(");
            }
        } else if (c == ')') {
            flushToken();
            tokens.push_back(")");
        } else {
            currentToken.push_back(c);
        }
    }
    flushToken();
    STORM_LOG_THROW(!tokens.empty(), storm::exceptions::ExpressionEvaluationException,
                    "Failed to convert CVC5 expression. Encountered empty numerical constant.");

    // CVC5 prints integer constants as a (possibly negated) plain number, e.g., "42" or "(- 42)".
    if (isInteger) {
        size_t position = 0;
        bool isNegative = tokens[position] == "(-";
        if (isNegative) {
            ++position;
        }
        STORM_LOG_THROW(position < tokens.size(), storm::exceptions::ExpressionEvaluationException,
                        "Failed to convert CVC5 expression. Encountered malformed integer constant " << stringRepresentation << ".");
        std::string const number = tokens[position];
        ++position;
        if (isNegative) {
            STORM_LOG_THROW(position < tokens.size() && tokens[position] == ")", storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert CVC5 expression. Encountered malformed integer constant " << stringRepresentation << ".");
            ++position;
        }
        STORM_LOG_THROW(position == tokens.size(), storm::exceptions::ExpressionEvaluationException,
                        "Failed to convert CVC5 expression. Encountered malformed integer constant " << stringRepresentation << ".");

        // Note that we negate the string instead of the parsed value because the latter may not be representable.
        int_fast64_t value = 0;
        try {
            value = std::stoll(isNegative ? "-" + number : number);
        } catch (std::logic_error const&) {
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert CVC5 expression. Integer constant " << stringRepresentation << " does not fit into a 64-bit integer.");
        }
        return manager.integer(value);
    }

    // CVC5 prints rational constants in the SMT-LIB notation, e.g., "2.0", "(- 1.5)", "(/ 7 3)", or "(/ (- 3) 4)".
    size_t position = 0;
    std::function<storm::RationalNumber(bool)> parse;
    parse = [&tokens, &position, &parse, &stringRepresentation](bool isNegative) -> storm::RationalNumber {
        STORM_LOG_THROW(position < tokens.size(), storm::exceptions::ExpressionEvaluationException,
                        "Failed to convert CVC5 expression. Encountered malformed rational constant " << stringRepresentation << ".");

        // A negative number is printed as "(- x)".
        if (tokens[position] == "(-") {
            STORM_LOG_THROW(!isNegative, storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert CVC5 expression. Encountered doubly negated rational constant " << stringRepresentation << ".");
            ++position;
            storm::RationalNumber result = parse(true);
            STORM_LOG_THROW(position < tokens.size() && tokens[position] == ")", storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert CVC5 expression. Encountered malformed rational constant " << stringRepresentation << ".");
            ++position;
            return result;
        }

        // A non-integral rational is printed as "(/ x y)", where a potential minus sign is part of the numerator.
        if (tokens[position] == "(/") {
            ++position;
            storm::RationalNumber numerator = parse(isNegative);
            storm::RationalNumber denominator = parse(false);
            STORM_LOG_THROW(position < tokens.size() && tokens[position] == ")", storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert CVC5 expression. Encountered malformed fraction " << stringRepresentation << ".");
            ++position;
            return numerator / denominator;
        }

        // Everything else is a plain (decimal) number.
        std::string const number = tokens[position];
        ++position;
        storm::RationalNumber const result = storm::utility::convertNumber<storm::RationalNumber>(number);
        return isNegative ? -result : result;
    };

    storm::RationalNumber const value = parse(false);
    STORM_LOG_THROW(position == tokens.size(), storm::exceptions::ExpressionEvaluationException,
                    "Failed to convert CVC5 expression. Encountered malformed rational constant " << stringRepresentation << ".");
    return manager.rational(value);
}

}  // namespace

Cvc5ExpressionAdapter::Cvc5ExpressionAdapter(storm::expressions::ExpressionManager& manager, cvc5::Solver& solver)
    : manager(manager), termManager(solver.getTermManager()), variableToExpressionMapping() {
    // Intentionally left empty.
}

cvc5::Term Cvc5ExpressionAdapter::translateExpression(storm::expressions::Expression const& expression) {
    STORM_LOG_ASSERT(expression.getManager() == this->manager, "Invalid expression for solver.");

    cvc5::Term result = boost::any_cast<cvc5::Term>(expression.getBaseExpression().accept(*this, boost::none));
    expressionCache.clear();
    return result;
}

cvc5::Term Cvc5ExpressionAdapter::translateExpression(storm::expressions::Variable const& variable) {
    STORM_LOG_ASSERT(variable.getManager() == this->manager, "Invalid expression for solver.");

    auto const& variableExpressionPair = variableToExpressionMapping.find(variable);
    if (variableExpressionPair == variableToExpressionMapping.end()) {
        return createVariable(variable);
    }

    return variableExpressionPair->second;
}

storm::expressions::Expression Cvc5ExpressionAdapter::translateExpression(cvc5::Term const& term) {
    // First, deal with the boolean constants, as they do not have a symbol that we could look up.
    if (term.getSort().isBoolean()) {
        if (term == this->termManager.mkBoolean(true)) {
            return this->manager.boolean(true);
        } else if (term == this->termManager.mkBoolean(false)) {
            return this->manager.boolean(false);
        }
    }

    // Second, deal with constants, i.e., variables and numerical constants.
    if (term.getKind() == cvc5::Kind::CONSTANT) {
        STORM_LOG_THROW(term.hasSymbol(), storm::exceptions::ExpressionEvaluationException,
                        "Failed to convert CVC5 expression. Encountered non-constant free constant.");
        return this->getVariable(term).getExpression();
    } else if (term.getKind() == cvc5::Kind::CONST_INTEGER || term.getKind() == cvc5::Kind::CONST_RATIONAL) {
        return translateNumericalConstant(this->manager, term.toString(), term.getKind() == cvc5::Kind::CONST_INTEGER);
    }

    switch (term.getKind()) {
        case cvc5::Kind::CONST_BOOLEAN:
            return term == this->termManager.mkBoolean(true) ? this->manager.boolean(true) : this->manager.boolean(false);
        case cvc5::Kind::EQUAL:
            return this->translateExpression(term[0]) == this->translateExpression(term[1]);
        case cvc5::Kind::DISTINCT: {
            STORM_LOG_THROW(term.getNumChildren() != 0, storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert CVC5 expression. DISTINCT (mutual !=) operator with 0-arity is assumed to be an error.");
            if (term.getNumChildren() == 1) {
                return this->manager.boolean(true);
            } else {
                storm::expressions::Expression retVal = this->translateExpression(term[0]) != this->translateExpression(term[1]);
                for (size_t arg2 = 2; arg2 < term.getNumChildren(); ++arg2) {
                    retVal = retVal && (this->translateExpression(term[0]) != this->translateExpression(term[arg2]));
                }
                for (size_t arg1 = 1; arg1 < term.getNumChildren(); ++arg1) {
                    for (size_t arg2 = arg1 + 1; arg2 < term.getNumChildren(); ++arg2) {
                        retVal = retVal && (this->translateExpression(term[arg1]) != this->translateExpression(term[arg2]));
                    }
                }
                return retVal;
            }
        }
        case cvc5::Kind::ITE:
            return storm::expressions::ite(this->translateExpression(term[0]), this->translateExpression(term[1]), this->translateExpression(term[2]));
        case cvc5::Kind::AND: {
            STORM_LOG_THROW(term.getNumChildren() != 0, storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert CVC5 expression. 0-ary AND is assumed to be an error.");
            if (term.getNumChildren() == 1) {
                return this->translateExpression(term[0]);
            } else {
                storm::expressions::Expression retVal = this->translateExpression(term[0]);
                for (size_t i = 1; i < term.getNumChildren(); ++i) {
                    retVal = retVal && this->translateExpression(term[i]);
                }
                return retVal;
            }
        }
        case cvc5::Kind::OR: {
            STORM_LOG_THROW(term.getNumChildren() != 0, storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert CVC5 expression. 0-ary OR is assumed to be an error.");
            if (term.getNumChildren() == 1) {
                return this->translateExpression(term[0]);
            } else {
                storm::expressions::Expression retVal = this->translateExpression(term[0]);
                for (size_t i = 1; i < term.getNumChildren(); ++i) {
                    retVal = retVal || this->translateExpression(term[i]);
                }
                return retVal;
            }
        }
        case cvc5::Kind::XOR:
            return storm::expressions::xclusiveor(this->translateExpression(term[0]), this->translateExpression(term[1]));
        case cvc5::Kind::NOT:
            return !this->translateExpression(term[0]);
        case cvc5::Kind::IMPLIES:
            return storm::expressions::implies(this->translateExpression(term[0]), this->translateExpression(term[1]));
        case cvc5::Kind::LEQ:
            return this->translateExpression(term[0]) <= this->translateExpression(term[1]);
        case cvc5::Kind::GEQ:
            return this->translateExpression(term[0]) >= this->translateExpression(term[1]);
        case cvc5::Kind::LT:
            return this->translateExpression(term[0]) < this->translateExpression(term[1]);
        case cvc5::Kind::GT:
            return this->translateExpression(term[0]) > this->translateExpression(term[1]);
        case cvc5::Kind::ADD:
            return this->translateExpression(term[0]) + this->translateExpression(term[1]);
        case cvc5::Kind::SUB:
            return this->translateExpression(term[0]) - this->translateExpression(term[1]);
        case cvc5::Kind::NEG:
            return -this->translateExpression(term[0]);
        case cvc5::Kind::MULT:
            return this->translateExpression(term[0]) * this->translateExpression(term[1]);
        case cvc5::Kind::DIVISION:
        case cvc5::Kind::INTS_DIVISION:
            return this->translateExpression(term[0]) / this->translateExpression(term[1]);
        case cvc5::Kind::POW:
            return storm::expressions::pow(this->translateExpression(term[0]), this->translateExpression(term[1]));
        case cvc5::Kind::TO_REAL:
            return this->translateExpression(term[0]);
        case cvc5::Kind::TO_INTEGER:
            // CVC5's to_int operation applies the floor function.
            return storm::expressions::floor(this->translateExpression(term[0]));
        case cvc5::Kind::ABS:
            return storm::expressions::abs(this->translateExpression(term[0]));
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert CVC5 expression. Encountered unhandled kind " << term.getKind() << ".");
            break;
    }
    STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException, "Failed to convert CVC5 expression.");
}

storm::expressions::Variable const& Cvc5ExpressionAdapter::getVariable(cvc5::Term const& constant) {
    auto const& constantVariablePair = expressionToVariableMapping.find(constant);
    STORM_LOG_ASSERT(constantVariablePair != expressionToVariableMapping.end(), "Unable to find the variable corresponding to the given constant.");
    return constantVariablePair->second;
}

boost::any Cvc5ExpressionAdapter::visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    cvc5::Term leftResult = boost::any_cast<cvc5::Term>(expression.getFirstOperand()->accept(*this, data));
    cvc5::Term rightResult = boost::any_cast<cvc5::Term>(expression.getSecondOperand()->accept(*this, data));

    cvc5::Kind kind;
    switch (expression.getOperatorType()) {
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::And:
            kind = cvc5::Kind::AND;
            break;
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Or:
            kind = cvc5::Kind::OR;
            break;
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Xor:
            kind = cvc5::Kind::XOR;
            break;
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Implies:
            kind = cvc5::Kind::IMPLIES;
            break;
        case storm::expressions::BinaryBooleanFunctionExpression::OperatorType::Iff:
            kind = cvc5::Kind::EQUAL;
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown boolean binary operator '" << static_cast<int>(expression.getOperatorType())
                                                                                            << "' in expression " << expression << ".");
    }

    cvc5::Term result = this->termManager.mkTerm(kind, {leftResult, rightResult});

    expressionCache.emplace(&expression, result);
    return result;
}

cvc5::Term Cvc5ExpressionAdapter::convertToRealIfExpected(cvc5::Term const& term, bool const& isRealExpected) const {
    if (isRealExpected && term.getSort().isInteger()) {
        return this->termManager.mkTerm(cvc5::Kind::TO_REAL, {term});
    }
    return term;
}

void Cvc5ExpressionAdapter::unifyNumericalTypes(cvc5::Term& leftTerm, cvc5::Term& rightTerm) const {
    if (leftTerm.getSort().isInteger() && rightTerm.getSort().isReal()) {
        leftTerm = this->termManager.mkTerm(cvc5::Kind::TO_REAL, {leftTerm});
    } else if (rightTerm.getSort().isInteger() && leftTerm.getSort().isReal()) {
        rightTerm = this->termManager.mkTerm(cvc5::Kind::TO_REAL, {rightTerm});
    }
}

boost::any Cvc5ExpressionAdapter::visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    cvc5::Term leftResult = boost::any_cast<cvc5::Term>(expression.getFirstOperand()->accept(*this, data));
    cvc5::Term rightResult = boost::any_cast<cvc5::Term>(expression.getSecondOperand()->accept(*this, data));

    // CVC5 requires the operands of an arithmetic operation to be of the same type. Where Storm admits a
    // mixed expression because the result is a real number, we convert the integer operand accordingly.
    bool const isRealExpected = expression.getType().isRationalType();
    leftResult = this->convertToRealIfExpected(leftResult, isRealExpected);
    rightResult = this->convertToRealIfExpected(rightResult, isRealExpected);

    cvc5::Term result;
    switch (expression.getOperatorType()) {
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Plus:
            result = this->termManager.mkTerm(cvc5::Kind::ADD, {leftResult, rightResult});
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Minus:
            result = this->termManager.mkTerm(cvc5::Kind::SUB, {leftResult, rightResult});
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Times:
            result = this->termManager.mkTerm(cvc5::Kind::MULT, {leftResult, rightResult});
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Divide:
            // Both operands have already been converted above, since a Storm division always yields a real number.
            result = this->termManager.mkTerm(cvc5::Kind::DIVISION, {leftResult, rightResult});
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Min:
            result = this->termManager.mkTerm(cvc5::Kind::ITE, {this->termManager.mkTerm(cvc5::Kind::LEQ, {leftResult, rightResult}), leftResult, rightResult});
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Max:
            result = this->termManager.mkTerm(cvc5::Kind::ITE, {this->termManager.mkTerm(cvc5::Kind::GEQ, {leftResult, rightResult}), leftResult, rightResult});
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Power:
            // CVC5 requires both operands of a power to be of the same sort. Integers have already been converted
            // above if the power yields a real number, so we only have to take care of mixed operands.
            this->unifyNumericalTypes(leftResult, rightResult);
            result = this->termManager.mkTerm(cvc5::Kind::POW, {leftResult, rightResult});
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown numerical binary operator '" << static_cast<int>(expression.getOperatorType())
                                                                                              << "' in expression " << expression << ".");
    }

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Cvc5ExpressionAdapter::visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    cvc5::Term leftResult = boost::any_cast<cvc5::Term>(expression.getFirstOperand()->accept(*this, data));
    cvc5::Term rightResult = boost::any_cast<cvc5::Term>(expression.getSecondOperand()->accept(*this, data));

    // CVC5 requires the operands of a comparison to be of the same type, so a real number may be compared with an
    // integer by converting the latter to a real number.
    this->unifyNumericalTypes(leftResult, rightResult);

    cvc5::Kind kind;
    switch (expression.getRelationType()) {
        case storm::expressions::RelationType::Equal:
            kind = cvc5::Kind::EQUAL;
            break;
        case storm::expressions::RelationType::NotEqual:
            kind = cvc5::Kind::DISTINCT;
            break;
        case storm::expressions::RelationType::Less:
            kind = cvc5::Kind::LT;
            break;
        case storm::expressions::RelationType::LessOrEqual:
            kind = cvc5::Kind::LEQ;
            break;
        case storm::expressions::RelationType::Greater:
            kind = cvc5::Kind::GT;
            break;
        case storm::expressions::RelationType::GreaterOrEqual:
            kind = cvc5::Kind::GEQ;
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown boolean binary operator '" << static_cast<int>(expression.getRelationType())
                                                                                            << "' in expression " << expression << ".");
    }

    cvc5::Term result = this->termManager.mkTerm(kind, {leftResult, rightResult});

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Cvc5ExpressionAdapter::visit(storm::expressions::BooleanLiteralExpression const& expression, boost::any const&) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    cvc5::Term result = this->termManager.mkBoolean(expression.getValue());

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Cvc5ExpressionAdapter::visit(storm::expressions::RationalLiteralExpression const& expression, boost::any const&) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    cvc5::Term result = this->createRational(expression.getValue());

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Cvc5ExpressionAdapter::visit(storm::expressions::IntegerLiteralExpression const& expression, boost::any const&) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    cvc5::Term result = this->termManager.mkInteger(static_cast<int64_t>(expression.getValue()));

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Cvc5ExpressionAdapter::visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    cvc5::Term result = boost::any_cast<cvc5::Term>(expression.getOperand()->accept(*this, data));

    switch (expression.getOperatorType()) {
        case storm::expressions::UnaryBooleanFunctionExpression::OperatorType::Not:
            result = this->termManager.mkTerm(cvc5::Kind::NOT, {result});
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown boolean unary operator '" << static_cast<int>(expression.getOperatorType())
                                                                                           << "' in expression " << expression << ".");
    }

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Cvc5ExpressionAdapter::visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    cvc5::Term result = boost::any_cast<cvc5::Term>(expression.getOperand()->accept(*this, data));

    switch (expression.getOperatorType()) {
        case storm::expressions::UnaryNumericalFunctionExpression::OperatorType::Minus:
            result = this->termManager.mkTerm(cvc5::Kind::NEG, {result});
            break;
        case storm::expressions::UnaryNumericalFunctionExpression::OperatorType::Floor: {
            // CVC5's to_int operation converts a real number to an integer using the floor function.
            result = this->termManager.mkTerm(cvc5::Kind::TO_INTEGER, {result});
            break;
        }
        case storm::expressions::UnaryNumericalFunctionExpression::OperatorType::Ceil: {
            // The ceiling of a real number is the negation of the floor of its negation.
            result = this->termManager.mkTerm(cvc5::Kind::NEG,
                                              {this->termManager.mkTerm(cvc5::Kind::TO_INTEGER, {this->termManager.mkTerm(cvc5::Kind::NEG, {result})})});
            break;
        }
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Cannot evaluate expression: unknown numerical unary operator '" << static_cast<int>(expression.getOperatorType())
                                                                                             << "' in expression " << expression << ".");
    }

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Cvc5ExpressionAdapter::visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) {
    auto cacheIt = expressionCache.find(&expression);
    if (cacheIt != expressionCache.end()) {
        return cacheIt->second;
    }

    cvc5::Term conditionResult = boost::any_cast<cvc5::Term>(expression.getCondition()->accept(*this, data));
    cvc5::Term thenResult = boost::any_cast<cvc5::Term>(expression.getThenExpression()->accept(*this, data));
    cvc5::Term elseResult = boost::any_cast<cvc5::Term>(expression.getElseExpression()->accept(*this, data));
    cvc5::Term result = this->termManager.mkTerm(cvc5::Kind::ITE, {conditionResult, thenResult, elseResult});

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Cvc5ExpressionAdapter::visit(storm::expressions::VariableExpression const& expression, boost::any const&) {
    return this->translateExpression(expression.getVariable());
}

cvc5::Term Cvc5ExpressionAdapter::createRational(storm::RationalNumber const& value) {
    // CVC5 accepts rationals in the SMT-LIB format, which is exactly the format in which GMP prints them.
    std::stringstream fractionStream;
    fractionStream << value;
    return this->termManager.mkReal(fractionStream.str());
}

cvc5::Term Cvc5ExpressionAdapter::createVariable(storm::expressions::Variable const& variable) {
    cvc5::Sort sort;
    if (variable.getType().isBooleanType()) {
        sort = this->termManager.getBooleanSort();
    } else if (variable.getType().isIntegerType()) {
        sort = this->termManager.getIntegerSort();
    } else if (variable.getType().isBitVectorType()) {
        sort = this->termManager.mkBitVectorSort(variable.getType().getWidth());
    } else if (variable.getType().isRationalType()) {
        sort = this->termManager.getRealSort();
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException,
                        "Encountered variable '" << variable.getName() << "' with unknown type while trying to create solver variables.");
    }
    cvc5::Term cvc5Variable = this->termManager.mkConst(sort, variable.getName());
    variableToExpressionMapping.insert(std::make_pair(variable, cvc5Variable));
    expressionToVariableMapping.insert(std::make_pair(cvc5Variable, variable));
    return cvc5Variable;
}

#endif
}  // namespace adapters
}  // namespace storm
