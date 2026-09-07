#include "CompiledExpression.h"

#include "storm/storage/expressions/ExprtkCompiledExpression.h"

namespace storm {
namespace expressions {

bool CompiledExpression::isExprtkCompiledExpression() const {
    return false;
}

ExprtkCompiledExpression& CompiledExpression::asExprtkCompiledExpression() {
    return static_cast<ExprtkCompiledExpression&>(*this);
}

ExprtkCompiledExpression const& CompiledExpression::asExprtkCompiledExpression() const {
    return static_cast<ExprtkCompiledExpression const&>(*this);
}

}  // namespace expressions
}  // namespace storm
