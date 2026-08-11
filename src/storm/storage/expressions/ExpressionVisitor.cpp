#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
boost::any ExpressionVisitor::visit(PredicateExpression const&, boost::any const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Predicate Expressions are not supported by this visitor.");
}
}  // namespace expressions
}  // namespace storm