#include "ContinuousType.h"

#include <string>

namespace storm {
namespace jani {
ContinuousType::ContinuousType() {
    // Intentionally left empty
}

bool ContinuousType::isContinuousType() const {
    return true;
}

std::string ContinuousType::getStringRepresentation() const {
    return "continuous";
}

std::unique_ptr<JaniType> ContinuousType::clone() const {
    return std::make_unique<ContinuousType>(*this);
}

}  // namespace jani
}  // namespace storm
