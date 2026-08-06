#pragma once

#include <memory>
#include "storm/logic/PlayerCoalition.h"
#include "storm/logic/UnaryStateFormula.h"

namespace storm {
namespace logic {
class GameFormula : public UnaryStateFormula {
   public:
    GameFormula(PlayerCoalition const& coalition, std::shared_ptr<Formula const> subFormula);

    virtual ~GameFormula() {
        // Intentionally left empty.
    }

    PlayerCoalition const& getCoalition() const;
    virtual bool isGameFormula() const override;
    virtual bool hasQualitativeResult() const override;
    virtual bool hasQuantitativeResult() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

   private:
    PlayerCoalition coalition;
};
}  // namespace logic
}  // namespace storm
