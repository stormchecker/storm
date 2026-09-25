#pragma once

#include <string>
#include <unordered_map>

#include "storm-parsers/storm-parsers-api.h"

namespace storm {
namespace parser {
STORM_PARSERS_API std::unordered_map<std::string, std::string> parseKeyValueString(std::string const& keyValueString);
}
}  // namespace storm
