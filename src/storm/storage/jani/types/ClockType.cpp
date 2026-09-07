#include "ClockType.h"

#include <string>

namespace storm {
namespace jani {
ClockType::ClockType() {
    // Intentionally left empty
}

bool ClockType::isClockType() const {
    return true;
}

std::string ClockType::getStringRepresentation() const {
    return "clock";
}

std::unique_ptr<JaniType> ClockType::clone() const {
    return std::make_unique<ClockType>(*this);
}

}  // namespace jani
}  // namespace storm