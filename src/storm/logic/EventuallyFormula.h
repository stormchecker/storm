#pragma once

#include <boost/optional.hpp>

#include "storm/logic/FormulaContext.h"
#include "storm/logic/RewardAccumulation.h"
#include "storm/logic/UnaryPathFormula.h"

namespace storm {
namespace logic {
class EventuallyFormula : public UnaryPathFormula {
   public:
    EventuallyFormula(std::shared_ptr<Formula const> const& subformula, FormulaContext context = FormulaContext::Probability,
                      boost::optional<RewardAccumulation> rewardAccumulation = boost::none);

    virtual ~EventuallyFormula() {
        // Intentionally left empty.
    }

    FormulaContext const& getContext() const;

    virtual bool isEventuallyFormula() const override;
    virtual bool isReachabilityProbabilityFormula() const override;
    virtual bool isReachabilityRewardFormula() const override;
    virtual bool isReachabilityTimeFormula() const override;
    virtual bool isProbabilityPathFormula() const override;
    virtual bool isRewardPathFormula() const override;
    virtual bool isTimePathFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;
    bool hasRewardAccumulation() const;
    RewardAccumulation const& getRewardAccumulation() const;

   private:
    FormulaContext context;
    boost::optional<RewardAccumulation> rewardAccumulation;
};
}  // namespace logic
}  // namespace storm
