#pragma once

#include <map>

#include "storm/logic/BinaryBooleanOperatorType.h"
#include "storm/logic/BinaryPathFormula.h"
#include "storm/logic/FormulaContext.h"

namespace storm {
namespace logic {
class BinaryBooleanPathFormula : public BinaryPathFormula {
   public:
    typedef storm::logic::BinaryBooleanOperatorType OperatorType;

    BinaryBooleanPathFormula(OperatorType operatorType, std::shared_ptr<Formula const> const& leftSubformula,
                             std::shared_ptr<Formula const> const& rightSubformula, FormulaContext context = FormulaContext::Probability);

    virtual ~BinaryBooleanPathFormula() {
        // Intentionally left empty.
    };

    FormulaContext const& getContext() const;

    virtual bool isBinaryBooleanPathFormula() const override;
    virtual bool isProbabilityPathFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    OperatorType getOperator() const;

    virtual bool isAnd() const;
    virtual bool isOr() const;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

   private:
    OperatorType operatorType;
    FormulaContext context;
};
}  // namespace logic
}  // namespace storm
