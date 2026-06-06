#pragma once
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/sparse/Valuations.h"

namespace storm::storage::sparse {

/*!
 * Transforms the given state valuations to a new state valuations over a new variable set.
 * The values of the new variables are determined by evaluating the provided expressions w.r.t. the old variable valuation.
 * The freshly introduced variables may either replace or extend the existing variable set.
 */
class ValuationTransformer {
   public:
    ValuationTransformer(Valuations const& oldValuations);

    /*!
     * Add a variable defined by the given expression.
     * Note that these should all be over the same expression manager and both should have the same type.
     * @param var A variable
     * @param expr An expression
     */
    void addExpression(storm::expressions::Variable const& var, storm::expressions::Expression const& expr);

    /*!
     * Build and export the state valuations. Should be called only once.
     * @param extend Whether to maintain also the existing variables.
     * @return
     */
    Valuations build(bool extend);

   private:
    Valuations const& oldValuations;
    std::vector<storm::expressions::Variable> variables;
    std::vector<storm::expressions::Expression> expressions;
};

}  // namespace storm::storage::sparse