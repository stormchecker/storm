#pragma once

#include <boost/optional.hpp>
#include <string>

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace jani {

class Constant {
   public:
    /*!
     * Creates a constant.
     */
    Constant(std::string const& name, storm::expressions::Variable const& variable,
             storm::expressions::Expression const& definingExpression = storm::expressions::Expression(),
             storm::expressions::Expression const& constraintExpression = storm::expressions::Expression());

    /*!
     * Retrieves the name of the constant.
     */
    std::string const& getName() const;

    /*!
     * Retrieves whether the constant is defined in the sense that it has a defining expression.
     */
    bool isDefined() const;

    /*!
     * Defines the constant with the given expression.
     * If a constraintExpression is set, it is checked whether the constant definition satisfies the given constraint.
     */
    void define(storm::expressions::Expression const& expression);

    /*!
     * Retrieves the type of the constant.
     */
    storm::expressions::Type const& getType() const;

    /*!
     * Retrieves whether the constant is a boolean constant.
     */
    bool isBooleanConstant() const;

    /*!
     * Retrieves whether the constant is an integer constant.
     */
    bool isIntegerConstant() const;

    /*!
     * Retrieves whether the constant is a real constant.
     */
    bool isRealConstant() const;

    /*!
     * Retrieves the expression variable associated with this constant.
     */
    storm::expressions::Variable const& getExpressionVariable() const;

    /*!
     * Retrieves the expression that defines this constant (if any).
     */
    storm::expressions::Expression const& getExpression() const;

    /*!
     * Retrieves whether there is a constraint for the possible values of this constant
     */
    bool hasConstraint() const;

    /*!
     * Retrieves the expression that constraints the possible values of this constant (if any).
     */
    storm::expressions::Expression const& getConstraintExpression() const;

    /*!
     * Sets a constraint expression. An exception is thrown if this constant is defined and the definition violates the given constraint.
     */
    void setConstraintExpression(storm::expressions::Expression const& expression);

   private:
    // The name of the constant.
    std::string name;

    // The expression variable associated with the constant.
    storm::expressions::Variable variable;

    // The expression defining the constant (if any).
    storm::expressions::Expression definingExpression;

    // The expression constraining possible values of the constant (if any).
    storm::expressions::Expression constraintExpression;
};

}  // namespace jani
}  // namespace storm
