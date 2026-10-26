#pragma once

namespace storm {
namespace expressions {

class ExprtkCompiledExpression;

class CompiledExpression {
   public:
    virtual ~CompiledExpression() = default;

    virtual bool isExprtkCompiledExpression() const;
    ExprtkCompiledExpression& asExprtkCompiledExpression();
    ExprtkCompiledExpression const& asExprtkCompiledExpression() const;

   private:
    // Currently empty.
};

}  // namespace expressions
}  // namespace storm
