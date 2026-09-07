#include "ArrayExpressionFinder.h"

#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/expressions/JaniExpressions.h"
#include "storm/storage/jani/traverser/JaniTraverser.h"
#include "storm/storage/jani/visitor/JaniExpressionVisitor.h"

namespace storm {
namespace jani {

namespace detail {
class ArrayExpressionFinderExpressionVisitor : public storm::expressions::ExpressionVisitor, public storm::expressions::JaniExpressionVisitor {
   public:
    using storm::expressions::ExpressionVisitor::visit;

    virtual boost::any visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) override {
        return boost::any_cast<bool>(expression.getCondition()->accept(*this, data)) ||
               boost::any_cast<bool>(expression.getThenExpression()->accept(*this, data)) ||
               boost::any_cast<bool>(expression.getElseExpression()->accept(*this, data));
    }

    virtual boost::any visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) override {
        return boost::any_cast<bool>(expression.getFirstOperand()->accept(*this, data)) ||
               boost::any_cast<bool>(expression.getSecondOperand()->accept(*this, data));
    }

    virtual boost::any visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) override {
        return boost::any_cast<bool>(expression.getFirstOperand()->accept(*this, data)) ||
               boost::any_cast<bool>(expression.getSecondOperand()->accept(*this, data));
    }

    virtual boost::any visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) override {
        return boost::any_cast<bool>(expression.getFirstOperand()->accept(*this, data)) ||
               boost::any_cast<bool>(expression.getSecondOperand()->accept(*this, data));
    }

    virtual boost::any visit(storm::expressions::VariableExpression const&, boost::any const&) override {
        return false;
    }

    virtual boost::any visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) override {
        return expression.getOperand()->accept(*this, data);
    }

    virtual boost::any visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) override {
        return expression.getOperand()->accept(*this, data);
    }

    virtual boost::any visit(storm::expressions::BooleanLiteralExpression const&, boost::any const&) override {
        return false;
    }

    virtual boost::any visit(storm::expressions::IntegerLiteralExpression const&, boost::any const&) override {
        return false;
    }

    virtual boost::any visit(storm::expressions::RationalLiteralExpression const&, boost::any const&) override {
        return false;
    }

    virtual boost::any visit(storm::expressions::ValueArrayExpression const&, boost::any const&) override {
        return true;
    }

    virtual boost::any visit(storm::expressions::ConstructorArrayExpression const&, boost::any const&) override {
        return true;
    }

    virtual boost::any visit(storm::expressions::ArrayAccessExpression const&, boost::any const&) override {
        return true;
    }

    virtual boost::any visit(storm::expressions::FunctionCallExpression const& expression, boost::any const& data) override {
        for (uint64_t i = 0; i < expression.getNumberOfArguments(); ++i) {
            if (boost::any_cast<bool>(expression.getArgument(i)->accept(*this, data))) {
                return true;
            }
        }
        return false;
    }

    virtual boost::any visit(storm::expressions::TranscendentalNumberLiteralExpression const&, boost::any const&) override {
        return false;
    }
};

class ArrayExpressionFinderTraverser : public ConstJaniTraverser {
   public:
    virtual void traverse(Model const& model, boost::any const& data) override {
        ConstJaniTraverser::traverse(model, data);
    }

    virtual void traverse(storm::expressions::Expression const& expression, boost::any const& data) override {
        auto& res = *boost::any_cast<bool*>(data);
        res = res || containsArrayExpression(expression);
    }
};
}  // namespace detail

bool containsArrayExpression(Model const& model) {
    bool result = false;
    detail::ArrayExpressionFinderTraverser().traverse(model, &result);
    return result;
}

bool containsArrayExpression(storm::expressions::Expression const& expression) {
    detail::ArrayExpressionFinderExpressionVisitor v;
    return boost::any_cast<bool>(expression.accept(v, boost::any()));
}
}  // namespace jani
}  // namespace storm
