#pragma once

#include "storm/expressions/CompiledExpression.h"

#include "storm/adapters/ExprttkAdapter.h"

namespace storm {
namespace expressions {

class ExprtkCompiledExpression : public CompiledExpression {
   public:
    typedef exprtk::expression<double> CompiledExpressionType;

    ExprtkCompiledExpression(CompiledExpressionType const& exprtkCompiledExpression);

    CompiledExpressionType const& getCompiledExpression() const;

    virtual bool isExprtkCompiledExpression() const override;

   private:
    CompiledExpressionType exprtkCompiledExpression;
};

}  // namespace expressions
}  // namespace storm
