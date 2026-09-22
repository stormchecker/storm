#include "storm/logic/ReleaseFormula.h"
#include <boost/any.hpp>
#include <ostream>

#include "storm/logic/FormulaVisitor.h"

namespace storm {
namespace logic {
ReleaseFormula::ReleaseFormula(std::shared_ptr<Formula const> const& leftSubformula, std::shared_ptr<Formula const> const& rightSubformula)
    : BinaryPathFormula(leftSubformula, rightSubformula) {
    // Intentionally left empty.
}

bool ReleaseFormula::isReleaseFormula() const {
    return true;
}

bool ReleaseFormula::isProbabilityPathFormula() const {
    return true;
}

boost::any ReleaseFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

std::ostream& ReleaseFormula::writeToStream(std::ostream& out, bool allowParentheses) const {
    if (allowParentheses) {
        out << "(";
    }
    this->getLeftSubformula().writeToStream(out, true);
    out << " R ";
    this->getRightSubformula().writeToStream(out, true);
    if (allowParentheses) {
        out << ")";
    }
    return out;
}
}  // namespace logic
}  // namespace storm
