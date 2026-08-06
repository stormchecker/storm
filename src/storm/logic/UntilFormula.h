#pragma once

#include "storm/logic/BinaryPathFormula.h"

namespace storm {
namespace logic {
class UntilFormula : public BinaryPathFormula {
   public:
    UntilFormula(std::shared_ptr<Formula const> const& leftSubformula, std::shared_ptr<Formula const> const& rightSubformula);

    virtual ~UntilFormula() {
        // Intentionally left empty.
    }

    virtual bool isUntilFormula() const override;
    virtual bool isProbabilityPathFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;
};
}  // namespace logic
}  // namespace storm
