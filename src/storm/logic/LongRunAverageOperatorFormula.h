#pragma once

#include "storm/logic/OperatorFormula.h"

namespace storm {
namespace logic {
class LongRunAverageOperatorFormula : public OperatorFormula {
   public:
    LongRunAverageOperatorFormula(std::shared_ptr<Formula const> const& subformula, OperatorInformation const& operatorInformation = OperatorInformation());

    virtual ~LongRunAverageOperatorFormula() {
        // Intentionally left empty.
    }

    virtual bool isLongRunAverageOperatorFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;
};
}  // namespace logic
}  // namespace storm
