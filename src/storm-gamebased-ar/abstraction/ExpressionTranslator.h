#pragma once

#include <set>

#include "storm/dd/DdType.h"
#include "storm/expressions/EquivalenceChecker.h"
#include "storm/expressions/ExpressionVisitor.h"

#include "storm/solver/SmtSolver.h"

namespace storm {
namespace dd {
template<storm::dd::DdType DdType>
class Bdd;
}

namespace expressions {
class Expression;
}
}  // namespace storm

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType DdType>
class AbstractionInformation;

template<storm::dd::DdType DdType>
class ExpressionTranslator : public storm::expressions::ExpressionVisitor {
   public:
    ExpressionTranslator(AbstractionInformation<DdType>& abstractionInformation, std::unique_ptr<storm::solver::SmtSolver>&& smtSolver);

    storm::dd::Bdd<DdType> translate(storm::expressions::Expression const& expression);

    virtual boost::any visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::VariableExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::BooleanLiteralExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::IntegerLiteralExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::RationalLiteralExpression const& expression, boost::any const& data);

   private:
    std::reference_wrapper<AbstractionInformation<DdType>> abstractionInformation;
    storm::expressions::EquivalenceChecker equivalenceChecker;

    std::set<storm::expressions::Variable> locationVariables;
    std::set<storm::expressions::Variable> abstractedVariables;
};

}  // namespace abstraction
}  // namespace storm::gbar
