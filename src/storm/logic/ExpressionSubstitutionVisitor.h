#pragma once

#include <functional>
#include <map>

#include "storm/logic/CloneVisitor.h"

#include "storm/storage/expressions/Expression.h"

namespace storm {

namespace logic {

class ExpressionSubstitutionVisitor : public CloneVisitor {
   public:
    ExpressionSubstitutionVisitor() = default;

    std::shared_ptr<Formula> substitute(Formula const& f,
                                        std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const& substitutionFunction) const;

    virtual boost::any visit(TimeOperatorFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(LongRunAverageOperatorFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(ProbabilityOperatorFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(RewardOperatorFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(BoundedUntilFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(CumulativeRewardFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(DiscountedCumulativeRewardFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(DiscountedTotalRewardFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(InstantaneousRewardFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(AtomicExpressionFormula const& f, boost::any const& data) const override;
};

}  // namespace logic
}  // namespace storm
