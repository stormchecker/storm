#pragma once

#include "storm/logic/CumulativeRewardFormula.h"

#include "storm/logic/TimeBound.h"
#include "storm/logic/TimeBoundType.h"

namespace storm {
namespace logic {
class DiscountedCumulativeRewardFormula : public CumulativeRewardFormula {
   public:
    DiscountedCumulativeRewardFormula(storm::expressions::Expression const& discountFactor, TimeBound const& bound,
                                      TimeBoundReference const& timeBoundReference = TimeBoundReference(TimeBoundType::Time),
                                      boost::optional<RewardAccumulation> rewardAccumulation = boost::none);
    DiscountedCumulativeRewardFormula(storm::expressions::Expression const& discountFactor, std::vector<TimeBound> const& bounds,
                                      std::vector<TimeBoundReference> const& timeBoundReferences,
                                      boost::optional<RewardAccumulation> rewardAccumulation = boost::none);

    virtual ~DiscountedCumulativeRewardFormula() = default;

    virtual bool isDiscountedCumulativeRewardFormula() const override;

    virtual bool isCumulativeRewardFormula() const override;

    void gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const override;

    std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

    storm::expressions::Expression const& getDiscountFactor() const;

    template<typename ValueType>
    ValueType getDiscountFactor() const;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

   private:
    static void checkNoVariablesInDiscountFactor(storm::expressions::Expression const& factor);

    storm::expressions::Expression const discountFactor;
};
}  // namespace logic
}  // namespace storm