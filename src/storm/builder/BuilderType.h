#pragma once

#include <vector>

#include "storm/utility/OptionalRef.h"

namespace storm {
namespace languages {
class SymbolicModelDescription;
}
namespace jani {
class ModelFeatures;
class Property;
}  // namespace jani

namespace builder {
enum class BuilderType { Explicit, Dd };

storm::jani::ModelFeatures getSupportedJaniFeatures(BuilderType const& builderType);

template<typename ValueType>
bool canHandle(BuilderType const& builderType, storm::languages::SymbolicModelDescription const& modelDescription,
               storm::OptionalRef<std::vector<storm::jani::Property> const> properties = storm::NullRef);
}  // namespace builder
}  // namespace storm