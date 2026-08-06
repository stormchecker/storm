#pragma once

#include "storm/storage/expressions/BaseExpression.h"
#include "storm/utility/OsDetection.h"

namespace storm {
namespace expressions {
class UnaryExpression : public BaseExpression {
   public:
    /*!
     * Creates a unary expression with the given return type and operand.
     *
     * @param manager The manager responsible for this expression.
     * @param type The return type of the expression.
     * @param operand The operand of the unary expression.
     */
    UnaryExpression(ExpressionManager const& manager, Type const& type, std::shared_ptr<BaseExpression const> const& operand);

    // Instantiate constructors and assignments with their default implementations.
    UnaryExpression(UnaryExpression const& other) = default;
    UnaryExpression& operator=(UnaryExpression const& other) = delete;
    UnaryExpression(UnaryExpression&&) = default;
    UnaryExpression& operator=(UnaryExpression&&) = delete;
    virtual ~UnaryExpression() = default;

    // Override base class methods.
    virtual bool isFunctionApplication() const override;
    virtual bool containsVariables() const override;
    virtual uint_fast64_t getArity() const override;
    virtual std::shared_ptr<BaseExpression const> getOperand(uint_fast64_t operandIndex) const override;
    virtual void gatherVariables(std::set<storm::expressions::Variable>& variables) const override;

    /*!
     * Retrieves the operand of the unary expression.
     *
     * @return The operand of the unary expression.
     */
    std::shared_ptr<BaseExpression const> const& getOperand() const;

   private:
    // The operand of the unary expression.
    std::shared_ptr<BaseExpression const> operand;
};
}  // namespace expressions
}  // namespace storm
