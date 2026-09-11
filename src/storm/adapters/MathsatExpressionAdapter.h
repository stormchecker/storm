#pragma once

#include "storm-config.h"

#include <unordered_map>
#include <vector>

#ifdef STORM_HAVE_MATHSAT
#include <mathsat.h>
#endif

#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/expressions/Variable.h"

#ifdef STORM_HAVE_MATHSAT
namespace std {
// Define hashing operator for MathSAT's declarations.
template<>
struct hash<msat_decl> {
    size_t operator()(msat_decl const& declaration) const {
        return hash<void*>()(declaration.repr);
    }
};
}  // namespace std

// Define equality operator to make hashing work.
bool operator==(msat_decl decl1, msat_decl decl2);
#endif

namespace storm {
namespace expressions {
class Expression;
class ExpressionManager;
}  // namespace expressions

namespace adapters {

#ifdef STORM_HAVE_MATHSAT

class MathsatExpressionAdapter : public storm::expressions::ExpressionVisitor {
   public:
    /*!
     * Creates an expression adapter that can translate expressions to the format of MathSAT.
     *
     * @param manager The manager that can be used to build expressions.
     * @param env The MathSAT environment in which to build the expressions.
     */
    MathsatExpressionAdapter(storm::expressions::ExpressionManager& manager, msat_env& env);

    /*!
     * Translates the given expression to an equivalent term for MathSAT.
     *
     * @param expression The expression to be translated.
     * @return An equivalent term for MathSAT.
     */
    msat_term translateExpression(storm::expressions::Expression const& expression);

    /*!
     * Translates the given variable to an equivalent expression for MathSAT.
     *
     * @param variable The variable to translate.
     * @return An equivalent term for MathSAT.
     */
    msat_term translateExpression(storm::expressions::Variable const& variable);

    bool hasAdditionalConstraints() const;

    /*!
     * Retrieves additional constraints that were created because of encodings using auxiliary variables.
     */
    std::vector<msat_term> const& getAdditionalConstraints() const;

    /*!
     * Retrieves the variable that is associated with the given MathSAT variable declaration.
     *
     * @param msatVariableDeclaration The MathSAT variable declaration.
     * @return The variable associated with the given declaration.
     */
    storm::expressions::Variable const& getVariable(msat_decl msatVariableDeclaration) const;

    std::unordered_map<storm::expressions::Variable, msat_decl> const& getAllDeclaredVariables() const;

    virtual boost::any visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::BooleanLiteralExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::RationalLiteralExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::IntegerLiteralExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::VariableExpression const& expression, boost::any const& data) override;

    storm::expressions::Expression translateExpression(msat_term const& term);

   private:
    /*!
     * Creates a MathSAT variable for the provided variable.
     *
     * @param variable The variable for which to create a MathSAT counterpart.
     */
    msat_decl createVariable(storm::expressions::Variable const& variable);

    // The expression manager to use.
    storm::expressions::ExpressionManager& manager;

    // The MathSAT environment used.
    msat_env& env;

    // A vector of constraints that need to be kept separate, because they were only implicitly part of an
    // assertion that was added.
    std::vector<msat_term> additionalConstraints;

    // A mapping of variable names to their declaration in the MathSAT environment.
    std::unordered_map<storm::expressions::Variable, msat_decl> variableToDeclarationMapping;

    // A mapping from MathSAT variable declarations to our variables.
    std::unordered_map<msat_decl, storm::expressions::Variable> declarationToVariableMapping;
};
#endif
}  // namespace adapters
}  // namespace storm
