#pragma once

#include <cstdint>
#include <string>

#include "storm/languages/jani/ModelType.h"

namespace storm {
namespace jani {

class Model;

struct InformationObject {
    InformationObject();
    storm::jani::ModelType modelType;  /// The type of the model
    uint64_t nrVariables;              /// The number of non-transient variables in the model
    uint64_t nrAutomata;               /// The number of automata in the model
    uint64_t nrEdges;                  /// The number of edges in the model
    uint64_t nrLocations;              /// The numer of all locations of all automata of the model
    uint64_t stateDomainSize;          /// The size of the domain of the states (i.e., the product of the range of all variables times the number of locations).
                                       /// Here, 0 means that the state domain size is unknown.
    double avgVarDomainSize;           /// The average range of the domain a variable can have
                                       /// Here, 0 means that the range of at least one non-transient variable is unknown.
};

InformationObject collectModelInformation(Model const& model);
}  // namespace jani
}  // namespace storm
