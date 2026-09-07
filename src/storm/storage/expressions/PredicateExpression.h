#pragma once

#include <vector>

#include "storm/storage/expressions/BaseExpression.h"

namespace storm {
namespace expressions {
/*!
 * The base class of all binary expressions.
 */
class PredicateExpression : public BaseExpression {
   public:
    enum class PredicateType { AtLeastOneOf, AtMostOneOf, ExactlyOneOf };

    PredicateExpression(ExpressionManager const &manager, Type const &type, std::vector<std::shared_ptr<BaseExpression const>> const &operands,
                        PredicateType predicateType);

    // Instantiate constructors and assignments with their default implementations.
    PredicateExpression(PredicateExpression const &other) = default;

    PredicateExpression &operator=(PredicateExpression const &other) = delete;

    PredicateExpression(PredicateExpression &&) = default;

    PredicateExpression &operator=(PredicateExpression &&) = delete;

    virtual ~PredicateExpression() = default;

    // Override base class methods.
    virtual storm::expressions::OperatorType getOperator() const override;

    virtual bool evaluateAsBool(Valuation const *valuation = nullptr) const override;

    virtual std::shared_ptr<BaseExpression const> simplify() const override;

    virtual boost::any accept(ExpressionVisitor &visitor, boost::any const &data) const override;

    virtual bool isPredicateExpression() const override;

    virtual bool isFunctionApplication() const override;

    virtual bool containsVariables() const override;

    virtual uint_fast64_t getArity() const override;

    virtual std::shared_ptr<BaseExpression const> getOperand(uint_fast64_t operandIndex) const override;

    virtual void gatherVariables(std::set<storm::expressions::Variable> &variables) const override;

    /*!
     * Retrieves the relation associated with the expression.
     *
     * @return The relation associated with the expression.
     */
    PredicateType getPredicateType() const;

   protected:
    // Override base class method.
    virtual void printToStream(std::ostream &stream) const override;

   private:
    PredicateType predicate;
    std::vector<std::shared_ptr<BaseExpression const>> operands;
};
}  // namespace expressions
}  // namespace storm
