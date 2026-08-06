#pragma once

#include "storm/logic/Formula.h"

namespace storm {
namespace logic {
class PathFormula : public Formula {
   public:
    virtual ~PathFormula() {
        // Intentionally left empty.
    }

    virtual bool isPathFormula() const override;
};
}  // namespace logic
}  // namespace storm
