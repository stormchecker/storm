#pragma once

#include <stack>

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/expressions/SimpleValuation.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace expressions {
class LinearCoefficientVisitor : public ExpressionVisitor {
   public:
    struct VariableCoefficients {
       public:
        VariableCoefficients(double constantPart = 0);

        VariableCoefficients(VariableCoefficients const& other) = default;
        VariableCoefficients& operator=(VariableCoefficients const& other) = default;
        VariableCoefficients(VariableCoefficients&& other) = default;
        VariableCoefficients& operator=(VariableCoefficients&& other) = default;

        VariableCoefficients& operator+=(VariableCoefficients&& other);
        VariableCoefficients& operator-=(VariableCoefficients&& other);
        VariableCoefficients& operator*=(VariableCoefficients&& other);
        VariableCoefficients& operator/=(VariableCoefficients&& other);

        void negate();
        void setCoefficient(storm::expressions::Variable const& variable, double coefficient);
        double getCoefficient(storm::expressions::Variable const& variable);
        double getConstantPart() const;

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
        std::map<storm::expressions::Variable, double>::const_iterator begin() const;
        std::map<storm::expressions::Variable, double>::const_iterator end() const;

        size_t size() const;

       private:
        std::map<storm::expressions::Variable, double> variableToCoefficientMapping;
        double constantPart;
    };

    /*!
     * Creates a linear coefficient visitor.
     */
    LinearCoefficientVisitor() = default;

    /*!
     * Computes the (double) coefficients of all identifiers appearing in the expression if the expression
     * was rewritten as a sum of atoms.. If the expression is not linear, an exception is thrown.
     *
     * @param expression The expression for which to compute the coefficients.
     * @return A structure representing the coefficients of the variables and the constant part.
     */
    VariableCoefficients getLinearCoefficients(Expression const& expression);

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
};
}  // namespace expressions
}  // namespace storm
