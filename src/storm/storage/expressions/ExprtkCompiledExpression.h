#pragma once

#include "storm/adapters/ExprttkAdapter.h"
#include "storm/storage/expressions/CompiledExpression.h"

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
