#include "storm/storage/jani/traverser/FunctionCallExpressionFinder.h"

#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/expressions/JaniExpressions.h"
#include "storm/storage/jani/traverser/JaniTraverser.h"
#include "storm/storage/jani/visitor/JaniExpressionVisitor.h"

namespace storm {
namespace jani {

namespace detail {
class FunctionCallExpressionFinderExpressionVisitor : public storm::expressions::ExpressionVisitor, public storm::expressions::JaniExpressionVisitor {
   public:
    using storm::expressions::ExpressionVisitor::visit;
    virtual boost::any visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) override {
        expression.getCondition()->accept(*this, data);
        expression.getThenExpression()->accept(*this, data);
        expression.getElseExpression()->accept(*this, data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) override {
        expression.getFirstOperand()->accept(*this, data);
        expression.getSecondOperand()->accept(*this, data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) override {
        expression.getFirstOperand()->accept(*this, data);
        expression.getSecondOperand()->accept(*this, data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) override {
        expression.getFirstOperand()->accept(*this, data);
        expression.getSecondOperand()->accept(*this, data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::VariableExpression const&, boost::any const&) override {
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) override {
        expression.getOperand()->accept(*this, data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) override {
        expression.getOperand()->accept(*this, data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::BooleanLiteralExpression const&, boost::any const&) override {
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::IntegerLiteralExpression const&, boost::any const&) override {
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::RationalLiteralExpression const&, boost::any const&) override {
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::ValueArrayExpression const& expression, boost::any const& data) override {
        STORM_LOG_ASSERT(expression.size()->isIntegerLiteralExpression(),
                         "Unexpected kind of size expression of ValueArrayExpression (" << expression.size()->toExpression() << ").");
        uint64_t size = expression.size()->evaluateAsInt();
        for (uint64_t i = 0; i < size; ++i) {
            expression.at(i)->accept(*this, data);
        }
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::ConstructorArrayExpression const& expression, boost::any const& data) override {
        expression.getElementExpression()->accept(*this, data);
        expression.size()->accept(*this, data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::ArrayAccessExpression const& expression, boost::any const& data) override {
        expression.getFirstOperand()->accept(*this, data);
        expression.getSecondOperand()->accept(*this, data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::FunctionCallExpression const& expression, boost::any const& data) override {
        auto& set = *boost::any_cast<std::unordered_set<std::string>*>(data);
        set.insert(expression.getFunctionIdentifier());
        for (uint64_t i = 0; i < expression.getNumberOfArguments(); ++i) {
            expression.getArgument(i)->accept(*this, data);
        }
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::TranscendentalNumberLiteralExpression const&, boost::any const&) override {
        return boost::any();
    }
};

class FunctionCallExpressionFinderTraverser : public ConstJaniTraverser {
   public:
    virtual void traverse(Model const& model, boost::any const& data) override {
        ConstJaniTraverser::traverse(model, data);
    }

    virtual void traverse(storm::expressions::Expression const& expression, boost::any const& data) override {
        auto& res = *boost::any_cast<bool*>(data);
        res = res || !getOccurringFunctionCalls(expression).empty();
    }
};
}  // namespace detail

bool containsFunctionCallExpression(Model const& model) {
    bool result = false;
    detail::FunctionCallExpressionFinderTraverser().traverse(model, &result);
    return result;
}

std::unordered_set<std::string> getOccurringFunctionCalls(storm::expressions::Expression const& expression) {
    detail::FunctionCallExpressionFinderExpressionVisitor v;
    std::unordered_set<std::string> result;
    expression.accept(v, &result);
    return result;
}
}  // namespace jani
}  // namespace storm
