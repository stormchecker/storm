#pragma once

#include <unordered_map>
#include <vector>

#include "storm-config.h"

// Include the headers of Z3 only if it is available.
#ifdef STORM_HAVE_Z3
#include <z3++.h>
#include <z3.h>
#endif

#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace expressions {
class BaseExpression;
}

namespace adapters {

#ifdef STORM_HAVE_Z3
class Z3ExpressionAdapter : public storm::expressions::ExpressionVisitor {
   public:
    /*!
     * Creates an expression adapter that can translate expressions to the format of Z3.
     *
     * @param manager The manager that can be used to build expressions.
     * @param context A reference to the Z3 context over which to build the expressions. The lifetime of the
     * context needs to be guaranteed as long as the instance of this adapter is used.
     */
    Z3ExpressionAdapter(storm::expressions::ExpressionManager& manager, z3::context& context);

    /*!
     * Translates the given expression to an equivalent expression for Z3.
     *
     * @param expression The expression to translate.
     * @return An equivalent expression for Z3.
     */
    z3::expr translateExpression(storm::expressions::Expression const& expression);

    /*!
     * Translates the given variable to an equivalent expression for Z3.
     *
     * @param variable The variable to translate.
     * @return An equivalent expression for Z3.
     */
    z3::expr translateExpression(storm::expressions::Variable const& variable);

    storm::expressions::Expression translateExpression(z3::expr const& expr);

    /*!
     * Finds the counterpart to the given z3 variable declaration.
     *
     * @param z3Declaration The declaration for which to find the equivalent.
     * @return The equivalent counterpart.
     */
    storm::expressions::Variable const& getVariable(z3::func_decl z3Declaration);

    virtual boost::any visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::BooleanLiteralExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::RationalLiteralExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::IntegerLiteralExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::VariableExpression const& expression, boost::any const& data) override;

   private:
    /*!
     * Creates a Z3 variable for the provided variable.
     *
     * @param variable The variable for which to create a Z3 counterpart.
     */
    z3::expr createVariable(storm::expressions::Variable const& variable);

    // The manager that can be used to build expressions.
    storm::expressions::ExpressionManager& manager;

    // The context that is used to translate the expressions.
    z3::context& context;

    // A vector of assertions that need to be kept separate, because they were only implicitly part of an
    // assertion that was added.
    std::vector<z3::expr> additionalAssertions;

    // A mapping from variables to their Z3 equivalent.
    std::unordered_map<storm::expressions::Variable, z3::expr> variableToExpressionMapping;

    // A mapping from z3 declarations to the corresponding variables.
    std::unordered_map<Z3_func_decl, storm::expressions::Variable> declarationToVariableMapping;

    // A cache of already translated constraints. Only valid during the translation of one expression.
    std::unordered_map<storm::expressions::BaseExpression const*, z3::expr> expressionCache;
};
#endif
}  // namespace adapters
}  // namespace storm
