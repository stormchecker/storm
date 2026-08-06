#pragma once

#include <boost/optional.hpp>
#include <map>

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/storage/prism/LocatedInformation.h"

namespace storm {
namespace prism {
class Formula : public LocatedInformation {
   public:
    /*!
     * Creates a formula with the given placeholder variable and expression.
     *
     * @param variable The placeholder variable that is used in expressions to represent this formula.
     * @param expression The expression associated with this formula.
     * @param filename The filename in which the transition reward is defined.
     * @param lineNumber The line number in which the transition reward is defined.
     */
    Formula(storm::expressions::Variable const& variable, storm::expressions::Expression const& expression, std::string const& filename = "",
            uint_fast64_t lineNumber = 0);

    /*!
     * Creates a formula with the given name and the assigning expression
     *
     * @param name the name of the formula.
     * @param expression The expression associated with this formula.
     * @param filename The filename in which the transition reward is defined.
     * @param lineNumber The line number in which the transition reward is defined.
     */
    Formula(std::string const& name, storm::expressions::Expression const& expression, std::string const& filename = "", uint_fast64_t lineNumber = 0);

    /*!
     * Creates a formula with the given name
     *
     * @param name the name of the formula.
     * @param filename The filename in which the transition reward is defined.
     * @param lineNumber The line number in which the transition reward is defined.
     */
    Formula(std::string const& name, std::string const& filename = "", uint_fast64_t lineNumber = 0);

    // Create default implementations of constructors/assignment.
    Formula() = default;
    Formula(Formula const& other) = default;
    Formula& operator=(Formula const& other) = default;
    Formula(Formula&& other) = default;
    Formula& operator=(Formula&& other) = default;

    /*!
     * Retrieves the name that is associated with this formula.
     *
     * @return The name that is associated with this formula.
     */
    std::string const& getName() const;

    /*!
     * Retrieves wheter a placeholder variable is used in expressions to represent this formula.
     *
     */
    bool hasExpressionVariable() const;

    /*!
     * Retrieves the placeholder variable that is used in expressions to represent this formula.
     *
     * @return The placeholder variable that is used in expressions to represent this formula.
     */
    storm::expressions::Variable const& getExpressionVariable() const;

    /*!
     * Retrieves the expression that is associated with this formula.
     *
     * @return The expression that is associated with this formula.
     */
    storm::expressions::Expression const& getExpression() const;

    /*!
     * Retrieves the return type of the formula, i.e., the return-type of the defining expression.
     *
     * @return The return type of the formula.
     */
    storm::expressions::Type const& getType() const;

    /*!
     * Substitutes all variables in the expression of the formula according to the given map.
     * Will not substitute the placeholder variable (if given).
     *
     * @param substitution The substitution to perform.
     * @return The resulting formula.
     */
    Formula substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;
    Formula substituteNonStandardPredicates() const;

    friend std::ostream& operator<<(std::ostream& stream, Formula const& formula);

   private:
    // The name of the formula.
    std::string name;

    // Expression variable that is used as a placeholder for this formula
    boost::optional<storm::expressions::Variable> variable;

    // A predicate that needs to be satisfied by states for the label to be attached.
    storm::expressions::Expression expression;
};
}  // namespace prism
}  // namespace storm
