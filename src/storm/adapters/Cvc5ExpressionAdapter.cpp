#include "storm/adapters/Cvc5ExpressionAdapter.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "storm/exceptions/ExpressionEvaluationException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/expressions/ExpressionManager.h"
#include "storm/expressions/Expressions.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::adapters {

#ifdef STORM_HAVE_CVC5

namespace {

/*!
 * Translates a numerical constant of cvc5 (given in its string representation) back to a Storm literal.
 *
 * cvc5 prints numerical constants as "42", "2.0", "(- 42)", "(/ 7 3)", or "(/ (- 3) 4)", i.e., the SMT-LIB notation
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
    // The tokens are views into stringRepresentation, which outlives this function body.
    std::vector<std::string_view> tokens;
    std::string_view const input(stringRepresentation);
    size_t tokenStart = 0;
    bool inToken = false;
    auto const flushToken = [&](size_t end) {
        if (inToken) {
            tokens.push_back(input.substr(tokenStart, end - tokenStart));
            inToken = false;
        }
    };
    for (size_t i = 0; i < input.size(); ++i) {
        char const c = input[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            flushToken(i);
        } else if (c == '(') {
            flushToken(i);
            // The operators for negative numbers and fractions are two-character symbols, i.e., "(-" and "(/".
            if (i + 1 < input.size() && (input[i + 1] == '-' || input[i + 1] == '/')) {
                tokens.push_back(input.substr(i, 2));
                ++i;
            } else {
                tokens.push_back(input.substr(i, 1));
            }
        } else if (c == ')') {
            flushToken(i);
            tokens.push_back(input.substr(i, 1));
        } else if (!inToken) {
            inToken = true;
            tokenStart = i;
        }
    }
    flushToken(input.size());
    STORM_LOG_THROW(!tokens.empty(), storm::exceptions::ExpressionEvaluationException,
                    "Failed to convert cvc5 expression. Encountered empty numerical constant.");

    // cvc5 prints integer constants as a (possibly negated) plain number, e.g., "42" or "(- 42)".
    if (isInteger) {
        size_t position = 0;
        bool isNegative = tokens[position] == "(-";
        if (isNegative) {
            ++position;
        }
        STORM_LOG_THROW(position < tokens.size(), storm::exceptions::ExpressionEvaluationException,
                        "Failed to convert cvc5 expression. Encountered malformed integer constant " << stringRepresentation << ".");
        std::string const number(tokens[position]);
        ++position;
        if (isNegative) {
            STORM_LOG_THROW(position < tokens.size() && tokens[position] == ")", storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert cvc5 expression. Encountered malformed integer constant " << stringRepresentation << ".");
            ++position;
        }
        STORM_LOG_THROW(position == tokens.size(), storm::exceptions::ExpressionEvaluationException,
                        "Failed to convert cvc5 expression. Encountered malformed integer constant " << stringRepresentation << ".");

        // cvc5 integers are unbounded, so the value may not fit. We parse the signed string (rather than negating the parsed value)
        // so that the lowest 64-bit integer is handled correctly.
        int64_t value = 0;
        try {
            value = std::stoll(isNegative ? "-" + number : number);
        } catch (std::logic_error const&) {
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert cvc5 expression. Integer constant " << stringRepresentation << " does not fit into a 64-bit integer.");
        }
        return manager.integer(value);
    }

    // cvc5 prints rational constants in the SMT-LIB notation, e.g., "2.0", "(- 1.5)", "(/ 7 3)", or "(/ (- 3) 4)".
    size_t position = 0;
    std::function<storm::RationalNumber(bool)> parse;
    parse = [&tokens, &position, &parse, &stringRepresentation](bool isNegative) -> storm::RationalNumber {
        STORM_LOG_THROW(position < tokens.size(), storm::exceptions::ExpressionEvaluationException,
                        "Failed to convert cvc5 expression. Encountered malformed rational constant " << stringRepresentation << ".");

        // A negative number is printed as "(- x)".
        if (tokens[position] == "(-") {
            STORM_LOG_THROW(!isNegative, storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert cvc5 expression. Encountered doubly negated rational constant " << stringRepresentation << ".");
            ++position;
            storm::RationalNumber result = parse(true);
            STORM_LOG_THROW(position < tokens.size() && tokens[position] == ")", storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert cvc5 expression. Encountered malformed rational constant " << stringRepresentation << ".");
            ++position;
            return result;
        }

        // A non-integral rational is printed as "(/ x y)", where a potential minus sign is part of the numerator.
        if (tokens[position] == "(/") {
            ++position;
            storm::RationalNumber numerator = parse(isNegative);
            storm::RationalNumber denominator = parse(false);
            STORM_LOG_THROW(position < tokens.size() && tokens[position] == ")", storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert cvc5 expression. Encountered malformed fraction " << stringRepresentation << ".");
            ++position;
            return numerator / denominator;
        }

        // Everything else is a plain (decimal) number.
        std::string const number(tokens[position]);
        ++position;
        storm::RationalNumber const result = storm::utility::convertNumber<storm::RationalNumber>(std::string(number));
        return isNegative ? -result : result;
    };

    storm::RationalNumber const value = parse(false);
    STORM_LOG_THROW(position == tokens.size(), storm::exceptions::ExpressionEvaluationException,
                    "Failed to convert cvc5 expression. Encountered malformed rational constant " << stringRepresentation << ".");
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
                        "Failed to convert cvc5 expression. Encountered a constant without a symbol.");
        return this->getVariable(term).getExpression();
    } else if (term.getKind() == cvc5::Kind::CONST_INTEGER || term.getKind() == cvc5::Kind::CONST_RATIONAL) {
        return translateNumericalConstant(this->manager, term.toString(), term.getKind() == cvc5::Kind::CONST_INTEGER);
    }

    switch (term.getKind()) {
        case cvc5::Kind::CONST_BOOLEAN:
            return term == this->termManager.mkBoolean(true) ? this->manager.boolean(true) : this->manager.boolean(false);
        case cvc5::Kind::EQUAL: {
            // Storm's operator== only accepts numerical operands, while cvc5's EQUAL is also how it represents
            // boolean equivalence. Use iff for boolean operands so that translating back an unsatisfiable core
            // that contains an equivalence does not throw.
            storm::expressions::Expression const first = this->translateExpression(term[0]);
            storm::expressions::Expression const second = this->translateExpression(term[1]);
            if (first.hasBooleanType() && second.hasBooleanType()) {
                return storm::expressions::iff(first, second);
            }
            return first == second;
        }
        case cvc5::Kind::DISTINCT: {
            STORM_LOG_THROW(term.getNumChildren() != 0, storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert cvc5 expression. DISTINCT (mutual !=) operator with 0-arity is assumed to be an error.");
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
                            "Failed to convert cvc5 expression. 0-ary AND is assumed to be an error.");
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
                            "Failed to convert cvc5 expression. 0-ary OR is assumed to be an error.");
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
        case cvc5::Kind::ADD: {
            // Collect the operands and build a balanced expression to avoid deep recursion on large sums.
            std::vector<storm::expressions::Expression> operands;
            operands.reserve(term.getNumChildren());
            for (size_t i = 0; i < term.getNumChildren(); ++i) {
                operands.push_back(this->translateExpression(term[i]));
            }
            // A 0-ary sum is equal to 0.
            return operands.empty() ? this->manager.integer(0) : storm::expressions::sum(operands);
        }
        case cvc5::Kind::SUB:
            return this->translateExpression(term[0]) - this->translateExpression(term[1]);
        case cvc5::Kind::NEG:
            return -this->translateExpression(term[0]);
        case cvc5::Kind::MULT: {
            // Collect the operands and build a balanced expression to avoid deep recursion on large products.
            std::vector<storm::expressions::Expression> operands;
            operands.reserve(term.getNumChildren());
            for (size_t i = 0; i < term.getNumChildren(); ++i) {
                operands.push_back(this->translateExpression(term[i]));
            }
            // A 0-ary product is equal to 1.
            return operands.empty() ? this->manager.integer(1) : storm::expressions::product(operands);
        }
        case cvc5::Kind::INTS_DIVISION:
            // Storm has no integer division, and translating this to Storm's division would silently change the semantics.
            STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Failed to convert cvc5 expression. Integer division is not supported.");
        case cvc5::Kind::DIVISION:
            return this->translateExpression(term[0]) / this->translateExpression(term[1]);
        case cvc5::Kind::POW:
            return storm::expressions::pow(this->translateExpression(term[0]), this->translateExpression(term[1]));
        case cvc5::Kind::TO_REAL:
            return this->translateExpression(term[0]);
        case cvc5::Kind::TO_INTEGER:
            // cvc5's to_int operation applies the floor function.
            return storm::expressions::floor(this->translateExpression(term[0]));
        case cvc5::Kind::ABS:
            return storm::expressions::abs(this->translateExpression(term[0]));
        default:
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Failed to convert cvc5 expression. Encountered unhandled kind " << term.getKind() << ".");
            break;
    }
    STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException, "Failed to convert cvc5 expression.");
}

storm::expressions::Variable const& Cvc5ExpressionAdapter::getVariable(cvc5::Term const& constant) {
    auto const& constantVariablePair = expressionToVariableMapping.find(constant);
    STORM_LOG_THROW(constantVariablePair != expressionToVariableMapping.end(), storm::exceptions::ExpressionEvaluationException,
                    "Unable to find the variable corresponding to the given constant.");
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
            kind = cvc5::Kind::EQUAL;  // EQUAL on booleans is an iff in cvc5.
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

    // cvc5 requires the operands of an arithmetic operation to be of the same type. Where Storm admits a
    // mixed expression because the result is a real number, we convert the integer operand accordingly.
    bool const isRealExpected = expression.getType().isRationalType();
    leftResult = this->convertToRealIfExpected(leftResult, isRealExpected);
    rightResult = this->convertToRealIfExpected(rightResult, isRealExpected);

    // Storm assigns the type of the operands to the quotient, so the division of two integers is an integer in
    // Storm. cvc5, in contrast, always divides over the rational numbers. We therefore convert the operands of a
    // division to real numbers explicitly. Otherwise, the resulting real term would be combined with the
    // surrounding integer terms of a parent expression, which cvc5 rejects in comparisons and ITEs.
    if (expression.getOperatorType() == storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Divide) {
        leftResult = this->convertToRealIfExpected(leftResult, true);
        rightResult = this->convertToRealIfExpected(rightResult, true);
    }

    // Since the sort of a translated operand may deviate from the type that Storm assigned to it (consider the
    // division above), we always unify the sorts of the operands before constructing the term.
    this->unifyNumericalTypes(leftResult, rightResult);

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
            result = this->termManager.mkTerm(cvc5::Kind::DIVISION, {leftResult, rightResult});
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Min:
            result = this->termManager.mkTerm(cvc5::Kind::ITE, {this->termManager.mkTerm(cvc5::Kind::LEQ, {leftResult, rightResult}), leftResult, rightResult});
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Max:
            result = this->termManager.mkTerm(cvc5::Kind::ITE, {this->termManager.mkTerm(cvc5::Kind::GEQ, {leftResult, rightResult}), leftResult, rightResult});
            break;
        case storm::expressions::BinaryNumericalFunctionExpression::OperatorType::Power:
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

    // cvc5 requires the operands of a comparison to be of the same type, so a real number may be compared with an
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
            // cvc5's to_int operation converts a real number to an integer using the floor function.
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

    // cvc5 requires both branches of an ITE to be of the same type. Storm, in contrast, admits an expression that
    // selects between an integer and a rational number, so we convert the integer branch if necessary.
    this->unifyNumericalTypes(thenResult, elseResult);

    cvc5::Term result = this->termManager.mkTerm(cvc5::Kind::ITE, {conditionResult, thenResult, elseResult});

    expressionCache.emplace(&expression, result);
    return result;
}

boost::any Cvc5ExpressionAdapter::visit(storm::expressions::VariableExpression const& expression, boost::any const&) {
    return this->translateExpression(expression.getVariable());
}

cvc5::Term Cvc5ExpressionAdapter::createRational(storm::RationalNumber const& value) {
    // Both GMP and CLN print a rational number as "numerator/denominator" (e.g., "-123/122"), which is accepted by mkReal.
    std::stringstream fractionStream;
    fractionStream << value;
    return this->termManager.mkReal(fractionStream.str());
}

cvc5::Term Cvc5ExpressionAdapter::createVariable(storm::expressions::Variable const& variable) {
    cvc5::Sort sort;
    if (variable.getType().isBooleanType()) {
        sort = this->termManager.getBooleanSort();
    } else if (variable.getType().isRationalType()) {
        sort = this->termManager.getRealSort();
    } else {
        // A bitvector variable is an integer type in Storm, and its width carries no semantics: expressions have no bitvector
        // operators. It is therefore modelled as an unbounded integer, whose width is not enforced, as in the Z3 and MathSAT adapters.
        STORM_LOG_THROW(variable.getType().isIntegerType(), storm::exceptions::InvalidTypeException,
                        "Encountered variable '" << variable.getName() << "' with unknown type while trying to create solver variables.");
        sort = this->termManager.getIntegerSort();
    }
    cvc5::Term cvc5Variable = this->termManager.mkConst(sort, variable.getName());
    variableToExpressionMapping.insert(std::make_pair(variable, cvc5Variable));
    expressionToVariableMapping.insert(std::make_pair(cvc5Variable, variable));
    return cvc5Variable;
}

#endif
}  // namespace storm::adapters
