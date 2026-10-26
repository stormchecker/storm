#pragma once

#include "storm/languages/jani/expressions/ArrayExpression.h"

#include <memory>
#include <string>
#include <vector>
#include "storm/expressions/BaseExpression.h"

namespace storm {
namespace expressions {
/*!
 * Represents an array with a given list of elements.
 */
class ValueArrayExpression : public ArrayExpression {
   public:
    ValueArrayExpression(ExpressionManager const& manager, Type const& type, std::vector<std::shared_ptr<BaseExpression const>> const& elements);

    // Instantiate constructors and assignments with their default implementations.
    ValueArrayExpression(ValueArrayExpression const& other) = default;
    ValueArrayExpression& operator=(ValueArrayExpression const& other) = delete;
    ValueArrayExpression(ValueArrayExpression&&) = default;
    ValueArrayExpression& operator=(ValueArrayExpression&&) = delete;

    virtual ~ValueArrayExpression() = default;

    virtual void gatherVariables(std::set<storm::expressions::Variable>& variables) const override;
    virtual bool containsVariables() const override;
    virtual std::shared_ptr<BaseExpression const> simplify() const override;
    virtual boost::any accept(ExpressionVisitor& visitor, boost::any const& data) const override;

    // Returns the size of the array
    virtual std::shared_ptr<BaseExpression const> size() const override;

    // Returns the element at position i
    virtual std::shared_ptr<BaseExpression const> at(uint64_t i) const override;

   protected:
    virtual void printToStream(std::ostream& stream) const override;

   private:
    std::vector<std::shared_ptr<BaseExpression const>> elements;
};
}  // namespace expressions
}  // namespace storm