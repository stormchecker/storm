#include "FormulaContext.h"

#include <ostream>

namespace storm {
namespace logic {
std::ostream& operator<<(std::ostream& out, FormulaContext const& formulaContext) {
    switch (formulaContext) {
        case storm::logic::FormulaContext::Undefined:
            out << "Undefined";
            break;
        case storm::logic::FormulaContext::Probability:
            out << "Probability";
            break;
        case storm::logic::FormulaContext::Reward:
            out << "Reward";
            break;
        case storm::logic::FormulaContext::LongRunAverage:
            out << "LongRunAverage";
            break;
        case storm::logic::FormulaContext::Time:
            out << "Time";
            break;
    }
    return out;
}
}  // namespace logic
}  // namespace storm
