#include "storm/logic/UnaryBooleanPathFormula.h"
#include <boost/any.hpp>
#include <ostream>

#include "storm/logic/FormulaVisitor.h"

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
UnaryBooleanPathFormula::UnaryBooleanPathFormula(OperatorType operatorType, std::shared_ptr<Formula const> const& subformula, FormulaContext context)
    : UnaryPathFormula(subformula), operatorType(operatorType), context(context) {
    STORM_LOG_THROW(this->getSubformula().isStateFormula() || this->getSubformula().isPathFormula(), storm::exceptions::InvalidPropertyException,
                    "Boolean path formula must have either path or state subformulas.");
    STORM_LOG_THROW(context == FormulaContext::Probability, storm::exceptions::InvalidPropertyException, "Invalid context for formula.");
}

FormulaContext const& UnaryBooleanPathFormula::getContext() const {
    return context;
}

bool UnaryBooleanPathFormula::isUnaryBooleanPathFormula() const {
    return true;
}

bool UnaryBooleanPathFormula::isProbabilityPathFormula() const {
    return true;
}

boost::any UnaryBooleanPathFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

UnaryBooleanPathFormula::OperatorType UnaryBooleanPathFormula::getOperator() const {
    return operatorType;
}

bool UnaryBooleanPathFormula::isNot() const {
    return this->getOperator() == OperatorType::Not;
}

std::ostream& UnaryBooleanPathFormula::writeToStream(std::ostream& out, bool allowParentheses) const {
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
