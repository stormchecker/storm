#pragma once

namespace storm {

namespace expressions {
class Expression;
}

namespace jani {

class Model;

bool containsArrayExpression(Model const& model);
bool containsArrayExpression(storm::expressions::Expression const& expr);
}  // namespace jani
}  // namespace storm
