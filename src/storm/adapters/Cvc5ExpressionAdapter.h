#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "storm-config.h"

// Include the headers of CVC5 only if it is available.
#ifdef STORM_HAVE_CVC5
#include <cvc5/cvc5.h>
#endif

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace expressions {
class BaseExpression;
}

namespace adapters {

#ifdef STORM_HAVE_CVC5
class Cvc5ExpressionAdapter : public storm::expressions::ExpressionVisitor {
   public:
    /*!
     * Creates an expression adapter that can translate expressions to the format of CVC5.
     *
     * @param manager The manager that can be used to build expressions.
     * @param solver A reference to the CVC5 solver over which to build the expressions. Its term manager is used
     * for all terms and sorts, so the lifetime of the solver needs to be guaranteed as long as the instance of
     * this adapter is used.
     */
    Cvc5ExpressionAdapter(storm::expressions::ExpressionManager& manager, cvc5::Solver& solver);

    /*!
     * Translates the given expression to an equivalent expression for CVC5.
     *
     * @param expression The expression to translate.
     * @return An equivalent expression for CVC5.
     */
    cvc5::Term translateExpression(storm::expressions::Expression const& expression);

    /*!
     * Translates the given variable to an equivalent expression for CVC5.
     *
     * @param variable The variable to translate.
     * @return An equivalent expression for CVC5.
     */
    cvc5::Term translateExpression(storm::expressions::Variable const& variable);

    /*!
     * Translates the given CVC5 term back to a Storm expression.
     *
     * @param term The term to translate.
     * @return An equivalent Storm expression.
     */
    storm::expressions::Expression translateExpression(cvc5::Term const& term);

    /*!
     * Finds the counterpart to the given CVC5 constant.
     *
     * @param constant The constant for which to find the equivalent.
     * @return The equivalent counterpart.
     */
    storm::expressions::Variable const& getVariable(cvc5::Term const& constant);

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
     * Creates a CVC5 constant for the provided variable.
     *
     * @param variable The variable for which to create a CVC5 counterpart.
     */
    cvc5::Term createVariable(storm::expressions::Variable const& variable);

    /*!
     * Creates a CVC5 term for the given (non-negative) rational number.
     */
    cvc5::Term createRational(storm::RationalNumber const& value);

    /*!
     * Converts the given numerical term to a real number if it is an integer and a real number is expected.
     *
     * CVC5 does not coerce between integers and real numbers, so where Storm allows a mixed expression, such
     * as the real number 1.0 and the integer 1, we have to insert the conversion ourselves.
     *
     * @param term The term to convert if necessary.
     * @param isRealExpected Whether the context of the term expects a real number.
     * @return The given term, converted to a real number if necessary.
     */
    cvc5::Term convertToRealIfExpected(cvc5::Term const& term, bool const& isRealExpected) const;

    /*!
     * Makes sure that the two given numerical terms are of the same type.
     *
     * If one of them is an integer and the other one is a real number, the integer is converted to a real
     * number, as CVC5 requires the operands of a comparison to be of equal type.
     *
     * @param leftTerm The first term. May be replaced by its conversion to a real number.
     * @param rightTerm The second term. May be replaced by its conversion to a real number.
     */
    void unifyNumericalTypes(cvc5::Term& leftTerm, cvc5::Term& rightTerm) const;

    // The manager that can be used to build expressions.
    storm::expressions::ExpressionManager& manager;

    // The manager of the solver the adapter translates for, which is where terms and sorts are built.
    cvc5::TermManager& termManager;

    // A mapping from variables to their CVC5 equivalent.
    std::unordered_map<storm::expressions::Variable, cvc5::Term> variableToExpressionMapping;

    // A mapping from CVC5 constants to the corresponding variables.
    std::unordered_map<cvc5::Term, storm::expressions::Variable> expressionToVariableMapping;

    // A cache of already translated constraints. Only valid during the translation of one expression.
    std::unordered_map<storm::expressions::BaseExpression const*, cvc5::Term> expressionCache;
};
#endif
}  // namespace adapters
}  // namespace storm
