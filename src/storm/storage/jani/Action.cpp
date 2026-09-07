#include "Action.h"

namespace storm {
namespace jani {
Action::Action(std::string const& name) : name(name) {
    // Intentionally left empty.
}

std::string const& Action::getName() const {
    return this->name;
}
}  // namespace jani
}  // namespace storm
