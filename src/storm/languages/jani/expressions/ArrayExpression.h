#pragma once

#include "storm/expressions/BaseExpression.h"

namespace storm {
namespace expressions {
/*!
 * The base class of all array expressions.
 */
class ArrayExpression : public BaseExpression {
   public:
    ArrayExpression(ExpressionManager const& manager, Type const& type);

    // Instantiate constructors and assignments with their default implementations.
    ArrayExpression(ArrayExpression const& other) = default;
    ArrayExpression& operator=(ArrayExpression const& other) = delete;
    ArrayExpression(ArrayExpression&&) = default;
    ArrayExpression& operator=(ArrayExpression&&) = delete;

    virtual ~ArrayExpression() = default;

    // Returns the size of the array
    virtual std::shared_ptr<BaseExpression const> size() const = 0;

    // Returns the element at position i
    virtual std::shared_ptr<BaseExpression const> at(uint64_t i) const = 0;
};
}  // namespace expressions
}  // namespace storm
