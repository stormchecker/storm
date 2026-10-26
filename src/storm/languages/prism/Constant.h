#pragma once

#include <map>

#include "storm/expressions/Expression.h"
#include "storm/expressions/Variable.h"
#include "storm/languages/prism/LocatedInformation.h"

namespace storm {
namespace prism {
class Constant : public LocatedInformation {
   public:
    /*!
     * Creates a defined constant.
     *
     * @param variable The expression variable associated with the constant.
     * @param expression The expression that defines the constant.
     * @param filename The filename in which the transition reward is defined.
     * @param lineNumber The line number in which the transition reward is defined.
     */
    Constant(storm::expressions::Variable const& variable, storm::expressions::Expression const& expression, std::string const& filename = "",
             uint_fast64_t lineNumber = 0);

    /*!
     * Creates an undefined constant.
     *
     * @param variable The expression variable associated with the constant.
     * @param filename The filename in which the transition reward is defined.
     * @param lineNumber The line number in which the transition reward is defined.
     */
    Constant(storm::expressions::Variable const& variable, std::string const& filename = "", uint_fast64_t lineNumber = 0);

    // Create default implementations of constructors/assignment.
    Constant() = default;
    Constant(Constant const& other) = default;
    Constant& operator=(Constant const& other) = default;
    Constant(Constant&& other) = default;
    Constant& operator=(Constant&& other) = default;

    /*!
     * Retrieves the name of the constant.
     *
     * @return The name of the constant.
     */
    std::string const& getName() const;

    /*!
     * Retrieves the type of the constant.
     *
     * @return The type of the constant;
     */
    storm::expressions::Type const& getType() const;

    /*!
     * Retrieves the expression variable associated with this constant.
     *
     * @return The expression variable associated with this constant.
     */
    storm::expressions::Variable const& getExpressionVariable() const;

    /*!
     * Retrieves whether the constant is defined, i.e., whether there is an expression defining its value.
     *
     * @return True iff the constant is defined.
     */
    bool isDefined() const;

    /*!
     * Retrieves the expression that defines the constant. Note that the expression may not be initialized.
     *
     * @return The expression that defines the constant.
     */
    storm::expressions::Expression const& getExpression() const;

    /*!
     * Substitutes all identifiers in the constant according to the given map.
     *
     * @param substitution The substitution to perform.
     * @return The resulting constant.
     */
    Constant substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;

    friend std::ostream& operator<<(std::ostream& stream, Constant const& constant);

   private:
    // The expression variable associated with the constant.
    storm::expressions::Variable variable;

    // The expression that defines the constant (in case it is defined).
    storm::expressions::Expression expression;
};

bool operator==(Constant const& lhs, Constant const& rhs);

}  // namespace prism
}  // namespace storm
