#pragma once

#include <map>
#include <optional>

#include "storm/expressions/Expression.h"
#include "storm/expressions/ExpressionVisitor.h"
#include "storm/expressions/Variable.h"

namespace storm {
namespace expressions {

/*!
 * Computes the coefficients of a linear expression. Instantiated for double and storm::RationalNumber.
 */
template<typename ValueType>
class LinearCoefficientVisitor : public ExpressionVisitor {
   public:
    struct VariableCoefficients {
       public:
        VariableCoefficients(ValueType constantPart = ValueType(0));

        VariableCoefficients(VariableCoefficients const& other) = default;
        VariableCoefficients& operator=(VariableCoefficients const& other) = default;
        VariableCoefficients(VariableCoefficients&& other) = default;
        VariableCoefficients& operator=(VariableCoefficients&& other) = default;

        VariableCoefficients& operator+=(VariableCoefficients&& other);
        VariableCoefficients& operator-=(VariableCoefficients&& other);
        VariableCoefficients& operator*=(VariableCoefficients&& other);
        VariableCoefficients& operator/=(VariableCoefficients&& other);

        void negate();
        void setCoefficient(storm::expressions::Variable const& variable, ValueType const& coefficient);
        ValueType getCoefficient(storm::expressions::Variable const& variable);
        ValueType const& getConstantPart() const;

        /*!
         * Brings all variables of the right-hand side coefficients to the left-hand side by negating them and
         * moves the constant part of the current coefficients to the right-hand side by subtracting it from the
         * constant part of the rhs. After performing this operation, the left-hand side has a constant part of
         * 0 and all variables and the right-hand side has no variables, but possibly a non-zero constant part.
         *
         * @param other The variable coefficients of the right-hand side.
         */
        void separateVariablesFromConstantPart(VariableCoefficients& rhs);

        // Propagate the iterators to variable-coefficient pairs.
        typename std::map<storm::expressions::Variable, ValueType>::const_iterator begin() const;
        typename std::map<storm::expressions::Variable, ValueType>::const_iterator end() const;

        size_t size() const;

       private:
        // Variables whose coefficient cancelled out are not stored.
        void removeZeroCoefficients();

        std::map<storm::expressions::Variable, ValueType> variableToCoefficientMapping;
        ValueType constantPart;
    };

    /*!
     * Computes the coefficients of all identifiers appearing in the expression if the expression was rewritten
     * as a sum of atoms. If the expression is not linear, an exception is thrown.
     */
    VariableCoefficients getLinearCoefficients(Expression const& expression);

    /*!
     * As getLinearCoefficients, but returns std::nullopt instead of throwing if the expression is not linear.
     */
    std::optional<VariableCoefficients> tryGetLinearCoefficients(Expression const& expression);

    virtual boost::any visit(IfThenElseExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(BinaryBooleanFunctionExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(BinaryNumericalFunctionExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(BinaryRelationExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(VariableExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(UnaryBooleanFunctionExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(UnaryNumericalFunctionExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(BooleanLiteralExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(IntegerLiteralExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(RationalLiteralExpression const& expression, boost::any const& data) override;

   private:
    // The visit methods return a std::optional<VariableCoefficients>, which is empty if the expression is not linear.
    using Result = std::optional<VariableCoefficients>;
};
}  // namespace expressions
}  // namespace storm
