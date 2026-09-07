#include "BinaryBooleanStateFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
BinaryBooleanStateFormula::BinaryBooleanStateFormula(OperatorType operatorType, std::shared_ptr<Formula const> const& leftSubformula,
                                                     std::shared_ptr<Formula const> const& rightSubformula)
    : BinaryStateFormula(leftSubformula, rightSubformula), operatorType(operatorType) {
    STORM_LOG_THROW(this->getLeftSubformula().hasQualitativeResult() && this->getRightSubformula().hasQualitativeResult(),
                    storm::exceptions::InvalidPropertyException, "Boolean formula must have subformulas with qualitative result.");
}

bool BinaryBooleanStateFormula::isBinaryBooleanStateFormula() const {
    return true;
}

boost::any BinaryBooleanStateFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

BinaryBooleanStateFormula::OperatorType BinaryBooleanStateFormula::getOperator() const {
    return operatorType;
}

bool BinaryBooleanStateFormula::isAnd() const {
    return this->getOperator() == OperatorType::And;
}

bool BinaryBooleanStateFormula::isOr() const {
    return this->getOperator() == OperatorType::Or;
}

std::ostream& BinaryBooleanStateFormula::writeToStream(std::ostream& out, bool allowParentheses) const {
    if (allowParentheses) {
        out << "(";
    }
    this->getLeftSubformula().writeToStream(out, true);
    switch (operatorType) {
        case OperatorType::And:
            out << " & ";
            break;
        case OperatorType::Or:
            out << " | ";
            break;
    }
    this->getRightSubformula().writeToStream(out, true);
    if (allowParentheses) {
        out << ")";
    }
    return out;
}
}  // namespace logic
}  // namespace storm
