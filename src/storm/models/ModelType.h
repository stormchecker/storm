#pragma once

#include <iosfwd>

namespace storm {
namespace models {
// All supported model types.
enum class ModelType { Dtmc, Ctmc, Mdp, MarkovAutomaton, S2pg, Pomdp, Smg };

ModelType getModelType(std::string const& type);

std::ostream& operator<<(std::ostream& os, ModelType const& type);
}  // namespace models
}  // namespace storm
