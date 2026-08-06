#pragma once

#include <string>

#include "storm/logic/StateFormula.h"

namespace storm {
namespace logic {
class AtomicLabelFormula : public StateFormula {
   public:
    AtomicLabelFormula(std::string const& label);

    virtual ~AtomicLabelFormula() {
        // Intentionally left empty.
    }

    virtual bool isAtomicLabelFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    std::string const& getLabel() const;

    virtual void gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

   private:
    std::string label;
};
}  // namespace logic
}  // namespace storm
