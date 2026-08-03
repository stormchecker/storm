#pragma once

#include "storm/logic/Formula.h"

namespace storm {
namespace logic {
class StateFormula : public Formula {
   public:
    virtual ~StateFormula() {
        // Intentionally left empty.
    }

    virtual bool isStateFormula() const override;

    virtual bool isProbabilityPathFormula() const override;
};
}  // namespace logic
}  // namespace storm
