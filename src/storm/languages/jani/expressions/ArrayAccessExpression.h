#pragma once

#include "storm/expressions/BinaryExpression.h"

namespace storm {
namespace expressions {
/*!
 * Represents an access to an array.
 */
class ArrayAccessExpression : public BinaryExpression {
   public:
    ArrayAccessExpression(ExpressionManager const& manager, Type const& type, std::shared_ptr<BaseExpression const> const& arrayExpression,
                          std::shared_ptr<BaseExpression const> const& indexExpression);

    // Instantiate constructors and assignments with their default implementations.
    ArrayAccessExpression(ArrayAccessExpression const& other) = default;
    ArrayAccessExpression& operator=(ArrayAccessExpression const& other) = delete;
    ArrayAccessExpression(ArrayAccessExpression&&) = default;
    ArrayAccessExpression& operator=(ArrayAccessExpression&&) = delete;

    virtual ~ArrayAccessExpression() = default;

    virtual std::shared_ptr<BaseExpression const> simplify() const override;
    virtual boost::any accept(ExpressionVisitor& visitor, boost::any const& data) const override;

   protected:
    virtual void printToStream(std::ostream& stream) const override;
};
}  // namespace expressions
}  // namespace storm
