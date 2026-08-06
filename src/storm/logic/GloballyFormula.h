#pragma once

#include "storm/logic/UnaryPathFormula.h"

namespace storm {
namespace logic {
class GloballyFormula : public UnaryPathFormula {
   public:
    GloballyFormula(std::shared_ptr<Formula const> const& subformula);

    virtual ~GloballyFormula() {
        // Intentionally left empty.
    }

    virtual bool isGloballyFormula() const override;
    virtual bool isProbabilityPathFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;
};
}  // namespace logic
}  // namespace storm
