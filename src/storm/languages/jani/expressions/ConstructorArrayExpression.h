#pragma once

#include "storm/expressions/Variable.h"
#include "storm/languages/jani/expressions/ArrayExpression.h"

namespace storm {
namespace expressions {
/*!
 * Represents an array of the given size, where the i'th entry is determined by the elementExpression, where occurrences of indexVar will be substituted by i
 */
class ConstructorArrayExpression : public ArrayExpression {
   public:
    ConstructorArrayExpression(ExpressionManager const& manager, Type const& type, std::shared_ptr<BaseExpression const> const& size,
                               storm::expressions::Variable indexVar, std::shared_ptr<BaseExpression const> const& elementExpression);

    // Instantiate constructors and assignments with their default implementations.
    ConstructorArrayExpression(ConstructorArrayExpression const& other) = default;
    ConstructorArrayExpression& operator=(ConstructorArrayExpression const& other) = delete;
    ConstructorArrayExpression(ConstructorArrayExpression&&) = default;
    ConstructorArrayExpression& operator=(ConstructorArrayExpression&&) = delete;

    virtual ~ConstructorArrayExpression() = default;

    virtual void gatherVariables(std::set<storm::expressions::Variable>& variables) const override;
    virtual bool containsVariables() const override;
    virtual std::shared_ptr<BaseExpression const> simplify() const override;
    virtual boost::any accept(ExpressionVisitor& visitor, boost::any const& data) const override;

    // Returns the size of the array
    virtual std::shared_ptr<BaseExpression const> size() const override;

    // Returns the element at position i
    virtual std::shared_ptr<BaseExpression const> at(uint64_t i) const override;

    std::shared_ptr<BaseExpression const> const& getElementExpression() const;
    storm::expressions::Variable const& getIndexVar() const;

   protected:
    virtual void printToStream(std::ostream& stream) const override;

   private:
    std::shared_ptr<BaseExpression const> sizeExpression;
    storm::expressions::Variable indexVar;
    std::shared_ptr<BaseExpression const> elementExpression;
};
}  // namespace expressions
}  // namespace storm