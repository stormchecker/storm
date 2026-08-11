#pragma once

#include <map>

#include "storm/logic/BinaryBooleanOperatorType.h"
#include "storm/logic/BinaryStateFormula.h"

namespace storm {
namespace logic {
class BinaryBooleanStateFormula : public BinaryStateFormula {
   public:
    typedef storm::logic::BinaryBooleanOperatorType OperatorType;

    BinaryBooleanStateFormula(OperatorType operatorType, std::shared_ptr<Formula const> const& leftSubformula,
                              std::shared_ptr<Formula const> const& rightSubformula);

    virtual ~BinaryBooleanStateFormula() {
        // Intentionally left empty.
    }

    virtual bool isBinaryBooleanStateFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    OperatorType getOperator() const;

    virtual bool isAnd() const;
    virtual bool isOr() const;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

   private:
    OperatorType operatorType;
};
}  // namespace logic
}  // namespace storm
