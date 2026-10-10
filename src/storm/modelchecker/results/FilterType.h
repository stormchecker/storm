#pragma once
#include <string>

namespace storm {
namespace modelchecker {

enum class StateFilter { ARGMIN, ARGMAX };

// PRISM also defines 'first', 'range', 'state' and 'print' that do not have equivalent here
// 'values' is 'printall' in PRISM
enum class FilterType { MIN, MAX, SUM, AVG, COUNT, FORALL, EXISTS, ARGMIN, ARGMAX, VALUES };

std::string toString(FilterType);
std::string toPrismSyntax(FilterType);
}  // namespace modelchecker
}  // namespace storm
