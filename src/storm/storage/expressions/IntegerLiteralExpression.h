#pragma once

#include "storm/storage/expressions/BaseExpression.h"
#include "storm/utility/OsDetection.h"

namespace storm {
namespace expressions {
class IntegerLiteralExpression : public BaseExpression {
   public:
    /*!
     * Creates an integer literal expression with the given value.
     *
     * @param manager The manager responsible for this expression.
     * @param value The value of the integer literal.
     */
    IntegerLiteralExpression(ExpressionManager const& manager, int_fast64_t value);

    // Instantiate constructors and assignments with their default implementations.
    IntegerLiteralExpression(IntegerLiteralExpression const& other) = default;
    IntegerLiteralExpression& operator=(IntegerLiteralExpression const& other) = delete;
    IntegerLiteralExpression(IntegerLiteralExpression&&) = default;
    IntegerLiteralExpression& operator=(IntegerLiteralExpression&&) = delete;

    virtual ~IntegerLiteralExpression() = default;

    // Override base class methods.
    virtual int_fast64_t evaluateAsInt(Valuation const* valuation = nullptr) const override;
    virtual double evaluateAsDouble(Valuation const* valuation = nullptr) const override;
    virtual bool isLiteral() const override;
    virtual void gatherVariables(std::set<storm::expressions::Variable>& variables) const override;
    virtual std::shared_ptr<BaseExpression const> simplify() const override;
    virtual boost::any accept(ExpressionVisitor& visitor, boost::any const& data) const override;
    virtual bool isIntegerLiteralExpression() const override;

    /*!
     * Retrieves the value of the integer literal.
     *
     * @return The value of the integer literal.
     */
    int_fast64_t getValue() const;

   protected:
    // Override base class method.
    virtual void printToStream(std::ostream& stream) const override;

   private:
    // The value of the integer literal.
    int_fast64_t value;
};
}  // namespace expressions
}  // namespace storm
