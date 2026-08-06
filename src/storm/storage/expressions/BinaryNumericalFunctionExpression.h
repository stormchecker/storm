#pragma once

#include "storm/storage/expressions/BinaryExpression.h"
#include "storm/utility/OsDetection.h"

namespace storm {
namespace expressions {
class BinaryNumericalFunctionExpression : public BinaryExpression {
   public:
    /*!
     * An enum type specifying the different operators applicable.
     */
    enum class OperatorType { Plus, Minus, Times, Divide, Min, Max, Power, Modulo, Logarithm };

    /*!
     * Constructs a binary numerical function expression with the given return type, operands and operator.
     *
     * @param manager The manager responsible for this expression.
     * @param type The return type of the expression.
     * @param firstOperand The first operand of the expression.
     * @param secondOperand The second operand of the expression.
     * @param functionType The operator of the expression.
     */
    BinaryNumericalFunctionExpression(ExpressionManager const& manager, Type const& type, std::shared_ptr<BaseExpression const> const& firstOperand,
                                      std::shared_ptr<BaseExpression const> const& secondOperand, OperatorType operatorType);

    // Instantiate constructors and assignments with their default implementations.
    BinaryNumericalFunctionExpression(BinaryNumericalFunctionExpression const& other) = default;
    BinaryNumericalFunctionExpression& operator=(BinaryNumericalFunctionExpression const& other) = delete;
    BinaryNumericalFunctionExpression(BinaryNumericalFunctionExpression&&) = default;
    BinaryNumericalFunctionExpression& operator=(BinaryNumericalFunctionExpression&&) = delete;

    virtual ~BinaryNumericalFunctionExpression() = default;

    // Override base class methods.
    virtual storm::expressions::OperatorType getOperator() const override;
    virtual int_fast64_t evaluateAsInt(Valuation const* valuation = nullptr) const override;
    virtual double evaluateAsDouble(Valuation const* valuation = nullptr) const override;
    virtual std::shared_ptr<BaseExpression const> simplify() const override;
    virtual boost::any accept(ExpressionVisitor& visitor, boost::any const& data) const override;
    virtual bool isBinaryNumericalFunctionExpression() const override;

    /*!
     * Retrieves the operator associated with the expression.
     *
     * @return The operator associated with the expression.
     */
    OperatorType getOperatorType() const;

   protected:
    // Override base class method.
    virtual void printToStream(std::ostream& stream) const override;

   private:
    // The operator of the expression.
    OperatorType operatorType;
};
}  // namespace expressions
}  // namespace storm
