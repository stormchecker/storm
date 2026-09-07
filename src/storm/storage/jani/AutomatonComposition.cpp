#include "AutomatonComposition.h"

namespace storm {
namespace jani {

AutomatonComposition::AutomatonComposition(std::string const& name, std::set<std::string> const& inputEnabledActions)
    : name(name), inputEnabledActions(inputEnabledActions) {
    // Intentionally left empty.
}

boost::any AutomatonComposition::accept(CompositionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

std::string const& AutomatonComposition::getAutomatonName() const {
    return name;
}

std::set<std::string> const& AutomatonComposition::getInputEnabledActions() const {
    return inputEnabledActions;
}

bool AutomatonComposition::isAutomatonComposition() const {
    return true;
}

void AutomatonComposition::write(std::ostream& stream) const {
    stream << name;
}

}  // namespace jani
}  // namespace storm
