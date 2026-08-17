#pragma once

#include <boost/optional.hpp>

#include "storm/logic/RewardAccumulation.h"
#include "storm/logic/TotalRewardFormula.h"
#include "storm/storage/expressions/Expression.h"

namespace storm {
namespace logic {
class DiscountedTotalRewardFormula : public TotalRewardFormula {
   public:
    DiscountedTotalRewardFormula(storm::expressions::Expression const discountFactor, boost::optional<RewardAccumulation> rewardAccumulation = boost::none);

    virtual ~DiscountedTotalRewardFormula() {
        // Intentionally left empty.
    }

    virtual bool isDiscountedTotalRewardFormula() const override;

    virtual bool isTotalRewardFormula() const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

    void gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const override;

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
