#include "storm/storage/expressions/UnaryNumericalFunctionExpression.h"

#include <cmath>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/expressions/IntegerLiteralExpression.h"
#include "storm/storage/expressions/OperatorType.h"
#include "storm/storage/expressions/RationalLiteralExpression.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
UnaryNumericalFunctionExpression::UnaryNumericalFunctionExpression(ExpressionManager const& manager, Type const& type,
                                                                   std::shared_ptr<BaseExpression const> const& operand, OperatorType operatorType)
    : UnaryExpression(manager, type, operand), operatorType(operatorType) {
    // Intentionally left empty.
}

UnaryNumericalFunctionExpression::OperatorType UnaryNumericalFunctionExpression::getOperatorType() const {
    return this->operatorType;
}

storm::expressions::OperatorType UnaryNumericalFunctionExpression::getOperator() const {
    storm::expressions::OperatorType result = storm::expressions::OperatorType::Minus;
    switch (this->getOperatorType()) {
        case OperatorType::Minus:
            result = storm::expressions::OperatorType::Minus;
            break;
        case OperatorType::Floor:
            result = storm::expressions::OperatorType::Floor;
            break;
        case OperatorType::Ceil:
            result = storm::expressions::OperatorType::Ceil;
            break;
        case OperatorType::Cos:
            result = storm::expressions::OperatorType::Cos;
            break;
        case OperatorType::Sin:
            result = storm::expressions::OperatorType::Sin;
            break;
    }
    return result;
}

int_fast64_t UnaryNumericalFunctionExpression::evaluateAsInt(Valuation const* valuation) const {
    STORM_LOG_THROW(this->hasIntegerType(), storm::exceptions::InvalidTypeException, "Unable to evaluate expression as integer.");

    if (this->getOperatorType() == OperatorType::Minus) {
        STORM_LOG_THROW(this->getOperand()->hasIntegerType(), storm::exceptions::InvalidTypeException, "Unable to evaluate expression as integer.");
        int_fast64_t result = this->getOperand()->evaluateAsInt(valuation);
        return -result;
    } else {
        // TODO: this should evaluate the operand as a rational.
        double result = this->getOperand()->evaluateAsDouble(valuation);
        switch (this->getOperatorType()) {
            case OperatorType::Floor:
                return static_cast<int_fast64_t>(std::floor(result));
            case OperatorType::Ceil:
                return static_cast<int_fast64_t>(std::ceil(result));
            default:
                STORM_LOG_ASSERT(false, "All other operator types should have been handled before.");
                return 0;  // Warning suppression.
        }
    }
}

double UnaryNumericalFunctionExpression::evaluateAsDouble(Valuation const* valuation) const {
    STORM_LOG_THROW(this->hasNumericalType(), storm::exceptions::InvalidTypeException, "Unable to evaluate expression as double.");

    double result = this->getOperand()->evaluateAsDouble(valuation);
    switch (this->getOperatorType()) {
        case OperatorType::Minus:
            result = -result;
            break;
        case OperatorType::Floor:
            result = std::floor(result);
            break;
        case OperatorType::Ceil:
            result = std::ceil(result);
            break;
        case OperatorType::Cos:
            result = std::cos(result);
            break;
        case OperatorType::Sin:
            result = std::sin(result);
            break;
    }
    return result;
}

std::shared_ptr<BaseExpression const> UnaryNumericalFunctionExpression::simplify() const {
    std::shared_ptr<BaseExpression const> operandSimplified = this->getOperand()->simplify();

    if (operandSimplified->isLiteral()) {
        if (operandSimplified->hasIntegerType()) {
            int_fast64_t intValue = operandSimplified->evaluateAsInt();
            storm::RationalNumber rationalValue;
            bool useInteger = true;
            switch (this->getOperatorType()) {
                case OperatorType::Minus:
                    intValue = -intValue;
                    break;
                // Nothing to be done for the other cases:
                case OperatorType::Floor:
                case OperatorType::Ceil:
                    break;
                case OperatorType::Cos:
                    useInteger = false;
                    rationalValue = storm::utility::cos(storm::utility::convertNumber<storm::RationalNumber>(intValue));
                    break;
                case OperatorType::Sin:
                    useInteger = false;
                    rationalValue = storm::utility::sin(storm::utility::convertNumber<storm::RationalNumber>(intValue));
                    break;
            }
            if (useInteger) {
                return std::shared_ptr<BaseExpression>(new IntegerLiteralExpression(this->getManager(), intValue));
            } else {
                return std::shared_ptr<BaseExpression>(new RationalLiteralExpression(this->getManager(), rationalValue));
            }
        } else if (operandSimplified->hasRationalType()) {
            storm::RationalNumber value = operandSimplified->evaluateAsRational();
            bool convertToInteger = false;
            switch (this->getOperatorType()) {
                case OperatorType::Minus:
                    value = -value;
                    break;
                case OperatorType::Floor:
                    value = storm::utility::floor(value);
                    convertToInteger = true;
                    break;
                case OperatorType::Ceil:
                    value = storm::utility::ceil(value);
                    convertToInteger = true;
                    break;
                case OperatorType::Cos:
                    value = storm::utility::cos(value);
                    break;
                case OperatorType::Sin:
                    value = storm::utility::sin(value);
                    break;
            }
            if (convertToInteger) {
                return std::shared_ptr<BaseExpression>(new IntegerLiteralExpression(this->getManager(), storm::utility::convertNumber<int64_t>(value)));
            } else {
                return std::shared_ptr<BaseExpression>(new RationalLiteralExpression(this->getManager(), value));
            }
        }
    }

    if (operandSimplified.get() == this->getOperand().get()) {
        return this->shared_from_this();
    } else {
        return std::shared_ptr<BaseExpression>(
            new UnaryNumericalFunctionExpression(this->getManager(), this->getType(), operandSimplified, this->getOperatorType()));
    }
}

boost::any UnaryNumericalFunctionExpression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

bool UnaryNumericalFunctionExpression::isUnaryNumericalFunctionExpression() const {
    return true;
}

void UnaryNumericalFunctionExpression::printToStream(std::ostream& stream) const {
    switch (this->getOperatorType()) {
        case OperatorType::Minus:
            stream << "-(";
            break;
        case OperatorType::Floor:
            stream << "floor(";
            break;
        case OperatorType::Ceil:
            stream << "ceil(";
            break;
        case OperatorType::Cos:
            stream << "cos(";
            break;
        case OperatorType::Sin:
            stream << "sin(";
            break;
    }
    stream << *this->getOperand() << ")";
}
}  // namespace expressions
}  // namespace storm
