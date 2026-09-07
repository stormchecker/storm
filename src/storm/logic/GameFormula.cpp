#include "GameFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/logic/FormulaVisitor.h"

namespace storm {
namespace logic {
GameFormula::GameFormula(PlayerCoalition const& coalition, std::shared_ptr<Formula const> subformula) : UnaryStateFormula(subformula), coalition(coalition) {
    // Intentionally left empty.
}

bool GameFormula::isGameFormula() const {
    return true;
}

bool GameFormula::hasQualitativeResult() const {
    return this->getSubformula().hasQualitativeResult();
}

bool GameFormula::hasQuantitativeResult() const {
    return this->getSubformula().hasQuantitativeResult();
}

PlayerCoalition const& GameFormula::getCoalition() const {
    return coalition;
}

boost::any GameFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

std::ostream& GameFormula::writeToStream(std::ostream& out, bool /* allowParentheses */) const {
    // No parenthesis necessary
    out << "<<" << coalition << ">> ";
    this->getSubformula().writeToStream(out, true);
    return out;
}
}  // namespace logic
}  // namespace storm
