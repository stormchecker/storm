#include "storm/logic/LiftableTransitionRewardsVisitor.h"
#include <boost/any.hpp>

#include "storm/logic/Formulas.h"
#include "storm/storage/jani/traverser/RewardModelInformation.h"

namespace storm {
namespace logic {

LiftableTransitionRewardsVisitor::LiftableTransitionRewardsVisitor(storm::storage::SymbolicModelDescription const& symbolicModelDescription)
    : symbolicModelDescription(symbolicModelDescription) {
    // Intentionally left empty.
}

bool LiftableTransitionRewardsVisitor::areTransitionRewardsLiftable(Formula const& f) const {
    return boost::any_cast<bool>(f.accept(*this, boost::any()));
}

boost::any LiftableTransitionRewardsVisitor::visit(AtomicExpressionFormula const&, boost::any const&) const {
    return true;
}

boost::any LiftableTransitionRewardsVisitor::visit(AtomicLabelFormula const&, boost::any const&) const {
    return true;
}

boost::any LiftableTransitionRewardsVisitor::visit(BinaryBooleanStateFormula const& f, boost::any const& data) const {
    return boost::any_cast<bool>(f.getLeftSubformula().accept(*this, data)) && boost::any_cast<bool>(f.getRightSubformula().accept(*this, data));
}

boost::any LiftableTransitionRewardsVisitor::visit(BinaryBooleanPathFormula const& f, boost::any const& data) const {
    return boost::any_cast<bool>(f.getLeftSubformula().accept(*this, data)) && boost::any_cast<bool>(f.getRightSubformula().accept(*this, data));
}

boost::any LiftableTransitionRewardsVisitor::visit(BooleanLiteralFormula const&, boost::any const&) const {
    return true;
}

boost::any LiftableTransitionRewardsVisitor::visit(BoundedUntilFormula const& f, boost::any const& data) const {
    for (unsigned i = 0; i < f.getDimension(); ++i) {
        if (f.getTimeBoundReference(i).isRewardBound() && rewardModelHasTransitionRewards(f.getTimeBoundReference(i).getRewardName())) {
            return false;
        }
    }

    bool result = true;
    if (f.hasMultiDimensionalSubformulas()) {
        for (unsigned i = 0; i < f.getDimension(); ++i) {
            result = result && boost::any_cast<bool>(f.getLeftSubformula(i).accept(*this, data));
            result = result && boost::any_cast<bool>(f.getRightSubformula(i).accept(*this, data));
        }
    } else {
        result = result && boost::any_cast<bool>(f.getLeftSubformula().accept(*this, data));
        result = result && boost::any_cast<bool>(f.getRightSubformula().accept(*this, data));
    }
    return result;
}

boost::any LiftableTransitionRewardsVisitor::visit(ConditionalFormula const& f, boost::any const& data) const {
    return !f.isConditionalRewardFormula() && boost::any_cast<bool>(f.getSubformula().accept(*this, data)) &&
           boost::any_cast<bool>(f.getConditionFormula().accept(*this, data));
}

boost::any LiftableTransitionRewardsVisitor::visit(CumulativeRewardFormula const& f, boost::any const&) const {
    for (unsigned i = 0; i < f.getDimension(); ++i) {
        if (f.getTimeBoundReference(i).isRewardBound() && rewardModelHasTransitionRewards(f.getTimeBoundReference(i).getRewardName())) {
            return false;
        }
    }
    return true;
}

boost::any LiftableTransitionRewardsVisitor::visit(EventuallyFormula const& f, boost::any const& data) const {
    return f.getSubformula().accept(*this, data);
}

boost::any LiftableTransitionRewardsVisitor::visit(TimeOperatorFormula const& f, boost::any const& data) const {
    return f.getSubformula().accept(*this, data);
}

boost::any LiftableTransitionRewardsVisitor::visit(GloballyFormula const& f, boost::any const& data) const {
    return f.getSubformula().accept(*this, data);
}

boost::any LiftableTransitionRewardsVisitor::visit(GameFormula const& f, boost::any const& data) const {
    STORM_LOG_WARN("Transitionbranch-based rewards might be reduced to action-based rewards. Be sure that this is correct for your property.");
    // TODO: Check if this is correct
    return f.getSubformula().accept(*this, data);
}

boost::any LiftableTransitionRewardsVisitor::visit(InstantaneousRewardFormula const&, boost::any const&) const {
    return true;
}

boost::any LiftableTransitionRewardsVisitor::visit(LongRunAverageOperatorFormula const& f, boost::any const& data) const {
    return f.getSubformula().accept(*this, data);
}

boost::any LiftableTransitionRewardsVisitor::visit(LongRunAverageRewardFormula const&, boost::any const&) const {
    return true;
}

boost::any LiftableTransitionRewardsVisitor::visit(MultiObjectiveFormula const& f, boost::any const& data) const {
    bool result = true;
    for (auto const& subF : f.getSubformulas()) {
        result = result && boost::any_cast<bool>(subF->accept(*this, data));
    }
    return result;
}

boost::any LiftableTransitionRewardsVisitor::visit(QuantileFormula const& f, boost::any const& data) const {
    return f.getSubformula().accept(*this, data);
}

boost::any LiftableTransitionRewardsVisitor::visit(NextFormula const& f, boost::any const& data) const {
    return boost::any_cast<bool>(f.getSubformula().accept(*this, data));
}

boost::any LiftableTransitionRewardsVisitor::visit(ProbabilityOperatorFormula const& f, boost::any const& data) const {
    return f.getSubformula().accept(*this, data);
}

boost::any LiftableTransitionRewardsVisitor::visit(RewardOperatorFormula const& f, boost::any const& data) const {
    return boost::any_cast<bool>(f.getSubformula().accept(*this, data));
}

boost::any LiftableTransitionRewardsVisitor::visit(TotalRewardFormula const&, boost::any const&) const {
    return true;
}

boost::any LiftableTransitionRewardsVisitor::visit(UnaryBooleanStateFormula const& f, boost::any const& data) const {
    return f.getSubformula().accept(*this, data);
}

boost::any LiftableTransitionRewardsVisitor::visit(UnaryBooleanPathFormula const& f, boost::any const& data) const {
    return f.getSubformula().accept(*this, data);
}

boost::any LiftableTransitionRewardsVisitor::visit(UntilFormula const& f, boost::any const& data) const {
    return boost::any_cast<bool>(f.getLeftSubformula().accept(*this, data)) && boost::any_cast<bool>(f.getRightSubformula().accept(*this));
}

boost::any LiftableTransitionRewardsVisitor::visit(HOAPathFormula const& f, boost::any const& data) const {
    for (auto const& ap : f.getAPMapping()) {
        if (!boost::any_cast<bool>(ap.second->accept(*this, data))) {
            return false;
        }
    }
    return true;
}

boost::any LiftableTransitionRewardsVisitor::visit(DiscountedCumulativeRewardFormula const& f, boost::any const&) const {
    for (unsigned i = 0; i < f.getDimension(); ++i) {
        if (f.getTimeBoundReference(i).isRewardBound() && rewardModelHasTransitionRewards(f.getTimeBoundReference(i).getRewardName())) {
            return false;
        }
    }
    return true;
}

boost::any LiftableTransitionRewardsVisitor::visit(DiscountedTotalRewardFormula const&, boost::any const&) const {
    return true;
}

bool LiftableTransitionRewardsVisitor::rewardModelHasTransitionRewards(std::string const& rewardModelName) const {
    if (symbolicModelDescription.hasModel()) {
        if (symbolicModelDescription.isJaniModel()) {
            return storm::jani::RewardModelInformation(symbolicModelDescription.asJaniModel(), rewardModelName).hasTransitionRewards();
        } else if (symbolicModelDescription.isPrismProgram()) {
            return symbolicModelDescription.asPrismProgram().getRewardModel(rewardModelName).hasTransitionRewards();
        }
    }
    return false;
}
}  // namespace logic
}  // namespace storm
