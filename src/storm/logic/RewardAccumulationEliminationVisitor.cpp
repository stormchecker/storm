#include "storm/logic/RewardAccumulationEliminationVisitor.h"
#include <boost/any.hpp>
#include "storm/logic/Formulas.h"

#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/traverser/RewardModelInformation.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {
namespace logic {

RewardAccumulationEliminationVisitor::RewardAccumulationEliminationVisitor(storm::jani::Model const& model) : model(model) {
    // Intentionally left empty
}

std::shared_ptr<Formula> RewardAccumulationEliminationVisitor::eliminateRewardAccumulations(Formula const& f) const {
    boost::any result = f.accept(*this, boost::any());
    return boost::any_cast<std::shared_ptr<Formula>>(result);
}

void RewardAccumulationEliminationVisitor::eliminateRewardAccumulations(std::vector<storm::jani::Property>& properties) const {
    for (auto& p : properties) {
        eliminateRewardAccumulations(p);
    }
}

void RewardAccumulationEliminationVisitor::eliminateRewardAccumulations(storm::jani::Property& property) const {
    auto formula = eliminateRewardAccumulations(*property.getFilter().getFormula());
    auto states = eliminateRewardAccumulations(*property.getFilter().getStatesFormula());
    storm::jani::FilterExpression fe(formula, property.getFilter().getFilterType(), states);
    property = storm::jani::Property(property.getName(), storm::jani::FilterExpression(formula, property.getFilter().getFilterType(), states),
                                     property.getUndefinedConstants(), property.getComment());
}

boost::any RewardAccumulationEliminationVisitor::visit(BoundedUntilFormula const& f, boost::any const& data) const {
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
        storm::logic::TimeBoundReference tbr = f.getTimeBoundReference(i);
        if (tbr.hasRewardAccumulation() && canEliminate(tbr.getRewardAccumulation(), tbr.getRewardName())) {
            // Eliminate accumulation
            tbr = storm::logic::TimeBoundReference(tbr.getRewardName(), boost::none);
        }
        timeBoundReferences.push_back(std::move(tbr));
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

boost::any RewardAccumulationEliminationVisitor::visit(CumulativeRewardFormula const& f, boost::any const& data) const {
    boost::optional<storm::logic::RewardAccumulation> rewAcc;
    STORM_LOG_THROW(!data.empty(), storm::exceptions::UnexpectedException, "Formula " << f << " does not seem to be a subformula of a reward operator.");
    auto rewName = boost::any_cast<boost::optional<std::string>>(data);
    if (f.hasRewardAccumulation() && !canEliminate(f.getRewardAccumulation(), rewName)) {
        rewAcc = f.getRewardAccumulation();
    }

    std::vector<TimeBound> bounds;
    std::vector<TimeBoundReference> timeBoundReferences;
    for (uint64_t i = 0; i < f.getDimension(); ++i) {
        bounds.emplace_back(TimeBound(f.isBoundStrict(i), f.getBound(i)));
        storm::logic::TimeBoundReference tbr = f.getTimeBoundReference(i);
        if (tbr.hasRewardAccumulation() && canEliminate(tbr.getRewardAccumulation(), tbr.getRewardName())) {
            // Eliminate accumulation
            tbr = storm::logic::TimeBoundReference(tbr.getRewardName(), boost::none);
        }
        timeBoundReferences.push_back(std::move(tbr));
    }
    return std::static_pointer_cast<Formula>(std::make_shared<CumulativeRewardFormula>(bounds, timeBoundReferences, rewAcc));
}

boost::any RewardAccumulationEliminationVisitor::visit(LongRunAverageRewardFormula const& f, boost::any const& data) const {
    STORM_LOG_THROW(!data.empty(), storm::exceptions::UnexpectedException, "Formula " << f << " does not seem to be a subformula of a reward operator.");
    auto rewName = boost::any_cast<boost::optional<std::string>>(data);
    if (!f.hasRewardAccumulation() || canEliminate(f.getRewardAccumulation(), rewName)) {
        return std::static_pointer_cast<Formula>(std::make_shared<LongRunAverageRewardFormula>());
    } else {
        return std::static_pointer_cast<Formula>(std::make_shared<LongRunAverageRewardFormula>(f.getRewardAccumulation()));
    }
}

boost::any RewardAccumulationEliminationVisitor::visit(EventuallyFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, data));
    if (f.hasRewardAccumulation()) {
        if (f.isTimePathFormula()) {
            if (model.isDiscreteTimeModel() && ((!f.getRewardAccumulation().isExitSet() && !f.getRewardAccumulation().isStepsSet()) ||
                                                (f.getRewardAccumulation().isStepsSet() && f.getRewardAccumulation().isExitSet()))) {
                return std::static_pointer_cast<Formula>(std::make_shared<EventuallyFormula>(subformula, f.getContext(), f.getRewardAccumulation()));
            } else if (!model.isDiscreteTimeModel() &&
                       (!f.getRewardAccumulation().isTimeSet() || f.getRewardAccumulation().isExitSet() || f.getRewardAccumulation().isStepsSet())) {
                return std::static_pointer_cast<Formula>(std::make_shared<EventuallyFormula>(subformula, f.getContext(), f.getRewardAccumulation()));
            }
        } else if (f.isRewardPathFormula()) {
            STORM_LOG_THROW(!data.empty(), storm::exceptions::UnexpectedException,
                            "Formula " << f << " does not seem to be a subformula of a reward operator.");
            auto rewName = boost::any_cast<boost::optional<std::string>>(data);
            if (!canEliminate(f.getRewardAccumulation(), rewName)) {
                return std::static_pointer_cast<Formula>(std::make_shared<EventuallyFormula>(subformula, f.getContext(), f.getRewardAccumulation()));
            }
        }
    }
    return std::static_pointer_cast<Formula>(std::make_shared<EventuallyFormula>(subformula, f.getContext()));
}

boost::any RewardAccumulationEliminationVisitor::visit(RewardOperatorFormula const& f, boost::any const& data) const {
    std::shared_ptr<Formula> subformula = boost::any_cast<std::shared_ptr<Formula>>(f.getSubformula().accept(*this, f.getOptionalRewardModelName()));
    return std::static_pointer_cast<Formula>(std::make_shared<RewardOperatorFormula>(subformula, f.getOptionalRewardModelName(), f.getOperatorInformation()));
}

boost::any RewardAccumulationEliminationVisitor::visit(TotalRewardFormula const& f, boost::any const& data) const {
    STORM_LOG_THROW(!data.empty(), storm::exceptions::UnexpectedException, "Formula " << f << " does not seem to be a subformula of a reward operator.");
    auto rewName = boost::any_cast<boost::optional<std::string>>(data);
    if (!f.hasRewardAccumulation() || canEliminate(f.getRewardAccumulation(), rewName)) {
        return std::static_pointer_cast<Formula>(std::make_shared<TotalRewardFormula>());
    } else {
        return std::static_pointer_cast<Formula>(std::make_shared<TotalRewardFormula>(f.getRewardAccumulation()));
    }
}

bool RewardAccumulationEliminationVisitor::canEliminate(storm::logic::RewardAccumulation const& accumulation,
                                                        boost::optional<std::string> rewardModelName) const {
    STORM_LOG_THROW(rewardModelName.is_initialized(), storm::exceptions::InvalidPropertyException,
                    "Unable to find transient variable for unique reward model.");
    storm::jani::RewardModelInformation info(model, rewardModelName.get());

    if ((info.hasActionRewards() || info.hasTransitionRewards()) && !accumulation.isStepsSet()) {
        return false;
    }
    if (info.hasStateRewards()) {
        if (model.isDiscreteTimeModel()) {
            if (!accumulation.isExitSet()) {
                return false;
            }
            // accumulating over time in discrete time models has no effect, i.e., the value of accumulation.isTimeSet() does not matter here.
        } else {
            if (accumulation.isExitSet() || !accumulation.isTimeSet()) {
                return false;
            }
        }
    }
    return true;
}

boost::any RewardAccumulationEliminationVisitor::visit(DiscountedCumulativeRewardFormula const& f, boost::any const& data) const {
    boost::optional<storm::logic::RewardAccumulation> rewAcc;
    STORM_LOG_THROW(!data.empty(), storm::exceptions::UnexpectedException, "Formula " << f << " does not seem to be a subformula of a reward operator.");
    auto rewName = boost::any_cast<boost::optional<std::string>>(data);
    if (f.hasRewardAccumulation() && !canEliminate(f.getRewardAccumulation(), rewName)) {
        rewAcc = f.getRewardAccumulation();
    }

    std::vector<TimeBound> bounds;
    std::vector<TimeBoundReference> timeBoundReferences;
    for (uint64_t i = 0; i < f.getDimension(); ++i) {
        bounds.emplace_back(TimeBound(f.isBoundStrict(i), f.getBound(i)));
        storm::logic::TimeBoundReference tbr = f.getTimeBoundReference(i);
        if (tbr.hasRewardAccumulation() && canEliminate(tbr.getRewardAccumulation(), tbr.getRewardName())) {
            // Eliminate accumulation
            tbr = storm::logic::TimeBoundReference(tbr.getRewardName(), boost::none);
        }
        timeBoundReferences.push_back(std::move(tbr));
    }

    return std::static_pointer_cast<Formula>(std::make_shared<DiscountedCumulativeRewardFormula>(f.getDiscountFactor(), bounds, timeBoundReferences, rewAcc));
}

boost::any RewardAccumulationEliminationVisitor::visit(DiscountedTotalRewardFormula const& f, boost::any const& data) const {
    STORM_LOG_THROW(!data.empty(), storm::exceptions::UnexpectedException, "Formula " << f << " does not seem to be a subformula of a reward operator.");
    auto rewName = boost::any_cast<boost::optional<std::string>>(data);
    if (!f.hasRewardAccumulation() || canEliminate(f.getRewardAccumulation(), rewName)) {
        return std::static_pointer_cast<Formula>(std::make_shared<DiscountedTotalRewardFormula>(f.getDiscountFactor()));
    } else {
        return std::static_pointer_cast<Formula>(std::make_shared<DiscountedTotalRewardFormula>(f.getDiscountFactor(), f.getRewardAccumulation()));
    }
}
}  // namespace logic
}  // namespace storm
