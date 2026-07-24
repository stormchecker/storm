#pragma once

#include "storm/logic/FormulaContext.h"
#include "storm/logic/UnaryBooleanOperatorType.h"
#include "storm/logic/UnaryPathFormula.h"

namespace storm {
namespace logic {
class UnaryBooleanPathFormula : public UnaryPathFormula {
   public:
    typedef storm::logic::UnaryBooleanOperatorType OperatorType;

    UnaryBooleanPathFormula(OperatorType operatorType, std::shared_ptr<Formula const> const& subformula, FormulaContext context = FormulaContext::Probability);

    virtual ~UnaryBooleanPathFormula() {
        // Intentionally left empty.
    };

    FormulaContext const& getContext() const;

    virtual bool isUnaryBooleanPathFormula() const override;
    virtual bool isProbabilityPathFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    OperatorType getOperator() const;

    virtual bool isNot() const;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

   private:
    OperatorType operatorType;
    FormulaContext context;
};
}  // namespace logic
}  // namespace storm
