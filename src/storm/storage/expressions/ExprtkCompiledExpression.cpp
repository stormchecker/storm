#include "ExprtkCompiledExpression.h"

namespace storm {
namespace expressions {

ExprtkCompiledExpression::ExprtkCompiledExpression(CompiledExpressionType const& exprtkCompiledExpression)
    : exprtkCompiledExpression(exprtkCompiledExpression) {
    // Intentionally left empty.
}

ExprtkCompiledExpression::CompiledExpressionType const& ExprtkCompiledExpression::getCompiledExpression() const {
    return exprtkCompiledExpression;
}

bool ExprtkCompiledExpression::isExprtkCompiledExpression() const {
    return true;
}

}  // namespace expressions
}  // namespace storm
