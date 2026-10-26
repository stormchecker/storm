#pragma once

#include <vector>

namespace storm {

namespace expressions {
class Expression;
}

namespace jani {
class Model;
class Property;

/*!
 * Eliminates all function references in the given model and the given properties by replacing them with their corresponding definitions.
 */
void eliminateFunctions(Model& model, std::vector<Property>& properties);

/*!
 * Eliminates all function calls in the given expression by replacing them with their corresponding definitions.
 * Only global function definitions are considered.
 */
storm::expressions::Expression eliminateFunctionCallsInExpression(storm::expressions::Expression const& expression, Model const& model);

}  // namespace jani
}  // namespace storm
