#pragma once

#include "storm/storage/dd/DdType.h"

namespace storm {
namespace dd {
// Declare DdIterator class so we can then specialize it for the different DD types.
template<DdType Type, typename ValueType>
class AddIterator;
}  // namespace dd
}  // namespace storm
