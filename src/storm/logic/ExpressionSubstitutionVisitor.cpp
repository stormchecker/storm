#include "storm/logic/ExpressionSubstitutionVisitor.h"
#include <boost/any.hpp>

#include "storm/logic/Formulas.h"

namespace storm {
namespace logic {

std::shared_ptr<Formula> ExpressionSubstitutionVisitor::substitute(
    Formula const& f, std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const& substitutionFunction) const {
    boost::any result = f.accept(*this, &substitutionFunction);
    return boost::any_cast<std::shared_ptr<Formula>>(result);
}

OperatorInformation substituteOperatorInformation(
    OperatorInformation const& operatorInformation,
    std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const& substitutionFunction) {
    boost::optional<Bound> bound;
    if (operatorInformation.bound) {
        bound = Bound(operatorInformation.bound->comparisonType, substitutionFunction(operatorInformation.bound->threshold));
    }
    return OperatorInformation(operatorInformation.optimalityType, bound);
}

boost::any ExpressionSubstitutionVisitor::visit(TimeOperatorFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    auto const& substitutionFunction = *boost::any_cast<std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const*>(data);
    return std::static_pointer_cast<Formula>(
        std::make_shared<TimeOperatorFormula>(subformula, substituteOperatorInformation(f.getOperatorInformation(), substitutionFunction)));
}

boost::any ExpressionSubstitutionVisitor::visit(LongRunAverageOperatorFormula const& f, boost::any const& data) const {
    auto const& substitutionFunction = *boost::any_cast<std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const*>(data);
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(
        std::make_shared<LongRunAverageOperatorFormula>(subformula, substituteOperatorInformation(f.getOperatorInformation(), substitutionFunction)));
}

boost::any ExpressionSubstitutionVisitor::visit(ProbabilityOperatorFormula const& f, boost::any const& data) const {
    auto const& substitutionFunction = *boost::any_cast<std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const*>(data);
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(
        std::make_shared<ProbabilityOperatorFormula>(subformula, substituteOperatorInformation(f.getOperatorInformation(), substitutionFunction)));
}

boost::any ExpressionSubstitutionVisitor::visit(RewardOperatorFormula const& f, boost::any const& data) const {
    auto const& substitutionFunction = *boost::any_cast<std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const*>(data);
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<RewardOperatorFormula>(
        subformula, f.getOptionalRewardModelName(), substituteOperatorInformation(f.getOperatorInformation(), substitutionFunction)));
}

boost::any ExpressionSubstitutionVisitor::visit(BoundedUntilFormula const& f, boost::any const& data) const {
    auto const& substitutionFunction = *boost::any_cast<std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const*>(data);
    std::vector<boost::optional<TimeBound>> lowerBounds, upperBounds;
    std::vector<TimeBoundReference> timeBoundReferences;
    for (uint64_t i = 0; i < f.getDimension(); ++i) {
        if (f.hasLowerBound(i)) {
            lowerBounds.emplace_back(TimeBound(f.isLowerBoundStrict(i), substitutionFunction(f.getLowerBound(i))));
        } else {
            lowerBounds.emplace_back();
        }
        if (f.hasUpperBound(i)) {
            upperBounds.emplace_back(TimeBound(f.isUpperBoundStrict(i), substitutionFunction(f.getUpperBound(i))));
        } else {
            upperBounds.emplace_back();
        }
        timeBoundReferences.push_back(f.getTimeBoundReference(i));
    }
    if (f.hasMultiDimensionalSubformulas()) {
        std::vector<std::shared_ptr<Formula const>> leftSubformulas, rightSubformulas;
        for (uint64_t i = 0; i < f.getDimension(); ++i) {
            leftSubformulas.push_back(boost::any_cast<std::shared_ptr<Formula>>(f.getLeftSubformula(i).accept(*this, data)));
            rightSubformulas.push_back(boost::any_cast<std::shared_ptr<Formula>>(f.getRightSubformula(i).accept(*this, data)));
        }
        return std::static_pointer_cast<Formula>(
            std::make_shared<BoundedUntilFormula>(leftSubformulas, rightSubformulas, lowerBounds, upperBounds, timeBoundReferences));
    } else {
        std::shared_ptr<Formula> left = boost::any_cast<std::shared_ptr<Formula>>(f.getLeftSubformula().accept(*this, data));
        std::shared_ptr<Formula> right = boost::any_cast<std::shared_ptr<Formula>>(f.getRightSubformula().accept(*this, data));
        return std::static_pointer_cast<Formula>(std::make_shared<BoundedUntilFormula>(left, right, lowerBounds, upperBounds, timeBoundReferences));
    }
}

boost::any ExpressionSubstitutionVisitor::visit(CumulativeRewardFormula const& f, boost::any const& data) const {
    auto const& substitutionFunction = *boost::any_cast<std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const*>(data);
    std::vector<TimeBound> bounds;
    std::vector<TimeBoundReference> timeBoundReferences;
    for (uint64_t i = 0; i < f.getDimension(); ++i) {
        bounds.emplace_back(TimeBound(f.isBoundStrict(i), substitutionFunction(f.getBound(i))));
        timeBoundReferences.push_back(f.getTimeBoundReference(i));
    }
    boost::optional<RewardAccumulation> optionalRewardAccumulation;
    if (f.hasRewardAccumulation()) {
        optionalRewardAccumulation = f.getRewardAccumulation();
    }
    return std::static_pointer_cast<Formula>(std::make_shared<CumulativeRewardFormula>(bounds, timeBoundReferences, optionalRewardAccumulation));
}

boost::any ExpressionSubstitutionVisitor::visit(DiscountedCumulativeRewardFormula const& f, boost::any const& data) const {
    auto const& substitutionFunction = *boost::any_cast<std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const*>(data);
    std::vector<TimeBound> bounds;
    std::vector<TimeBoundReference> timeBoundReferences;
    for (uint64_t i = 0; i < f.getDimension(); ++i) {
        bounds.emplace_back(TimeBound(f.isBoundStrict(i), substitutionFunction(f.getBound(i))));
        timeBoundReferences.push_back(f.getTimeBoundReference(i));
    }
    boost::optional<RewardAccumulation> optionalRewardAccumulation;
    if (f.hasRewardAccumulation()) {
        optionalRewardAccumulation = f.getRewardAccumulation();
    }
    return std::static_pointer_cast<Formula>(std::make_shared<DiscountedCumulativeRewardFormula>(substitutionFunction(f.getDiscountFactor()), bounds,
                                                                                                 timeBoundReferences, optionalRewardAccumulation));
}

boost::any ExpressionSubstitutionVisitor::visit(DiscountedTotalRewardFormula const& f, boost::any const& data) const {
    auto const& substitutionFunction = *boost::any_cast<std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const*>(data);
    boost::optional<RewardAccumulation> optionalRewardAccumulation;
    if (f.hasRewardAccumulation()) {
        optionalRewardAccumulation = f.getRewardAccumulation();
    }
    return std::static_pointer_cast<Formula>(
        std::make_shared<DiscountedTotalRewardFormula>(substitutionFunction(f.getDiscountFactor()), optionalRewardAccumulation));
}

boost::any ExpressionSubstitutionVisitor::visit(InstantaneousRewardFormula const& f, boost::any const& data) const {
    auto const& substitutionFunction = *boost::any_cast<std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const*>(data);
    return std::static_pointer_cast<Formula>(std::make_shared<InstantaneousRewardFormula>(substitutionFunction(f.getBound()), f.getTimeBoundType()));
}

boost::any ExpressionSubstitutionVisitor::visit(AtomicExpressionFormula const& f, boost::any const& data) const {
    auto const& substitutionFunction = *boost::any_cast<std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const*>(data);
    return std::static_pointer_cast<Formula>(std::make_shared<AtomicExpressionFormula>(substitutionFunction(f.getExpression())));
}

}  // namespace logic
}  // namespace storm
