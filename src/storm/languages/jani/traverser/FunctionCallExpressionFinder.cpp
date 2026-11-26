#include "storm/languages/jani/traverser/FunctionCallExpressionFinder.h"

#include "storm/languages/jani/Model.h"
#include "storm/languages/jani/expressions/JaniExpressions.h"
#include "storm/languages/jani/traverser/JaniTraverser.h"
#include "storm/languages/jani/visitor/JaniExpressionVisitor.h"

namespace storm {
namespace jani {

namespace detail {
class FunctionCallExpressionFinderExpressionVisitor : public storm::expressions::ExpressionVisitor, public storm::expressions::JaniExpressionVisitor {
   public:
    using storm::expressions::ExpressionVisitor::visit;

    void find(storm::expressions::BaseExpression const& expression, boost::any const& data) {
        if (visited.insert(&expression).second) {
            expression.accept(*this, data);
        }
    }

    virtual boost::any visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) override {
        find(*expression.getCondition(), data);
        find(*expression.getThenExpression(), data);
        find(*expression.getElseExpression(), data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) override {
        find(*expression.getFirstOperand(), data);
        find(*expression.getSecondOperand(), data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) override {
        find(*expression.getFirstOperand(), data);
        find(*expression.getSecondOperand(), data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) override {
        find(*expression.getFirstOperand(), data);
        find(*expression.getSecondOperand(), data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::VariableExpression const&, boost::any const&) override {
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) override {
        find(*expression.getOperand(), data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) override {
        find(*expression.getOperand(), data);
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
            find(*expression.at(i), data);
        }
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::ConstructorArrayExpression const& expression, boost::any const& data) override {
        find(*expression.getElementExpression(), data);
        find(*expression.size(), data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::ArrayAccessExpression const& expression, boost::any const& data) override {
        find(*expression.getFirstOperand(), data);
        find(*expression.getSecondOperand(), data);
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::FunctionCallExpression const& expression, boost::any const& data) override {
        auto& set = *boost::any_cast<std::unordered_set<std::string>*>(data);
        set.insert(expression.getFunctionIdentifier());
        for (uint64_t i = 0; i < expression.getNumberOfArguments(); ++i) {
            find(*expression.getArgument(i), data);
        }
        return boost::any();
    }

    virtual boost::any visit(storm::expressions::TranscendentalNumberLiteralExpression const&, boost::any const&) override {
        return boost::any();
    }

   private:
    std::unordered_set<storm::expressions::BaseExpression const*> visited;
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
    v.find(expression.getBaseExpression(), &result);
    return result;
}
}  // namespace jani
}  // namespace storm
