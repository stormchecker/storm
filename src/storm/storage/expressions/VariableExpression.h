#pragma once

#include "storm/storage/expressions/BaseExpression.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/utility/OsDetection.h"

namespace storm {
namespace expressions {
class VariableExpression : public BaseExpression {
   public:
    /*!
     * Creates a variable expression with the given return type and variable name.
     *
     * @param returnType The return type of the variable expression.
     * @param variableName The name of the variable associated with this expression.
     */
    VariableExpression(Variable const& variable);

    // Instantiate constructors and assignments with their default implementations.
    VariableExpression(VariableExpression const&) = default;
    VariableExpression& operator=(VariableExpression const&) = delete;
    VariableExpression(VariableExpression&&) = default;
    VariableExpression& operator=(VariableExpression&&) = delete;

    virtual ~VariableExpression() = default;

    // Override base class methods.
    virtual bool evaluateAsBool(Valuation const* valuation = nullptr) const override;
    virtual int_fast64_t evaluateAsInt(Valuation const* valuation = nullptr) const override;
    virtual double evaluateAsDouble(Valuation const* valuation = nullptr) const override;
    virtual std::string const& getIdentifier() const override;
    virtual bool containsVariables() const override;
    virtual bool isVariable() const override;
    virtual void gatherVariables(std::set<storm::expressions::Variable>& variables) const override;
    virtual std::shared_ptr<BaseExpression const> simplify() const override;
    virtual boost::any accept(ExpressionVisitor& visitor, boost::any const& data) const override;
    virtual bool isVariableExpression() const override;

    /*!
     * Retrieves the name of the variable associated with this expression.
     *
     * @return The name of the variable.
     */
    std::string const& getVariableName() const;

    /*!
     * Retrieves the variable associated with this expression.
     *
     * @return The variable associated with this expression.
     */
    Variable const& getVariable() const;

   protected:
    // Override base class method.
    virtual void printToStream(std::ostream& stream) const override;

   private:
    // The variable that is represented by this expression.
    Variable variable;
};
}  // namespace expressions
}  // namespace storm
