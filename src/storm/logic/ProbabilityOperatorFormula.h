#pragma once

#include "storm/logic/OperatorFormula.h"

namespace storm {
namespace logic {
class ProbabilityOperatorFormula : public OperatorFormula {
   public:
    ProbabilityOperatorFormula(std::shared_ptr<Formula const> const& subformula, OperatorInformation const& operatorInformation = OperatorInformation());

    virtual ~ProbabilityOperatorFormula() {
        // Intentionally left empty.
    }

    virtual bool isProbabilityOperatorFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;
};
}  // namespace logic
}  // namespace storm
