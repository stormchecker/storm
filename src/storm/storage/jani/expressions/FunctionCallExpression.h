#pragma once

#include <memory>
#include <string>
#include <vector>

#include "storm/storage/expressions/BaseExpression.h"

namespace storm {
namespace expressions {
/*!
 * Represents an array with a given list of elements.
 */
class FunctionCallExpression : public BaseExpression {
   public:
    FunctionCallExpression(ExpressionManager const& manager, Type const& type, std::string const& functionIdentifier,
                           std::vector<std::shared_ptr<BaseExpression const>> const& arguments);

    // Instantiate constructors and assignments with their default implementations.
    FunctionCallExpression(FunctionCallExpression const& other) = default;
    FunctionCallExpression& operator=(FunctionCallExpression const& other) = delete;
    FunctionCallExpression(FunctionCallExpression&&) = default;
    FunctionCallExpression& operator=(FunctionCallExpression&&) = delete;

    virtual ~FunctionCallExpression() = default;

    virtual void gatherVariables(std::set<storm::expressions::Variable>& variables) const override;
    virtual bool containsVariables() const override;
    virtual std::shared_ptr<BaseExpression const> simplify() const override;
    virtual boost::any accept(ExpressionVisitor& visitor, boost::any const& data) const override;

    std::string const& getFunctionIdentifier() const;
    uint64_t getNumberOfArguments() const;
    std::shared_ptr<BaseExpression const> getArgument(uint64_t i) const;
    std::vector<std::shared_ptr<BaseExpression const>> const& getArguments() const;

   protected:
    virtual void printToStream(std::ostream& stream) const override;

   private:
    std::string identifier;
    std::vector<std::shared_ptr<BaseExpression const>> arguments;
};
}  // namespace expressions
}  // namespace storm