#include "storm/logic/RewardModelNameSubstitutionVisitor.h"
#include <boost/any.hpp>
#include "storm/logic/Formulas.h"

#include "storm/storage/jani/Model.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {
namespace logic {

RewardModelNameSubstitutionVisitor::RewardModelNameSubstitutionVisitor(std::map<std::string, std::string> const& rewardModelNameMapping)
    : rewardModelNameMapping(rewardModelNameMapping) {
    // Intentionally left empty
}

std::shared_ptr<Formula> RewardModelNameSubstitutionVisitor::substitute(Formula const& f) const {
    boost::any result = f.accept(*this, boost::any());
    return boost::any_cast<std::shared_ptr<Formula>>(result);
}

boost::any RewardModelNameSubstitutionVisitor::visit(BoundedUntilFormula const& f, boost::any const& data) const {
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
        auto const& tbr = f.getTimeBoundReference(i);
        if (tbr.isRewardBound()) {
            timeBoundReferences.emplace_back(getNewName(tbr.getRewardName()), tbr.getOptionalRewardAccumulation());
        } else {
            timeBoundReferences.push_back(tbr);
        }
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

boost::any RewardModelNameSubstitutionVisitor::visit(CumulativeRewardFormula const& f, boost::any const&) const {
    // Data is unused; no children to pass this on.
    std::vector<TimeBound> bounds;
    std::vector<TimeBoundReference> timeBoundReferences;
    for (uint64_t i = 0; i < f.getDimension(); ++i) {
        bounds.emplace_back(TimeBound(f.isBoundStrict(i), f.getBound(i)));
        storm::logic::TimeBoundReference tbr = f.getTimeBoundReference(i);
        if (tbr.isRewardBound()) {
            tbr = storm::logic::TimeBoundReference(getNewName(tbr.getRewardName()), tbr.getOptionalRewardAccumulation());
        }
        timeBoundReferences.push_back(std::move(tbr));
    }
    if (f.hasRewardAccumulation()) {
        return std::static_pointer_cast<Formula>(std::make_shared<CumulativeRewardFormula>(bounds, timeBoundReferences, f.getRewardAccumulation()));
    } else {
        return std::static_pointer_cast<Formula>(std::make_shared<CumulativeRewardFormula>(bounds, timeBoundReferences));
    }
}

boost::any RewardModelNameSubstitutionVisitor::visit(RewardOperatorFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    if (f.hasRewardModelName()) {
        return std::static_pointer_cast<Formula>(
            std::make_shared<RewardOperatorFormula>(subformula, getNewName(f.getRewardModelName()), f.getOperatorInformation()));
    } else {
        return std::static_pointer_cast<Formula>(std::make_shared<RewardOperatorFormula>(subformula, boost::none, f.getOperatorInformation()));
    }
}

std::string const& RewardModelNameSubstitutionVisitor::getNewName(std::string const& oldName) const {
    auto nameIt = rewardModelNameMapping.find(oldName);
    if (nameIt == rewardModelNameMapping.end()) {
        return oldName;
    } else {
        return nameIt->second;
    }
}

boost::any RewardModelNameSubstitutionVisitor::visit(DiscountedCumulativeRewardFormula const& f, boost::any const&) const {
    // Data is unused; no children to pass this on.
    std::vector<TimeBound> bounds;
    std::vector<TimeBoundReference> timeBoundReferences;
    for (uint64_t i = 0; i < f.getDimension(); ++i) {
        bounds.emplace_back(TimeBound(f.isBoundStrict(i), f.getBound(i)));
        storm::logic::TimeBoundReference tbr = f.getTimeBoundReference(i);
        if (tbr.isRewardBound()) {
            tbr = storm::logic::TimeBoundReference(getNewName(tbr.getRewardName()), tbr.getOptionalRewardAccumulation());
        }
        timeBoundReferences.push_back(std::move(tbr));
    }
    if (f.hasRewardAccumulation()) {
        return std::static_pointer_cast<Formula>(
            std::make_shared<DiscountedCumulativeRewardFormula>(f.getDiscountFactor(), bounds, timeBoundReferences, f.getRewardAccumulation()));
    } else {
        return std::static_pointer_cast<Formula>(std::make_shared<DiscountedCumulativeRewardFormula>(f.getDiscountFactor(), bounds, timeBoundReferences));
    }
}

}  // namespace logic
}  // namespace storm
