#pragma once

#include <boost/optional.hpp>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "storm/languages/jani/ModelFeatures.h"

namespace storm {
namespace converter {

struct JaniConversionOptions {
    /// (Automaton,Variable)-pairs that will be transformed to location variables of the respective automaton.
    std::vector<std::pair<std::string, std::string>> locationVariables;

    /// If set, the model might have transient assignments to the edges
    bool edgeAssignments = false;

    /// If set, the model is transformed into a single automaton
    bool flatten = false;

    /// If set, constants in expressions are substituted with their definition
    bool substituteConstants = true;

    /// If set, variables will be made local wherever possible
    bool localVars = false;

    /// If set, variables will be made global wherever possible
    bool globalVars = false;

    /// If given, the model will get this name
    boost::optional<std::string> modelName;

    /// Only these model features are allowed in the output
    storm::jani::ModelFeatures allowedModelFeatures = storm::jani::getAllKnownModelFeatures();

    /// Add constants that are defined in the properties to the model
    bool addPropertyConstants = true;

    /// If set, local and global variables that are (a) not assigned to some value and (b) have a known initial value are replaced by constants.
    bool replaceUnassignedVariablesWithConstants = false;

    /// If set, attempts to simplify the system composition
    bool simplifyComposition = false;

    /// If set, attempts to perform location elimination of states to reduce the state space of the final model
    bool locationElimination = false;

    /// Controls which locations are eliminated if location elimination is enabled
    uint64_t locationEliminationLocationHeuristic = 10;

    // Controls which edges are eliminated if location elimination is enabled
    uint64_t locationEliminationEdgeHeuristic = 10000;
};
}  // namespace converter
}  // namespace storm
