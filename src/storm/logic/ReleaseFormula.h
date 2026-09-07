#pragma once

#include "storm/logic/BinaryPathFormula.h"

namespace storm {
namespace logic {
class ReleaseFormula : public BinaryPathFormula {
   public:
    ReleaseFormula(std::shared_ptr<Formula const> const& leftSubformula, std::shared_ptr<Formula const> const& rightSubformula);

    virtual ~ReleaseFormula() {
        // Intentionally left empty.
    }

    virtual bool isReleaseFormula() const override;
    virtual bool isProbabilityPathFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;
};
}  // namespace logic
}  // namespace storm
