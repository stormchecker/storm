#pragma once

#include "storm/logic/PathFormula.h"

#include "storm/logic/TimeBoundType.h"
#include "storm/storage/expressions/Expression.h"

namespace storm {
namespace logic {
class InstantaneousRewardFormula : public PathFormula {
   public:
    InstantaneousRewardFormula(storm::expressions::Expression const& bound, TimeBoundType const& timeBoundType = TimeBoundType::Time);

    virtual ~InstantaneousRewardFormula() {
        // Intentionally left empty.
    }

    virtual bool isInstantaneousRewardFormula() const override;

    virtual bool isRewardPathFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

    TimeBoundType const& getTimeBoundType() const;
    bool isStepBounded() const;
    bool isTimeBounded() const;

    bool hasIntegerBound() const;

    storm::expressions::Expression const& getBound() const;

    template<typename ValueType>
    ValueType getBound() const;

    virtual void gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const override;

   private:
    static void checkNoVariablesInBound(storm::expressions::Expression const& bound);

    TimeBoundType timeBoundType;
    storm::expressions::Expression bound;
};
}  // namespace logic
}  // namespace storm
