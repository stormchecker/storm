#pragma once

#include <boost/optional.hpp>
#include "storm/logic/PathFormula.h"
#include "storm/logic/RewardAccumulation.h"

namespace storm {
namespace logic {
class LongRunAverageRewardFormula : public PathFormula {
   public:
    LongRunAverageRewardFormula(boost::optional<RewardAccumulation> rewardAccumulation = boost::none);

    virtual ~LongRunAverageRewardFormula() {
        // Intentionally left empty.
    }

    virtual bool isLongRunAverageRewardFormula() const override;
    virtual bool isRewardPathFormula() const override;
    bool hasRewardAccumulation() const;
    RewardAccumulation const& getRewardAccumulation() const;
    std::shared_ptr<LongRunAverageRewardFormula const> stripRewardAccumulation() const;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

   private:
    boost::optional<RewardAccumulation> rewardAccumulation;
};
}  // namespace logic
}  // namespace storm
