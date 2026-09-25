#pragma once

#include <ostream>

#include "storm-parsers/storm-parsers-api.h"

namespace storm {
namespace parser {

enum class ConstantDataType { Bool, Integer, Rational };

STORM_PARSERS_API std::ostream& operator<<(std::ostream& out, ConstantDataType const& constantDataType);
}  // namespace parser
}  // namespace storm
