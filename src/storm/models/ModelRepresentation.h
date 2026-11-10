#pragma once

#include <ostream>

#include "storm/dd/DdType.h"

namespace storm {
namespace models {
enum class ModelRepresentation { Sparse, DdCudd, DdSylvan };
std::ostream& operator<<(std::ostream& os, ModelRepresentation const& representation);

template<storm::dd::DdType ddType>
struct GetModelRepresentation;
template<>
struct GetModelRepresentation<storm::dd::DdType::CUDD> {
    static const ModelRepresentation representation = ModelRepresentation::DdCudd;
};
template<>
struct GetModelRepresentation<storm::dd::DdType::Sylvan> {
    static const ModelRepresentation representation = ModelRepresentation::DdSylvan;
};

template<ModelRepresentation representation>
struct GetDdType;
template<>
struct GetDdType<ModelRepresentation::Sparse> {
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
};
template<>
struct GetDdType<ModelRepresentation::DdCudd> {
    static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
};
template<>
struct GetDdType<ModelRepresentation::DdSylvan> {
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
};

}  // namespace models
}  // namespace storm
