#include "storm/logic/CloneVisitor.h"
#include <boost/any.hpp>

#include "storm/logic/Formulas.h"

namespace storm {
namespace logic {

std::shared_ptr<Formula> CloneVisitor::clone(Formula const& f) const {
    boost::any result = f.accept(*this, boost::any());
    return boost::any_cast<std::shared_ptr<Formula>>(result);
}

boost::any CloneVisitor::visit(AtomicExpressionFormula const& f, boost::any const&) const {
    return std::static_pointer_cast<Formula>(std::make_shared<AtomicExpressionFormula>(f));
}

boost::any CloneVisitor::visit(AtomicLabelFormula const& f, boost::any const&) const {
    return std::static_pointer_cast<Formula>(std::make_shared<AtomicLabelFormula>(f));
}

boost::any CloneVisitor::visit(BinaryBooleanStateFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> left = boost::any_cast<std::shared_ptr<Formula>>(f.getLeftSubformula().accept(*this, data));
    std::shared_ptr<Formula> right = boost::any_cast<std::shared_ptr<Formula>>(f.getRightSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<BinaryBooleanStateFormula>(f.getOperator(), left, right));
}

boost::any CloneVisitor::visit(BinaryBooleanPathFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> left = boost::any_cast<std::shared_ptr<Formula>>(f.getLeftSubformula().accept(*this, data));
    std::shared_ptr<Formula> right = boost::any_cast<std::shared_ptr<Formula>>(f.getRightSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<BinaryBooleanPathFormula>(f.getOperator(), left, right));
}

boost::any CloneVisitor::visit(BooleanLiteralFormula const& f, boost::any const&) const {
    return std::static_pointer_cast<Formula>(std::make_shared<BooleanLiteralFormula>(f));
}

boost::any CloneVisitor::visit(BoundedUntilFormula const& f, boost::any const& data) const {
    std::vector<boost::optional<TimeBound>> lowerBounds, upperBounds;
    std::vector<TimeBoundReference> timeBoundReferences;
    for (uint64_t i = 0; i < f.getDimension(); ++i) {
        if (f.hasLowerBound(i)) {
            lowerBounds.emplace_back(TimeBound(f.isLowerBoundStrict(i), f.getLowerBound(i)));
        } else {
            lowerBounds.emplace_back();
        }
        if (f.hasUpperBound(i)) {
            upperBounds.emplace_back(TimeBound(f.isUpperBoundStrict(i), f.getUpperBound(i)));
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

boost::any CloneVisitor::visit(ConditionalFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    std::shared_ptr<Formula> conditionFormula = boost::any_cast<std::shared_ptr<Formula>>(f.getConditionFormula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<ConditionalFormula>(subformula, conditionFormula, f.getContext()));
}

boost::any CloneVisitor::visit(CumulativeRewardFormula const& f, boost::any const&) const {
    return std::static_pointer_cast<Formula>(std::make_shared<CumulativeRewardFormula>(f));
}

boost::any CloneVisitor::visit(EventuallyFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    if (f.hasRewardAccumulation()) {
        return std::static_pointer_cast<Formula>(std::make_shared<EventuallyFormula>(subformula, f.getContext(), f.getRewardAccumulation()));
    } else {
        return std::static_pointer_cast<Formula>(std::make_shared<EventuallyFormula>(subformula, f.getContext()));
    }
}

boost::any CloneVisitor::visit(TimeOperatorFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<TimeOperatorFormula>(subformula, f.getOperatorInformation()));
}

boost::any CloneVisitor::visit(GloballyFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<GloballyFormula>(subformula));
}

boost::any CloneVisitor::visit(GameFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<GameFormula>(f.getCoalition(), subformula));
}

boost::any CloneVisitor::visit(InstantaneousRewardFormula const& f, boost::any const&) const {
    return std::static_pointer_cast<Formula>(std::make_shared<InstantaneousRewardFormula>(f));
}

boost::any CloneVisitor::visit(LongRunAverageOperatorFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<LongRunAverageOperatorFormula>(subformula, f.getOperatorInformation()));
}

boost::any CloneVisitor::visit(LongRunAverageRewardFormula const& f, boost::any const&) const {
    return std::static_pointer_cast<Formula>(std::make_shared<LongRunAverageRewardFormula>(f));
}

boost::any CloneVisitor::visit(MultiObjectiveFormula const& f, boost::any const& data) const {
    std::vector<std::shared_ptr<Formula const>> subformulas;
    for (auto const& subF : f.getSubformulas()) {
        subformulas.push_back(boost::any_cast<std::shared_ptr<Formula>>(subF->accept(*this, data)));
    }
    return std::static_pointer_cast<Formula>(std::make_shared<MultiObjectiveFormula>(subformulas, f.getType()));
}

boost::any CloneVisitor::visit(QuantileFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<QuantileFormula>(f.getBoundVariables(), subformula));
}

boost::any CloneVisitor::visit(NextFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<NextFormula>(subformula));
}

boost::any CloneVisitor::visit(ProbabilityOperatorFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<ProbabilityOperatorFormula>(subformula, f.getOperatorInformation()));
}

boost::any CloneVisitor::visit(RewardOperatorFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<RewardOperatorFormula>(subformula, f.getOptionalRewardModelName(), f.getOperatorInformation()));
}

boost::any CloneVisitor::visit(TotalRewardFormula const& f, boost::any const&) const {
    return std::static_pointer_cast<Formula>(std::make_shared<TotalRewardFormula>(f));
}

boost::any CloneVisitor::visit(UnaryBooleanStateFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<UnaryBooleanStateFormula>(f.getOperator(), subformula));
}

boost::any CloneVisitor::visit(UnaryBooleanPathFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<UnaryBooleanPathFormula>(f.getOperator(), subformula));
}

boost::any CloneVisitor::visit(UntilFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> left = boost::any_cast<std::shared_ptr<Formula>>(f.getLeftSubformula().accept(*this, data));
    std::shared_ptr<Formula> right = boost::any_cast<std::shared_ptr<Formula>>(f.getRightSubformula().accept(*this, data));
    return std::static_pointer_cast<Formula>(std::make_shared<UntilFormula>(left, right));
}

boost::any CloneVisitor::visit(HOAPathFormula const& f, boost::any const& data) const {
    std::shared_ptr<HOAPathFormula> result = std::make_shared<HOAPathFormula>(f.getAutomatonFile());
    for (auto& mapped : f.getAPMapping()) {
        std::shared_ptr<Formula> clonedExpression = boost::any_cast<std::shared_ptr<Formula>>(mapped.second->accept(*this, data));
        result->addAPMapping(mapped.first, clonedExpression);
    }
    return std::static_pointer_cast<Formula>(result);
}

boost::any CloneVisitor::visit(DiscountedCumulativeRewardFormula const& f, boost::any const&) const {
    return std::static_pointer_cast<Formula>(std::make_shared<DiscountedCumulativeRewardFormula>(f));
}

boost::any CloneVisitor::visit(DiscountedTotalRewardFormula const& f, boost::any const&) const {
    return std::static_pointer_cast<Formula>(std::make_shared<DiscountedTotalRewardFormula>(f));
}
}  // namespace logic
}  // namespace storm
