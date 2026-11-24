#pragma once

#include <ostream>

namespace storm {
namespace jani {

enum class ModelType { UNDEFINED = 0, LTS = 1, DTMC = 2, CTMC = 3, MDP = 4, CTMDP = 5, MA = 6, TA = 7, PTA = 8, STA = 9, HA = 10, PHA = 11, SHA = 12 };

ModelType getModelType(std::string const& input);
std::string to_string(ModelType const& type);
std::ostream& operator<<(std::ostream& stream, ModelType const& type);

bool isDeterministicModel(ModelType const& modelType);
bool isDiscreteTimeModel(ModelType const& modelType);

}  // namespace jani
}  // namespace storm
