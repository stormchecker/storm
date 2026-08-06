#pragma once

#include "storm/storage/expressions/BaseExpression.h"
#include "storm/utility/OsDetection.h"

namespace storm {
namespace expressions {
/*!
 * The base class of all binary expressions.
 */
class BinaryExpression : public BaseExpression {
   public:
    /*!
     * Constructs a binary expression with the given return type and operands.
     *
     * @param manager The manager responsible for this expression.
     * @param type The return type of the expression.
     * @param firstOperand The first operand of the expression.
     * @param secondOperand The second operand of the expression.
     */
    BinaryExpression(ExpressionManager const& manager, Type const& type, std::shared_ptr<BaseExpression const> const& firstOperand,
                     std::shared_ptr<BaseExpression const> const& secondOperand);

    // Instantiate constructors and assignments with their default implementations.
    BinaryExpression(BinaryExpression const& other) = default;
    BinaryExpression& operator=(BinaryExpression const& other) = delete;
    BinaryExpression(BinaryExpression&&) = default;
    BinaryExpression& operator=(BinaryExpression&&) = delete;

    virtual ~BinaryExpression() = default;

    // Override base class methods.
    virtual bool isFunctionApplication() const override;
    virtual bool containsVariables() const override;
    virtual uint_fast64_t getArity() const override;
    virtual std::shared_ptr<BaseExpression const> getOperand(uint_fast64_t operandIndex) const override;
    virtual void gatherVariables(std::set<storm::expressions::Variable>& variables) const override;

    /*!
     * Retrieves the first operand of the expression.
     *
     * @return The first operand of the expression.
     */
    std::shared_ptr<BaseExpression const> const& getFirstOperand() const;

    /*!
     * Retrieves the second operand of the expression.
     *
     * @return The second operand of the expression.
     */
    std::shared_ptr<BaseExpression const> const& getSecondOperand() const;

   private:
    // The first operand of the expression.
    std::shared_ptr<BaseExpression const> firstOperand;

    // The second operand of the expression.
    std::shared_ptr<BaseExpression const> secondOperand;
};
}  // namespace expressions
}  // namespace storm
