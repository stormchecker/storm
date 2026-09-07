#include "ModelRepresentation.h"

#include "storm/exceptions/InvalidTypeException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace models {

std::ostream& operator<<(std::ostream& os, ModelRepresentation const& representation) {
    switch (representation) {
        case ModelRepresentation::Sparse:
            os << "sparse";
            break;
        case ModelRepresentation::DdCudd:
            os << "DD (Cudd)";
            break;
        case ModelRepresentation::DdSylvan:
            os << "DD (Sylvan)";
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Unknown model representation.");
    }
    return os;
}
}  // namespace models
}  // namespace storm
