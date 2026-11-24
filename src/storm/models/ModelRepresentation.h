#pragma once

#include <ostream>

#include "storm/dd/DdType.h"
#include "storm/dd/backend/DdBackends.h"

namespace storm {
namespace models {

/*!
 * The representation of a model: either sparse or symbolic with one of the DD backends.
 * The representation of a DD model is derived from its DdType.
 */
enum class ModelRepresentation { Sparse = 0 };
std::ostream& operator<<(std::ostream& os, ModelRepresentation const& representation);

template<storm::dd::DdType ddType>
struct GetModelRepresentation {
    static const ModelRepresentation representation = static_cast<ModelRepresentation>(1 + static_cast<int>(ddType));
};

template<ModelRepresentation representation>
struct GetDdType {
    // For the sparse representation there is no DD type. The first registered backend serves as a placeholder.
    static const storm::dd::DdType ddType = representation == ModelRepresentation::Sparse
                                                ? storm::dd::RegisteredDdTypes::first
                                                : static_cast<storm::dd::DdType>(static_cast<int>(representation) - 1);
};

}  // namespace models
}  // namespace storm
