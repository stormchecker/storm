#include "storm/storage/expressions/BinaryNumericalFunctionExpression.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/expressions/IntegerLiteralExpression.h"
#include "storm/storage/expressions/OperatorType.h"
#include "storm/storage/expressions/RationalLiteralExpression.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
BinaryNumericalFunctionExpression::BinaryNumericalFunctionExpression(ExpressionManager const& manager, Type const& type,
                                                                     std::shared_ptr<BaseExpression const> const& firstOperand,
                                                                     std::shared_ptr<BaseExpression const> const& secondOperand, OperatorType operatorType)
    : BinaryExpression(manager, type, firstOperand, secondOperand), operatorType(operatorType) {
    // Intentionally left empty.
}

BinaryNumericalFunctionExpression::OperatorType BinaryNumericalFunctionExpression::getOperatorType() const {
    return this->operatorType;
}

storm::expressions::OperatorType BinaryNumericalFunctionExpression::getOperator() const {
    storm::expressions::OperatorType result = storm::expressions::OperatorType::Plus;
    switch (this->getOperatorType()) {
        case OperatorType::Plus:
            result = storm::expressions::OperatorType::Plus;
            break;
        case OperatorType::Minus:
            result = storm::expressions::OperatorType::Minus;
            break;
        case OperatorType::Times:
            result = storm::expressions::OperatorType::Times;
            break;
        case OperatorType::Divide:
            result = storm::expressions::OperatorType::Divide;
            break;
        case OperatorType::Min:
            result = storm::expressions::OperatorType::Min;
            break;
        case OperatorType::Max:
            result = storm::expressions::OperatorType::Max;
            break;
        case OperatorType::Power:
            result = storm::expressions::OperatorType::Power;
            break;
        case OperatorType::Modulo:
            result = storm::expressions::OperatorType::Modulo;
            break;
        case OperatorType::Logarithm:
            result = storm::expressions::OperatorType::Logarithm;
            break;
    }
    return result;
}

template<typename V>
V logHelper(V const& x, V const& b) {
    // There is no std::log method for arbitrary bases.
    // This catches common cases which should yield more accurate results
    if (b == static_cast<V>(2)) {
        return std::log2(x);
    } else if (b == static_cast<V>(10)) {
        return std::log10(x);
    } else {
        return std::log2(x) / std::log2(b);
    }
}

int_fast64_t BinaryNumericalFunctionExpression::evaluateAsInt(Valuation const* valuation) const {
    STORM_LOG_THROW(this->hasIntegerType(), storm::exceptions::InvalidTypeException, "Unable to evaluate expression as integer.");

    int_fast64_t firstOperandEvaluation = this->getFirstOperand()->evaluateAsInt(valuation);
    int_fast64_t secondOperandEvaluation = this->getSecondOperand()->evaluateAsInt(valuation);
    int_fast64_t result = 0;
    switch (this->getOperatorType()) {
        case OperatorType::Plus:
            result = firstOperandEvaluation + secondOperandEvaluation;
            break;
        case OperatorType::Minus:
            result = firstOperandEvaluation - secondOperandEvaluation;
            break;
        case OperatorType::Times:
            result = firstOperandEvaluation * secondOperandEvaluation;
            break;
        case OperatorType::Divide:
            result = firstOperandEvaluation / secondOperandEvaluation;
            break;
        case OperatorType::Min:
            result = std::min(firstOperandEvaluation, secondOperandEvaluation);
            break;
        case OperatorType::Max:
            result = std::max(firstOperandEvaluation, secondOperandEvaluation);
            break;
        case OperatorType::Power:
            result = static_cast<int_fast64_t>(std::pow(firstOperandEvaluation, secondOperandEvaluation));
            break;
        case OperatorType::Modulo:
            result = firstOperandEvaluation % secondOperandEvaluation;
            break;
        case OperatorType::Logarithm:
            result = logHelper(firstOperandEvaluation, secondOperandEvaluation);
            break;
    }
    return result;
}

double BinaryNumericalFunctionExpression::evaluateAsDouble(Valuation const* valuation) const {
    STORM_LOG_THROW(this->hasNumericalType(), storm::exceptions::InvalidTypeException, "Unable to evaluate expression as double.");

    double firstOperandEvaluation = this->getFirstOperand()->evaluateAsDouble(valuation);
    double secondOperandEvaluation = this->getSecondOperand()->evaluateAsDouble(valuation);
    double result = 0;
    switch (this->getOperatorType()) {
        case OperatorType::Plus:
            result = firstOperandEvaluation + secondOperandEvaluation;
            break;
        case OperatorType::Minus:
            result = firstOperandEvaluation - secondOperandEvaluation;
            break;
        case OperatorType::Times:
            result = firstOperandEvaluation * secondOperandEvaluation;
            break;
        case OperatorType::Divide:
            result = firstOperandEvaluation / secondOperandEvaluation;
            break;
        case OperatorType::Min:
            result = std::min(firstOperandEvaluation, secondOperandEvaluation);
            break;
        case OperatorType::Max:
            result = std::max(firstOperandEvaluation, secondOperandEvaluation);
            break;
        case OperatorType::Power:
            result = std::pow(firstOperandEvaluation, secondOperandEvaluation);
            break;
        case OperatorType::Modulo:
            result = std::fmod(firstOperandEvaluation, secondOperandEvaluation);
            break;
        case OperatorType::Logarithm:
            result = logHelper(firstOperandEvaluation, secondOperandEvaluation);
            break;
    }
    return result;
}

std::shared_ptr<BaseExpression const> BinaryNumericalFunctionExpression::simplify() const {
    std::shared_ptr<BaseExpression const> firstOperandSimplified = this->getFirstOperand()->simplify();
    std::shared_ptr<BaseExpression const> secondOperandSimplified = this->getSecondOperand()->simplify();

    if (firstOperandSimplified->isLiteral() && secondOperandSimplified->isLiteral()) {
        if (this->hasIntegerType()) {
            int_fast64_t firstOperandEvaluation = firstOperandSimplified->evaluateAsInt();
            int_fast64_t secondOperandEvaluation = secondOperandSimplified->evaluateAsInt();
            std::optional<int_fast64_t> newValue;
            switch (this->getOperatorType()) {
                case OperatorType::Plus:
                    newValue = firstOperandEvaluation + secondOperandEvaluation;
                    break;
                case OperatorType::Minus:
                    newValue = firstOperandEvaluation - secondOperandEvaluation;
                    break;
                case OperatorType::Times:
                    newValue = firstOperandEvaluation * secondOperandEvaluation;
                    break;
                case OperatorType::Min:
                    newValue = std::min(firstOperandEvaluation, secondOperandEvaluation);
                    break;
                case OperatorType::Max:
                    newValue = std::max(firstOperandEvaluation, secondOperandEvaluation);
                    break;
                case OperatorType::Power:
                    if (secondOperandEvaluation >= 0) {
                        // Only simplify if this evaluates to an integer.
                        // Otherwise, we note that the type of this expression could change due to simplifications (which might or might not be expected)
                        newValue = static_cast<int_fast64_t>(std::pow(firstOperandEvaluation, secondOperandEvaluation));
                    }
                    break;
                case OperatorType::Modulo:
                    newValue = firstOperandEvaluation % secondOperandEvaluation;
                    break;
                case OperatorType::Logarithm:
                    // Do not simplify as it is not clear how a non-integer result should be treated.
                    break;
                case OperatorType::Divide:
                    if (firstOperandEvaluation % secondOperandEvaluation == 0) {
                        // Only simplify if there is no remainder, because otherwise it is not clear whether we want integer division or floating point
                        // division. Otherwise, we note that the type of this expression could change due to simplifications (which might or might not be
                        // expected)
                        newValue = firstOperandEvaluation / secondOperandEvaluation;
                    }
                    break;
            }
            if (newValue) {
                return std::shared_ptr<BaseExpression>(new IntegerLiteralExpression(this->getManager(), newValue.value()));
            }
        } else if (this->hasRationalType()) {
            storm::RationalNumber firstOperandEvaluation = firstOperandSimplified->evaluateAsRational();
            storm::RationalNumber secondOperandEvaluation = secondOperandSimplified->evaluateAsRational();
            std::optional<storm::RationalNumber> newValue;
            switch (this->getOperatorType()) {
                case OperatorType::Plus:
                    newValue = firstOperandEvaluation + secondOperandEvaluation;
                    break;
                case OperatorType::Minus:
                    newValue = firstOperandEvaluation - secondOperandEvaluation;
                    break;
                case OperatorType::Times:
                    newValue = firstOperandEvaluation * secondOperandEvaluation;
                    break;
                case OperatorType::Min:
                    newValue = std::min(firstOperandEvaluation, secondOperandEvaluation);
                    break;
                case OperatorType::Max:
                    newValue = std::max(firstOperandEvaluation, secondOperandEvaluation);
                    break;
                case OperatorType::Divide:
                    newValue = firstOperandEvaluation / secondOperandEvaluation;
                    break;
                case OperatorType::Power: {
                    if (carl::isInteger(secondOperandEvaluation)) {
                        auto exponent = carl::toInt<carl::sint>(secondOperandEvaluation);
                        if (exponent >= 0) {
                            newValue = carl::pow(firstOperandEvaluation, exponent);
                        } else {
                            storm::RationalNumber power = carl::pow(firstOperandEvaluation, -exponent);
                            newValue = storm::utility::one<storm::RationalNumber>() / power;
                        }
                    }
                    break;
                }
                case OperatorType::Modulo: {
                    if (carl::isInteger(firstOperandEvaluation) && carl::isInteger(secondOperandEvaluation)) {
                        newValue = storm::utility::mod(storm::utility::numerator(firstOperandEvaluation), storm::utility::numerator(secondOperandEvaluation));
                    }
                    break;
                }
                case OperatorType::Logarithm:
                    // Do not simplify as it is not clear how a non-rational result should be treated.
                    break;
            }
            if (newValue) {
                return std::shared_ptr<BaseExpression>(new RationalLiteralExpression(this->getManager(), newValue.value()));
            }
        }
    }

    if (firstOperandSimplified.get() == this->getFirstOperand().get() && secondOperandSimplified.get() == this->getSecondOperand().get()) {
        return this->shared_from_this();
    } else {
        return std::shared_ptr<BaseExpression>(new BinaryNumericalFunctionExpression(this->getManager(), this->getType(), firstOperandSimplified,
                                                                                     secondOperandSimplified, this->getOperatorType()));
    }
}

boost::any BinaryNumericalFunctionExpression::accept(ExpressionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

bool BinaryNumericalFunctionExpression::isBinaryNumericalFunctionExpression() const {
    return true;
}

void BinaryNumericalFunctionExpression::printToStream(std::ostream& stream) const {
    stream << "(";
    switch (this->getOperatorType()) {
        case OperatorType::Plus:
            stream << *this->getFirstOperand() << " + " << *this->getSecondOperand();
            break;
        case OperatorType::Minus:
            stream << *this->getFirstOperand() << " - " << *this->getSecondOperand();
            break;
        case OperatorType::Times:
            stream << *this->getFirstOperand() << " * " << *this->getSecondOperand();
            break;
        case OperatorType::Divide:
            stream << *this->getFirstOperand() << " / " << *this->getSecondOperand();
            break;
        case OperatorType::Min:
            stream << "min(" << *this->getFirstOperand() << ", " << *this->getSecondOperand() << ")";
            break;
        case OperatorType::Max:
            stream << "max(" << *this->getFirstOperand() << ", " << *this->getSecondOperand() << ")";
            break;
        case OperatorType::Power:
            stream << *this->getFirstOperand() << " ^ " << *this->getSecondOperand();
            break;
        case OperatorType::Modulo:
            stream << *this->getFirstOperand() << " % " << *this->getSecondOperand();
            break;
        case OperatorType::Logarithm:
            stream << "log(" << *this->getFirstOperand() << ", " << *this->getSecondOperand() << ")";
            break;
    }
    stream << ")";
}
}  // namespace expressions
}  // namespace storm
