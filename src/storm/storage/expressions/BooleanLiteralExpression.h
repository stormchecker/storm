#pragma once

#include "storm/storage/expressions/BaseExpression.h"
#include "storm/utility/OsDetection.h"

namespace storm {
namespace expressions {
class BooleanLiteralExpression : public BaseExpression {
   public:
    /*!
     * Creates a boolean literal expression with the given value.
     *
     * @param manager The manager responsible for this expression.
     * @param value The value of the boolean literal.
     */
    BooleanLiteralExpression(ExpressionManager const& manager, bool value);

    // Instantiate constructors and assignments with their default implementations.
    BooleanLiteralExpression(BooleanLiteralExpression const& other) = default;
    BooleanLiteralExpression& operator=(BooleanLiteralExpression const& other) = delete;
    BooleanLiteralExpression(BooleanLiteralExpression&&) = default;
    BooleanLiteralExpression& operator=(BooleanLiteralExpression&&) = delete;

    virtual ~BooleanLiteralExpression() = default;

    // Override base class methods.
    virtual bool evaluateAsBool(Valuation const* valuation = nullptr) const override;
    virtual bool isLiteral() const override;
    virtual bool isTrue() const override;
    virtual bool isFalse() const override;
    virtual void gatherVariables(std::set<storm::expressions::Variable>& variables) const override;
    virtual std::shared_ptr<BaseExpression const> simplify() const override;
    virtual boost::any accept(ExpressionVisitor& visitor, boost::any const& data) const override;
    virtual bool isBooleanLiteralExpression() const override;
    /*!
     * Retrieves the value of the boolean literal.
     *
     * @return The value of the boolean literal.
     */
    bool getValue() const;

   protected:
    // Override base class method.
    virtual void printToStream(std::ostream& stream) const override;

   private:
    // The value of the boolean literal.
    bool value;
};
}  // namespace expressions
}  // namespace storm
