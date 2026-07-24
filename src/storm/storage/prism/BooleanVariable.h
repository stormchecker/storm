#pragma once

#include <map>

#include "storm/storage/prism/Variable.h"

namespace storm {
namespace prism {
class BooleanVariable : public Variable {
   public:
    // Create default implementations of constructors/assignment.
    BooleanVariable() = default;
    BooleanVariable(BooleanVariable const& other) = default;
    BooleanVariable& operator=(BooleanVariable const& other) = default;
    BooleanVariable(BooleanVariable&& other) = default;
    BooleanVariable& operator=(BooleanVariable&& other) = default;

    /*!
     * Creates a boolean variable with the given constant initial value expression.
     *
     * @param variable The expression variable associated with this variable.
     * @param initialValueExpression The constant expression that defines the initial value of the variable.
     * @param filename The filename in which the variable is defined.
     * @param lineNumber The line number in which the variable is defined.
     */
    BooleanVariable(storm::expressions::Variable const& variable, storm::expressions::Expression const& initialValueExpression, bool observable,
                    std::string const& filename = "", uint_fast64_t lineNumber = 0);

    /*!
     * Substitutes all identifiers in the boolean variable according to the given map.
     *
     * @param substitution The substitution to perform.
     * @return The resulting boolean variable.
     */
    BooleanVariable substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;
    BooleanVariable substituteNonStandardPredicates() const;

    virtual void createMissingInitialValue() override;

    friend std::ostream& operator<<(std::ostream& stream, BooleanVariable const& variable);
};

}  // namespace prism
}  // namespace storm
