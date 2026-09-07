#include "BinaryBooleanFunctionExpression.h"

#include "storm/exceptions/InvalidTypeException.h"
#include "storm/storage/expressions/BooleanLiteralExpression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/storage/expressions/OperatorType.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
BinaryBooleanFunctionExpression::BinaryBooleanFunctionExpression(ExpressionManager const& manager, Type const& type,
                                                                 std::shared_ptr<BaseExpression const> const& firstOperand,
                                                                 std::shared_ptr<BaseExpression const> const& secondOperand, OperatorType operatorType)
    : BinaryExpression(manager, type, firstOperand, secondOperand), operatorType(operatorType) {
    // Intentionally left empty.
}

BinaryBooleanFunctionExpression::OperatorType BinaryBooleanFunctionExpression::getOperatorType() const {
    return this->operatorType;
}

storm::expressions::OperatorType BinaryBooleanFunctionExpression::getOperator() const {
    storm::expressions::OperatorType result = storm::expressions::OperatorType::And;
    switch (this->getOperatorType()) {
        case OperatorType::And:
            result = storm::expressions::OperatorType::And;
            break;
        case OperatorType::Or:
            result = storm::expressions::OperatorType::Or;
            break;
        case OperatorType::Xor:
            result = storm::expressions::OperatorType::Xor;
            break;
        case OperatorType::Implies:
            result = storm::expressions::OperatorType::Implies;
            break;
        case OperatorType::Iff:
            result = storm::expressions::OperatorType::Iff;
            break;
    }
    return result;
}

bool BinaryBooleanFunctionExpression::evaluateAsBool(Valuation const* valuation) const {
    STORM_LOG_THROW(this->hasBooleanType(), storm::exceptions::InvalidTypeException, "Unable to evaluate expression as boolean.");

    bool firstOperandEvaluation = this->getFirstOperand()->evaluateAsBool(valuation);
    bool secondOperandEvaluation = this->getSecondOperand()->evaluateAsBool(valuation);

    bool result;
    switch (this->getOperatorType()) {
        case OperatorType::And:
            result = firstOperandEvaluation && secondOperandEvaluation;
            break;
        case OperatorType::Or:
            result = firstOperandEvaluation || secondOperandEvaluation;
            break;
        case OperatorType::Xor:
            result = firstOperandEvaluation ^ secondOperandEvaluation;
            break;
        case OperatorType::Implies:
            result = !firstOperandEvaluation || secondOperandEvaluation;
            break;
        case OperatorType::Iff:
            result = (firstOperandEvaluation && secondOperandEvaluation) || (!firstOperandEvaluation && !secondOperandEvaluation);
            break;
    }

    return result;
}

std::shared_ptr<BaseExpression const> BinaryBooleanFunctionExpression::simplify() const {
    std::shared_ptr<BaseExpression const> firstOperandSimplified = this->getFirstOperand()->simplify();
    std::shared_ptr<BaseExpression const> secondOperandSimplified = this->getSecondOperand()->simplify();

    if (firstOperandSimplified->isLiteral() || secondOperandSimplified->isLiteral()) {
        switch (this->getOperatorType()) {
            case OperatorType::And:
                if (firstOperandSimplified->isTrue()) {
                    return secondOperandSimplified;
                } else if (firstOperandSimplified->isFalse()) {
                    return firstOperandSimplified;
                } else if (secondOperandSimplified->isTrue()) {
                    return firstOperandSimplified;
                } else if (secondOperandSimplified->isFalse()) {
                    return secondOperandSimplified;
                }
                break;
            case OperatorType::Or:
                if (firstOperandSimplified->isTrue()) {
                    return firstOperandSimplified;
                } else if (firstOperandSimplified->isFalse()) {
                    return secondOperandSimplified;
                } else if (secondOperandSimplified->isTrue()) {
                    return secondOperandSimplified;
                } else if (secondOperandSimplified->isFalse()) {
                    return firstOperandSimplified;
                }
                break;
            case OperatorType::Xor:
                if (firstOperandSimplified->isTrue()) {
                    if (secondOperandSimplified->isFalse()) {
                        return firstOperandSimplified;
                    } else if (secondOperandSimplified->isTrue()) {
                        return this->getManager().boolean(false).getBaseExpressionPointer();
                    }
                } else if (firstOperandSimplified->isFalse()) {
                    if (secondOperandSimplified->isTrue()) {
                        return secondOperandSimplified;
                    } else if (secondOperandSimplified->isFalse()) {
                        return this->getManager().boolean(false).getBaseExpressionPointer();
                    }
                }
                break;
            case OperatorType::Implies:
                if (firstOperandSimplified->isTrue()) {
                    return secondOperandSimplified;
                } else if (firstOperandSimplified->isFalse()) {
                    return std::shared_ptr<BaseExpression>(new BooleanLiteralExpression(this->getManager(), true));
                } else if (secondOperandSimplified->isTrue()) {
                    return std::shared_ptr<BaseExpression>(new BooleanLiteralExpression(this->getManager(), true));
                }
                break;
            case OperatorType::Iff:
                if (firstOperandSimplified->isTrue() && secondOperandSimplified->isTrue()) {
                    return std::shared_ptr<BaseExpression>(new BooleanLiteralExpression(this->getManager(), true));
                } else if (firstOperandSimplified->isFalse() && secondOperandSimplified->isFalse()) {
                    return std::shared_ptr<BaseExpression>(new BooleanLiteralExpression(this->getManager(), true));
                } else if (firstOperandSimplified->isTrue() && secondOperandSimplified->isFalse()) {
                    return std::shared_ptr<BaseExpression>(new BooleanLiteralExpression(this->getManager(), false));
                } else if (firstOperandSimplified->isFalse() && secondOperandSimplified->isTrue()) {
                    return std::shared_ptr<BaseExpression>(new BooleanLiteralExpression(this->getManager(), false));
                }
                if (firstOperandSimplified->isTrue()) {
                    return secondOperandSimplified;
                }
                if (secondOperandSimplified->isTrue()) {
                    return firstOperandSimplified;
                }
                if (firstOperandSimplified->isFalse()) {
                    return std::shared_ptr<BaseExpression>(new UnaryBooleanFunctionExpression(this->getManager(), this->getType(), secondOperandSimplified,
                                                                                              UnaryBooleanFunctionExpression::OperatorType::Not));
                }
                if (secondOperandSimplified->isFalse()) {
                    return std::shared_ptr<BaseExpression>(new UnaryBooleanFunctionExpression(this->getManager(), this->getType(), firstOperandSimplified,
                                                                                              UnaryBooleanFunctionExpression::OperatorType::Not));
                }
                break;
        }
    }

    // If the two successors remain unchanged, we can return a shared_ptr to this very object.
    if (firstOperandSimplified.get() == this->getFirstOperand().get() && secondOperandSimplified.get() == this->getSecondOperand().get()) {
        return this->shared_from_this();
    } else {
        return std::shared_ptr<BaseExpression>(
            new BinaryBooleanFunctionExpression(this->getManager(), this->getType(), firstOperandSimplified, secondOperandSimplified, this->getOperatorType()));
    }
}

boost::any BinaryBooleanFunctionExpression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

bool BinaryBooleanFunctionExpression::isBinaryBooleanFunctionExpression() const {
    return true;
}

void BinaryBooleanFunctionExpression::printToStream(std::ostream& stream) const {
    stream << "(" << *this->getFirstOperand();
    switch (this->getOperatorType()) {
        case OperatorType::And:
            stream << " & ";
            break;
        case OperatorType::Or:
            stream << " | ";
            break;
        case OperatorType::Xor:
            stream << " != ";
            break;
        case OperatorType::Implies:
            stream << " => ";
            break;
        case OperatorType::Iff:
            stream << " = ";
            break;
    }
    stream << *this->getSecondOperand() << ")";
}
}  // namespace expressions
}  // namespace storm
