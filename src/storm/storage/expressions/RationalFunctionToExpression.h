#pragma once

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace expressions {
template<typename ValueType>
class RationalFunctionToExpression {
   public:
    RationalFunctionToExpression(std::shared_ptr<ExpressionManager> manager);

    /*!
     * Retrieves the manager responsible for the variables of this valuation.
     *
     * @return The pointer to the manager.
     */
    std::shared_ptr<ExpressionManager> getManager();

    /*!
     * Transforms the function into an expression.
     *
     * @param function The function to transform
     * @return The created expression.
     */
    Expression toExpression(ValueType function);

   private:
    // The manager responsible for the variables of this valuation.
    std::shared_ptr<ExpressionManager> manager;
};
}  // namespace expressions
}  // namespace storm
