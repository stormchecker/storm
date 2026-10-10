#pragma once

#include <set>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/constants.h"

namespace storm {
namespace expressions {

/*!
 * Converts the given polynomial into a Storm expression.
 *
 * The coefficients of the polynomial are converted to rational literals and every variable occurring in the
 * polynomial is looked up in (or declared in) the given expression manager. The mapping from the polynomial's
 * variables to the expression manager's variables is performed by name, which mirrors the mapping performed by
 * storm::expressions::ToRationalFunctionVisitor.
 *
 * @param polynomial The polynomial to convert.
 * @param manager The expression manager that owns the resulting expression. Variables occurring in the polynomial
 * are declared in this manager if they are not known yet.
 * @return The expression representing the given polynomial.
 */
template<typename PolynomialType>
Expression polynomialToExpression(PolynomialType const& polynomial, std::shared_ptr<ExpressionManager> const& manager) {
    std::set<storm::RationalFunctionVariable> polynomialVariables;
    polynomial.gatherVariables(polynomialVariables);
    for (auto const& variable : polynomialVariables) {
        if (!manager->hasVariable(variable.name())) {
            manager->declareRationalVariable(variable.name());
        }
    }

    // Put the terms of the polynomial into carl's canonical order, so that the resulting expression does not
    // depend on the order in which the polynomial happens to store them.
    polynomial.makeOrdered();

    auto zero = storm::utility::convertNumber<storm::RationalNumber>(0);
    auto one = storm::utility::convertNumber<storm::RationalNumber>(1);

    Expression result;
    bool isFirstTerm = true;
    for (auto const& term : polynomial) {
        // The constant term has no monomial, in which case this set stays empty and the monomial is never
        // dereferenced.
        std::set<storm::RationalFunctionVariable> termVariables;
        term.gatherVariables(termVariables);

        auto coefficient = storm::utility::convertNumber<storm::RationalNumber, typename PolynomialType::CoeffType>(term.coeff());
        bool const isNegative = coefficient < zero;
        auto absoluteCoefficient = isNegative ? -coefficient : coefficient;

        Expression variablesPart;
        for (auto const& variable : termVariables) {
            auto exponent = static_cast<int64_t>(term.monomial()->exponentOfVariable(variable));
            auto variableExpression = manager->getVariable(variable.name());
            auto factor = exponent == 1 ? variableExpression : pow(variableExpression, manager->integer(exponent));
            variablesPart = variablesPart.isInitialized() ? variablesPart * factor : factor;
        }

        // Terms without variables are plain constants and keep their sign, since there is nothing to subtract
        // them from.
        bool const isConstantTerm = !variablesPart.isInitialized();
        Expression termExpression = isConstantTerm ? manager->rational(coefficient)
                                                   : (absoluteCoefficient == one ? variablesPart : manager->rational(absoluteCoefficient) * variablesPart);

        // Emit negative non-constant terms as a subtraction, so that the result reads naturally.
        if (isFirstTerm) {
            result = isNegative && !isConstantTerm ? -termExpression : termExpression;
        } else {
            result = isNegative && !isConstantTerm ? result - termExpression : result + termExpression;
        }
        isFirstTerm = false;
    }

    // The polynomial has no terms at all, so it represents zero.
    if (isFirstTerm) {
        result = manager->rational(zero);
    }
    return result;
}

}  // namespace expressions
}  // namespace storm
