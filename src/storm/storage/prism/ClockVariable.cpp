#include "storm/storage/prism/ClockVariable.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/constants.h"

namespace storm {
namespace prism {
ClockVariable::ClockVariable(storm::expressions::Variable const& variable, bool observable, std::string const& filename, uint_fast64_t lineNumber)
    : Variable(variable, variable.getManager().rational(storm::utility::zero<storm::RationalNumber>()), observable, filename, lineNumber) {
    // Nothing to do here.
}

void ClockVariable::createMissingInitialValue() {
    if (!this->hasInitialValue()) {
        this->setInitialValueExpression(this->getExpressionVariable().getManager().rational(storm::utility::zero<storm::RationalNumber>()));
    }
}

std::ostream& operator<<(std::ostream& stream, ClockVariable const& variable) {
    stream << variable.getName() << ": clock" << ";";
    return stream;
}

}  // namespace prism
}  // namespace storm
