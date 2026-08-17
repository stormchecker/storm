#pragma once

#include "storm/storage/expressions/UnaryExpression.h"
#include "storm/utility/OsDetection.h"

namespace storm {
namespace expressions {
class UnaryBooleanFunctionExpression : public UnaryExpression {
   public:
    /*!
     * An enum type specifying the different functions applicable.
     */
    enum class OperatorType { Not };

    /*!
     * Creates a unary boolean function expression with the given return type, operand and operator.
     *
     * @param manager The manager responsible for this expression.
     * @param type The return type of the expression.
     * @param operand The operand of the expression.
     * @param operatorType The operator of the expression.
     */
    UnaryBooleanFunctionExpression(ExpressionManager const& manager, Type const& type, std::shared_ptr<BaseExpression const> const& operand,
                                   OperatorType operatorType);

    // Instantiate constructors and assignments with their default implementations.
    UnaryBooleanFunctionExpression(UnaryBooleanFunctionExpression const& other) = default;
    UnaryBooleanFunctionExpression& operator=(UnaryBooleanFunctionExpression const& other) = delete;
    UnaryBooleanFunctionExpression(UnaryBooleanFunctionExpression&&) = default;
    UnaryBooleanFunctionExpression& operator=(UnaryBooleanFunctionExpression&&) = delete;
    virtual ~UnaryBooleanFunctionExpression() = default;

    // Override base class methods.
    virtual storm::expressions::OperatorType getOperator() const override;
    virtual bool evaluateAsBool(Valuation const* valuation = nullptr) const override;
    virtual std::shared_ptr<BaseExpression const> simplify() const override;
    virtual boost::any accept(ExpressionVisitor& visitor, boost::any const& data) const override;
    virtual bool isUnaryBooleanFunctionExpression() const override;

    /*!
     * Retrieves the operator associated with this expression.
     *
     * @return The operator associated with this expression.
     */
    OperatorType getOperatorType() const;

   protected:
    // Override base class method.
    virtual void printToStream(std::ostream& stream) const override;

   private:
    // The operator of this expression.
    OperatorType operatorType;
};
}  // namespace expressions
}  // namespace storm
