#include "storm/logic/ToExpressionVisitor.h"
#include <boost/any.hpp>

#include "storm/logic/Formulas.h"

#include "storm/storage/expressions/ExpressionManager.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {

storm::expressions::Expression ToExpressionVisitor::toExpression(Formula const& f, storm::expressions::ExpressionManager const& manager) const {
    boost::any result = f.accept(*this, std::ref(manager));
    return boost::any_cast<storm::expressions::Expression>(result);
}

boost::any ToExpressionVisitor::visit(AtomicExpressionFormula const& f, boost::any const&) const {
    return f.getExpression();
}

boost::any ToExpressionVisitor::visit(AtomicLabelFormula const& f, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException,
                    "Cannot assemble expression, because the undefined atomic label '" << f.getLabel() << "' appears in the formula.");
}

boost::any ToExpressionVisitor::visit(BinaryBooleanStateFormula const& f, boost::any const& data) const {
    storm::expressions::Expression left = boost::any_cast<storm::expressions::Expression>(f.getLeftSubformula().accept(*this, data));
    storm::expressions::Expression right = boost::any_cast<storm::expressions::Expression>(f.getRightSubformula().accept(*this, data));
    switch (f.getOperator()) {
        case BinaryBooleanStateFormula::OperatorType::And:
            return left && right;
        case BinaryBooleanStateFormula::OperatorType::Or:
            return left || right;
    }
    return boost::any();
}

boost::any ToExpressionVisitor::visit(BinaryBooleanPathFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(BooleanLiteralFormula const& f, boost::any const& data) const {
    storm::expressions::Expression result;
    if (f.isTrueFormula()) {
        result = boost::any_cast<std::reference_wrapper<storm::expressions::ExpressionManager const>>(data).get().boolean(true);
    } else {
        result = boost::any_cast<std::reference_wrapper<storm::expressions::ExpressionManager const>>(data).get().boolean(false);
    }
    return result;
}

boost::any ToExpressionVisitor::visit(BoundedUntilFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(ConditionalFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(CumulativeRewardFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(EventuallyFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(TimeOperatorFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(GloballyFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(GameFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(InstantaneousRewardFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(LongRunAverageOperatorFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(LongRunAverageRewardFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(MultiObjectiveFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(QuantileFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(NextFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(ProbabilityOperatorFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(RewardOperatorFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(TotalRewardFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(UnaryBooleanStateFormula const& f, boost::any const& data) const {
    storm::expressions::Expression subexpression = boost::any_cast<storm::expressions::Expression>(f.getSubformula().accept(*this, data));
    switch (f.getOperator()) {
        case UnaryBooleanStateFormula::OperatorType::Not:
            return !subexpression;
    }
    return boost::any();
}

boost::any ToExpressionVisitor::visit(UnaryBooleanPathFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(UntilFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(HOAPathFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(DiscountedCumulativeRewardFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}

boost::any ToExpressionVisitor::visit(DiscountedTotalRewardFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot assemble expression from formula that contains illegal elements.");
}
}  // namespace logic
}  // namespace storm
