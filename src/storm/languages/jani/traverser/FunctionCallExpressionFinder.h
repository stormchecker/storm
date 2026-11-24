#pragma once

#include <string>
#include <unordered_set>

namespace storm {

namespace expressions {
class Expression;
}

namespace jani {

class Model;

bool containsFunctionCallExpression(Model const& model);
std::unordered_set<std::string> getOccurringFunctionCalls(storm::expressions::Expression const& expr);
}  // namespace jani
}  // namespace storm
