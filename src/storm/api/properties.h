#pragma once

#include <boost/optional.hpp>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace storm {

namespace jani {
class Property;
class Model;
}  // namespace jani
namespace expressions {
class Variable;
class Expression;
}  // namespace expressions
namespace prism {
class Program;
}
namespace storage {
class SymbolicModelDescription;
}
namespace logic {
class Formula;
}

namespace api {

// Process properties.
std::vector<storm::jani::Property> substituteConstantsInProperties(std::vector<storm::jani::Property> const& properties,
                                                                   std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution);
std::vector<storm::jani::Property> substituteTranscendentalNumbersInProperties(std::vector<storm::jani::Property> const& properties);
std::vector<storm::jani::Property> filterProperties(std::vector<storm::jani::Property> const& properties,
                                                    boost::optional<std::set<std::string>> const& propertyFilter);
std::vector<std::shared_ptr<storm::logic::Formula const>> extractFormulasFromProperties(std::vector<storm::jani::Property> const& properties);
storm::jani::Property createMultiObjectiveProperty(std::vector<storm::jani::Property> const& properties, bool lexicographic);

}  // namespace api
}  // namespace storm
