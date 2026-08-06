#pragma once

#include <map>

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/storage/prism/LocatedInformation.h"

namespace storm {
namespace prism {
class Assignment : public LocatedInformation {
   public:
    /*!
     * Constructs an assignment using the given variable name and expression.
     *
     * @param variable The variable that this assignment targets.
     * @param expression The expression to assign to the variable.
     * @param filename The filename in which the assignment is defined.
     * @param lineNumber The line number in which the assignment is defined.
     */
    Assignment(storm::expressions::Variable const& variable, storm::expressions::Expression const& expression, std::string const& filename = "",
               uint_fast64_t lineNumber = 0);

    // Create default implementations of constructors/assignment.
    Assignment() = default;
    Assignment(Assignment const& other) = default;
    Assignment& operator=(Assignment const& other) = default;
    Assignment(Assignment&& other) = default;
    Assignment& operator=(Assignment&& other) = default;

    /*!
     * Retrieves the name of the variable that this assignment targets.
     *
     * @return The name of the variable that this assignment targets.
     */
    std::string const& getVariableName() const;

    /*!
     * Retrieves the variable that is written to by this assignment.
     *
     * @return The variable that is written to by this assignment.
     */
    storm::expressions::Variable const& getVariable() const;

    /*!
     * Retrieves the expression that is assigned to the variable.
     *
     * @return The expression that is assigned to the variable.
     */
    storm::expressions::Expression const& getExpression() const;

    /*!
     * Substitutes all variables in the assignment according to the given map.
     *
     * @param substitution The substitution to perform.
     * @return The resulting assignment.
     */
    Assignment substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;

    Assignment substituteNonStandardPredicates() const;

    /*!
     * Checks whether the assignment is an identity (lhs equals rhs)
     *
     * @return true iff the assignment is of the form a' = a.
     */
    bool isIdentity() const;

    friend std::ostream& operator<<(std::ostream& stream, Assignment const& assignment);

   private:
    // The variable written in this assignment.
    storm::expressions::Variable variable;

    // The expression that is assigned to the variable.
    storm::expressions::Expression expression;
};
}  // namespace prism
}  // namespace storm
