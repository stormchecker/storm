#pragma once
#include <string>

namespace storm {
namespace modelchecker {

enum class StateFilter { ARGMIN, ARGMAX };

enum class FilterType { MIN, MAX, SUM, AVG, COUNT, FORALL, EXISTS, ARGMIN, ARGMAX, VALUES };

std::string toString(FilterType);
std::string toPrismSyntax(FilterType);
}  // namespace modelchecker
}  // namespace storm
