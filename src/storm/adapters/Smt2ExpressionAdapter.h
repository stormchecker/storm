#pragma once

#include <unordered_map>

#include "storm-config.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/ExpressionEvaluationException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/utility/macros.h"

namespace storm {
namespace adapters {

class Smt2ExpressionAdapter {
   public:
    /*!
     * Creates an expression adapter that can translate expressions to the format of Smt2.
     *
     * @param manager The manager that can be used to build expressions.
     * @param useReadableVarNames sets whether the expressions should use human readable names for the variables or the internal representation
     */
    Smt2ExpressionAdapter(storm::expressions::ExpressionManager&, bool useReadableVarNames) : useReadableVarNames(useReadableVarNames) {
        declaredVariables.emplace_back(std::set<std::string>());
    }

    /*!
     * Translates the given expression to an equivalent expression for Smt2.
     *
     * @param expression The expression to translate.
     * @return An equivalent expression for Smt2.
     */
    std::string translateExpression(storm::expressions::Expression const&) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not (yet) implemented.");
    }

    /*!
     * Translates the given constraint "leftHandSide relation rightHandSide" to an equivalent expression for Smt2.

     * @param leftHandSide
     * @param relation
     * @param RightHandSide
     * @return An equivalent expression for Smt2.
     */
    std::string translateExpression(storm::RationalFunction const& leftHandSide, storm::CompareRelation const& relation,
                                    storm::RationalFunction const& rightHandSide) {
        return "(" + carl::toString(relation) + " (/ " + leftHandSide.nominator().toString(false, useReadableVarNames) + " " +
               leftHandSide.denominator().toString(false, useReadableVarNames) + ") (/ " + rightHandSide.nominator().toString(false, useReadableVarNames) +
               " " + rightHandSide.denominator().toString(false, useReadableVarNames) + ") " + ")";
    }

    /*!
     * Translates the given constraint "leftHandSide relation 0" to an equivalent expression for Smt2.

     * @param constraint
     * @return An equivalent expression for Smt2.
     */
    std::string translateExpression(storm::RationalFunction const& leftHandSide, storm::CompareRelation const& relation) {
        std::stringstream ss;
        ss << "(" << relation << " " << leftHandSide.toString(false, useReadableVarNames) << " " << "0 " << ")";
        return ss.str();
    }

    /*!
     * Translates the given variable to an equivalent expression for Smt2.
     *
     * @param variable The variable to translate.
     * @return An equivalent expression for smt2.
     */
    std::string translateExpression(storm::expressions::Variable const&) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not (yet) implemented.");
    }

    /*!
     * Finds the counterpart to the given smt2 variable declaration.
     *
     * @param smt2Declaration The declaration for which to find the equivalent.
     * @return The equivalent counterpart.
     */
    storm::expressions::Variable const& getVariable(std::string const&) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not (yet) implemented.");
    }

    void increaseScope(uint_fast64_t n = 1) {
        for (uint_fast64_t i = 0; i < n; ++i) {
            declaredVariables.emplace_back(std::set<std::string>());
        }
    }

    void decreaseScope(uint_fast64_t n = 1) {
        STORM_LOG_THROW(declaredVariables.size() >= n, storm::exceptions::InvalidArgumentException, "Scope increased too much. Too many calls of pop()?");
        for (uint_fast64_t i = 0; i < n; ++i) {
            declaredVariables.pop_back();
        }
    }

    /*! Checks whether the variables in the given set are already declared and creates them if necessary
     *  @param variables the set of variables to check
     */
    std::vector<std::string> const checkForUndeclaredVariables(std::set<storm::RationalFunctionVariable> const& variables) {
        std::vector<std::string> result;
        carl::VariablePool& vPool = carl::VariablePool::getInstance();
        for (storm::RationalFunctionVariable const& variableToCheck : variables) {
            std::string const& variableString = vPool.getName(variableToCheck, useReadableVarNames);
            // first check if this variable is already declared
            bool alreadyDeclared = false;
            for (std::set<std::string> const& variables : declaredVariables) {
                if (variables.find(variableString) != variables.end()) {
                    alreadyDeclared = true;
                    break;
                }
            }
            // secondly, declare the variable if necessary
            if (!alreadyDeclared) {
                STORM_LOG_DEBUG("Declaring the variable " + variableString);
                declaredVariables.back().insert(variableString);
                std::string varDeclaration = "( declare-fun " + variableString + " () ";
                switch (variableToCheck.type()) {
                    case carl::VariableType::VT_BOOL:
                        varDeclaration += "Bool";
                        break;
                    case carl::VariableType::VT_REAL:
                        varDeclaration += "Real";
                        break;
                    case carl::VariableType::VT_INT:
                        varDeclaration += "Int";
                        break;
                    default:
                        STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "The type of the variable is not supported.");
                }
                varDeclaration += " )";
                result.push_back(varDeclaration);
            }
        }
        return result;
    }

   private:
    // The manager that can be used to build expressions.
    // storm::expressions::ExpressionManager& manager;
    // A flag to decide whether readable var names should be used instead of intern representation
    bool useReadableVarNames;
    // the declared variables for the different scopes
    std::vector<std::set<std::string>> declaredVariables;
};
}  // namespace adapters
}  // namespace storm
