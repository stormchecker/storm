#pragma once

#include <functional>

#include "storm/expressions/Expression.h"
#include "storm/languages/jani/LValue.h"

namespace storm {
namespace jani {

class Assignment {
   public:
    /*!
     * Creates an assignment of the given expression to the given LValue.
     */
    Assignment(storm::jani::LValue const& lValue, storm::expressions::Expression const& expression, int64_t level = 0);

    /*!
     * Creates an assignment of the given expression to the given Variable.
     */
    Assignment(storm::jani::Variable const& lValue, storm::expressions::Expression const&, int64_t level = 0);

    Assignment(Assignment const&) = default;
    bool operator==(Assignment const& other) const;

    /*!
     * Returns true if the lValue is a variable
     */
    bool lValueIsVariable() const;

    /*!
     * Returns true if the lValue is an array access
     */
    bool lValueIsArrayAccess() const;

    /*!
     * Retrieves the lValue that is written in this assignment.
     */
    storm::jani::LValue const& getLValue() const;

    /*!
     * Retrieves the lValue that is written in this assignment.
     */
    storm::jani::LValue& getLValue();

    /*!
     * Retrieves the Variable that is written in this assignment.
     * This assumes that the lValue is a variable.
     */
    storm::jani::Variable const& getVariable() const;

    /*!
     * Retrieves the expression variable that is written in this assignment.
     * This assumes that the lValue is a variable.
     */
    storm::expressions::Variable const& getExpressionVariable() const;

    /*!
     * Retrieves the expression whose value is assigned to the target variable.
     */
    storm::expressions::Expression const& getAssignedExpression() const;

    /*!
     * Sets a new expression that is assigned to the target LValue.
     */
    void setAssignedExpression(storm::expressions::Expression const& expression);

    /*!
     * Substitutes all variables in all expressions according to the given substitution.
     */
    void substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution, bool const substituteTranscendentalNumbers);

    /**
     * Retrieves whether the assignment assigns to a transient variable.
     */
    bool isTransient() const;

    /*!
     * Retrieves the level of the assignment.
     */
    int64_t getLevel() const;

    /*!
     * Sets the level
     */
    void setLevel(int64_t level);

    /*!
     * Checks the assignment for linearity.
     */
    bool isLinear() const;

    friend std::ostream& operator<<(std::ostream& stream, Assignment const& assignment);

   private:
    // The lValue being assigned.
    LValue lValue;

    // The expression that is being assigned to the lValue.
    storm::expressions::Expression expression;

    // The level of the assignment.
    int64_t level;
};

/*!
 * This functor enables ordering the assignments first by the assignment level and then by lValue.
 * Note that this is a partial order.
 */
struct AssignmentPartialOrderByLevelAndLValue {
    bool operator()(Assignment const& left, Assignment const& right) const;
    bool operator()(Assignment const& left, std::shared_ptr<Assignment> const& right) const;
    bool operator()(std::shared_ptr<Assignment> const& left, std::shared_ptr<Assignment> const& right) const;
    bool operator()(std::shared_ptr<Assignment> const& left, Assignment const& right) const;
};

}  // namespace jani
}  // namespace storm
