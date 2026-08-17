#pragma once

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/expressions/BaseExpression.h"

namespace storm {
namespace expressions {
class RationalLiteralExpression : public BaseExpression {
   public:
    /*!
     * Creates an double literal expression with the given value.
     *
     * @param manager The manager responsible for this expression.
     * @param value The value of the double literal.
     */
    RationalLiteralExpression(ExpressionManager const& manager, double value);

    /*!
     * Creates an double literal expression with the value given as a string.
     *
     * @param manager The manager responsible for this expression.
     * @param value The string representation of the value of the literal.
     */
    RationalLiteralExpression(ExpressionManager const& manager, std::string const& valueAsString);

    /*!
     * Creates an double literal expression with the rational value.
     *
     * @param manager The manager responsible for this expression.
     * @param value The rational number that is the value of this literal expression.
     */
    RationalLiteralExpression(ExpressionManager const& manager, storm::RationalNumber const& value);

    // Instantiate constructors and assignments with their default implementations.
    RationalLiteralExpression(RationalLiteralExpression const& other) = default;
    RationalLiteralExpression& operator=(RationalLiteralExpression const& other) = delete;
    RationalLiteralExpression(RationalLiteralExpression&&) = default;
    RationalLiteralExpression& operator=(RationalLiteralExpression&&) = delete;

    virtual ~RationalLiteralExpression() = default;

    // Override base class methods.
    virtual double evaluateAsDouble(Valuation const* valuation = nullptr) const override;
    virtual bool isLiteral() const override;
    virtual void gatherVariables(std::set<storm::expressions::Variable>& variables) const override;
    virtual std::shared_ptr<BaseExpression const> simplify() const override;
    virtual boost::any accept(ExpressionVisitor& visitor, boost::any const& data) const override;
    virtual bool isRationalLiteralExpression() const override;

    /*!
     * Retrieves the value of the double literal.
     *
     * @return The value of the double literal.
     */
    double getValueAsDouble() const;

    /*!
     * Retrieves the value of the double literal.
     *
     * @return The value of the double literal.
     */
    storm::RationalNumber getValue() const;

   protected:
    // Override base class method.
    virtual void printToStream(std::ostream& stream) const override;

   private:
    // The value of the literal.
    storm::RationalNumber value;
};
}  // namespace expressions
}  // namespace storm
