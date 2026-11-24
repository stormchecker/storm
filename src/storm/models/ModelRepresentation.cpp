#include "storm/models/ModelRepresentation.h"

#include "storm/dd/backend/BackendTraits.h"
#include "storm/dd/backend/DdBackends.h"

namespace storm {
namespace models {

std::ostream& operator<<(std::ostream& os, ModelRepresentation const& representation) {
    if (representation == ModelRepresentation::Sparse) {
        os << "sparse";
    } else {
        storm::dd::dispatchDdType(static_cast<storm::dd::DdType>(static_cast<int>(representation) - 1),
                                  [&os](auto ddType) { os << "DD (" << storm::dd::BackendTraits<decltype(ddType)::value>::name << ")"; });
    }
    return os;
}
}  // namespace models
}  // namespace storm
