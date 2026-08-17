#pragma once

#include <map>

#include "storm/storage/prism/Variable.h"

namespace storm {
namespace prism {
class IntegerVariable : public Variable {
   public:
    // Create default implementations of constructors/assignment.
    IntegerVariable() = default;
    IntegerVariable(IntegerVariable const& other) = default;
    IntegerVariable& operator=(IntegerVariable const& other) = default;
    IntegerVariable(IntegerVariable&& other) = default;
    IntegerVariable& operator=(IntegerVariable&& other) = default;

    /*!
     * Creates an integer variable with the given initial value expression.
     *
     * @param variable The expression variable associated with this variable.
     * @param lowerBoundExpression A constant expression defining the lower bound of the domain of the variable.
     * @param upperBoundExpression A constant expression defining the upper bound of the domain of the variable.
     * @param initialValueExpression A constant expression that defines the initial value of the variable.
     * @param filename The filename in which the variable is defined.
     * @param lineNumber The line number in which the variable is defined.
     */
    IntegerVariable(storm::expressions::Variable const& variable, storm::expressions::Expression const& lowerBoundExpression,
                    storm::expressions::Expression const& upperBoundExpression, storm::expressions::Expression const& initialValueExpression, bool observable,
                    std::string const& filename = "", uint_fast64_t lineNumber = 0);

    /*!
     * @return true if a lower bound for this integer variable is defined
     */
    bool hasLowerBoundExpression() const;

    /*!
     * Retrieves an expression defining the lower bound for this integer variable.
     * @pre A lower bound for this integer variable is defined
     * @return An expression defining the lower bound for this integer variable.
     */
    storm::expressions::Expression const& getLowerBoundExpression() const;

    /*!
     * @return true if an upper bound for this integer variable is defined
     */
    bool hasUpperBoundExpression() const;

    /*!
     * Retrieves an expression defining the upper bound for this integer variable.
     * @pre An upper bound for this integer variable is defined
     * @return An expression defining the upper bound for this integer variable.
     */
    storm::expressions::Expression const& getUpperBoundExpression() const;

    /*!
     * Retrieves an expression characterizing the legal range of the variable.
     * Only bounds that are defined will be considered in this expression.
     * If neither a lower nor an upper bound is defined, this expression will be equivalent to true.
     *
     * @return An expression characterizing the legal range of the variable.
     */
    storm::expressions::Expression getRangeExpression() const;

    /*!
     * Substitutes all identifiers in the boolean variable according to the given map.
     *
     * @param substitution The substitution to perform.
     * @return The resulting boolean variable.
     */
    IntegerVariable substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;

    IntegerVariable substituteNonStandardPredicates() const;

    virtual void createMissingInitialValue() override;

    friend std::ostream& operator<<(std::ostream& stream, IntegerVariable const& variable);

   private:
    // A constant expression that specifies the lower bound of the domain of the variable.
    storm::expressions::Expression lowerBoundExpression;

    // A constant expression that specifies the upper bound of the domain of the variable.
    storm::expressions::Expression upperBoundExpression;
};

}  // namespace prism
}  // namespace storm
