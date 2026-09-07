#include "InitialConstruct.h"

#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace prism {
InitialConstruct::InitialConstruct(storm::expressions::Expression initialStatesExpression, std::string const& filename, uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber), initialStatesExpression(initialStatesExpression) {
    // Intentionally left empty.
}

storm::expressions::Expression InitialConstruct::getInitialStatesExpression() const {
    return this->initialStatesExpression;
}

InitialConstruct InitialConstruct::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    return InitialConstruct(this->getInitialStatesExpression().substitute(substitution));
}

std::ostream& operator<<(std::ostream& stream, InitialConstruct const& initialConstruct) {
    stream << "init \n";
    stream << "\t" << initialConstruct.getInitialStatesExpression() << '\n';
    stream << "endinit\n";
    return stream;
}
}  // namespace prism
}  // namespace storm
