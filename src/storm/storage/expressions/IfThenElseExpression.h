#pragma once

#include "storm/storage/expressions/BaseExpression.h"
#include "storm/utility/OsDetection.h"

namespace storm {
namespace expressions {
class IfThenElseExpression : public BaseExpression {
   public:
    /*!
     * Creates an if-then-else expression with the given return type, condition and operands.
     *
     * @param manager The manager responsible for this expression.
     * @param type The return type of the expression.
     * @param thenExpression The expression evaluated if the condition evaluates true.
     * @param elseExpression The expression evaluated if the condition evaluates false.
     */
    IfThenElseExpression(ExpressionManager const& manager, Type const& type, std::shared_ptr<BaseExpression const> const& condition,
                         std::shared_ptr<BaseExpression const> const& thenExpression, std::shared_ptr<BaseExpression const> const& elseExpression);

    // Instantiate constructors and assignments with their default implementations.
    IfThenElseExpression(IfThenElseExpression const& other) = default;
    IfThenElseExpression& operator=(IfThenElseExpression const& other) = delete;
    IfThenElseExpression(IfThenElseExpression&&) = default;
    IfThenElseExpression& operator=(IfThenElseExpression&&) = delete;

    virtual ~IfThenElseExpression() = default;

    // Override base class methods.
    virtual std::shared_ptr<BaseExpression const> getOperand(uint_fast64_t operandIndex) const override;
    virtual OperatorType getOperator() const override;
    virtual bool isFunctionApplication() const override;
    virtual bool containsVariables() const override;
    virtual uint_fast64_t getArity() const override;
    virtual bool evaluateAsBool(Valuation const* valuation = nullptr) const override;
    virtual int_fast64_t evaluateAsInt(Valuation const* valuation = nullptr) const override;
    virtual double evaluateAsDouble(Valuation const* valuation = nullptr) const override;
    virtual void gatherVariables(std::set<storm::expressions::Variable>& variables) const override;
    virtual std::shared_ptr<BaseExpression const> simplify() const override;
    virtual boost::any accept(ExpressionVisitor& visitor, boost::any const& data) const override;
    virtual bool isIfThenElseExpression() const override;

    /*!
     * Retrieves the condition expression of the if-then-else expression.
     *
     * @return The condition expression of the if-then-else expression.
     */
    std::shared_ptr<BaseExpression const> getCondition() const;

    /*!
     * Retrieves the then expression of the if-then-else expression.
     *
     * @return The then expression of the if-then-else expression.
     */
    std::shared_ptr<BaseExpression const> getThenExpression() const;

    /*!
     * Retrieves the else expression of the if-then-else expression.
     *
     * @return The else expression of the if-then-else expression.
     */
    std::shared_ptr<BaseExpression const> getElseExpression() const;

   protected:
    // Override base class method.
    virtual void printToStream(std::ostream& stream) const override;

   private:
    // The condition of the if-then-else.
    std::shared_ptr<BaseExpression const> condition;

    // The return expression of the if-part.
    std::shared_ptr<BaseExpression const> thenExpression;

    // The return expression of the else-part.
    std::shared_ptr<BaseExpression const> elseExpression;
};
}  // namespace expressions
}  // namespace storm
