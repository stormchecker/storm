#include "UnaryBooleanStateFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
UnaryBooleanStateFormula::UnaryBooleanStateFormula(OperatorType operatorType, std::shared_ptr<Formula const> const& subformula)
    : UnaryStateFormula(subformula), operatorType(operatorType) {
    STORM_LOG_THROW(this->getSubformula().hasQualitativeResult(), storm::exceptions::InvalidPropertyException,
                    "Boolean formula must have subformulas with qualitative result.");
}

bool UnaryBooleanStateFormula::isUnaryBooleanStateFormula() const {
    return true;
}

boost::any UnaryBooleanStateFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

UnaryBooleanStateFormula::OperatorType UnaryBooleanStateFormula::getOperator() const {
    return operatorType;
}

bool UnaryBooleanStateFormula::isNot() const {
    return this->getOperator() == OperatorType::Not;
}

std::ostream& UnaryBooleanStateFormula::writeToStream(std::ostream& out, bool allowParentheses) const {
    if (allowParentheses) {
        out << "(";
    }
    switch (operatorType) {
        case OperatorType::Not:
            out << "!";
            break;
    }
    this->getSubformula().writeToStream(out, !this->getSubformula().isUnaryFormula());
    if (allowParentheses) {
        out << ")";
    }
    return out;
}
}  // namespace logic
}  // namespace storm
