#pragma once

#include "storm/logic/UnaryBooleanOperatorType.h"
#include "storm/logic/UnaryStateFormula.h"

namespace storm {
namespace logic {
class UnaryBooleanStateFormula : public UnaryStateFormula {
   public:
    typedef storm::logic::UnaryBooleanOperatorType OperatorType;

    UnaryBooleanStateFormula(OperatorType operatorType, std::shared_ptr<Formula const> const& subformula);

    virtual ~UnaryBooleanStateFormula() {
        // Intentionally left empty.
    }

    virtual bool isUnaryBooleanStateFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    OperatorType getOperator() const;

    virtual bool isNot() const;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

   private:
    OperatorType operatorType;
};
}  // namespace logic
}  // namespace storm
