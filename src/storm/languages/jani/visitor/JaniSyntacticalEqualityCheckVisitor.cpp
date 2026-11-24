#include "JaniSyntacticalEqualityCheckVisitor.h"

#include "storm/exceptions/InvalidArgumentException.h"

namespace storm {

namespace expressions {

JaniSyntacticalEqualityCheckVisitor::JaniSyntacticalEqualityCheckVisitor() : SyntacticalEqualityCheckVisitor() {
    // Intentionally left empty.
}

boost::any JaniSyntacticalEqualityCheckVisitor::visit(ValueArrayExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    auto const rhs = std::dynamic_pointer_cast<storm::expressions::ValueArrayExpression const>(otherBaseExpression.getSharedPointer());
    if (rhs) {
        STORM_LOG_ASSERT(!expression.size()->containsVariables(), "Non-const size of value array expr.");
        STORM_LOG_ASSERT(!rhs->size()->containsVariables(), "Non-const size of value array expr.");
        auto lhsSize = expression.size()->evaluateAsInt();
        auto rhsSize = rhs->size()->evaluateAsInt();
        if (lhsSize != rhsSize) {
            return false;
        }
        for (int64_t i = 0; i < lhsSize; ++i) {
            if (!boost::any_cast<bool>(expression.at(i)->accept(*this, std::ref(*rhs->at(i))))) {
                return false;
            }
        }
        return true;
    } else {
        return false;
    }
}

boost::any JaniSyntacticalEqualityCheckVisitor::visit(ConstructorArrayExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    auto const rhs = std::dynamic_pointer_cast<storm::expressions::ConstructorArrayExpression const>(otherBaseExpression.getSharedPointer());
    if (rhs) {
        return boost::any_cast<bool>(expression.size()->accept(*this, std::ref(*rhs->size()))) &&
               boost::any_cast<bool>(expression.getElementExpression()->accept(*this, std::ref(*rhs->getElementExpression()))) &&
               (expression.getIndexVar() == rhs->getIndexVar());
    } else {
        return false;
    }
}

boost::any JaniSyntacticalEqualityCheckVisitor::visit(ArrayAccessExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    auto const rhs = std::dynamic_pointer_cast<storm::expressions::ArrayAccessExpression const>(otherBaseExpression.getSharedPointer());
    if (rhs) {
        return boost::any_cast<bool>(expression.getFirstOperand()->accept(*this, std::ref(*rhs->getFirstOperand()))) &&
               boost::any_cast<bool>(expression.getSecondOperand()->accept(*this, std::ref(*rhs->getSecondOperand())));
    } else {
        return false;
    }
}

boost::any JaniSyntacticalEqualityCheckVisitor::visit(FunctionCallExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    auto const rhs = std::dynamic_pointer_cast<storm::expressions::FunctionCallExpression const>(otherBaseExpression.getSharedPointer());
    if (rhs) {
        if (expression.getFunctionIdentifier() != rhs->getFunctionIdentifier() || expression.getNumberOfArguments() != rhs->getNumberOfArguments()) {
            return false;
        }
        for (uint64_t i = 0; i < expression.getNumberOfArguments(); ++i) {
            if (!boost::any_cast<bool>(expression.getArgument(i)->accept(*this, std::ref(*rhs->getArgument(i))))) {
                return false;
            }
        }
        return true;
    } else {
        return false;
    }
}

boost::any JaniSyntacticalEqualityCheckVisitor::visit(TranscendentalNumberLiteralExpression const& expression, boost::any const& data) {
    BaseExpression const& otherBaseExpression = boost::any_cast<std::reference_wrapper<BaseExpression const>>(data).get();
    auto const rhs = std::dynamic_pointer_cast<storm::expressions::TranscendentalNumberLiteralExpression const>(otherBaseExpression.getSharedPointer());
    if (rhs) {
        return expression.getTranscendentalNumber() == rhs->getTranscendentalNumber();
    } else {
        return false;
    }
}

}  // namespace expressions
}  // namespace storm
