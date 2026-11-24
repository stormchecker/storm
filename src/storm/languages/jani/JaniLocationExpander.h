#pragma once

#include "storm/languages/jani/Automaton.h"
#include "storm/languages/jani/Model.h"

namespace storm {
namespace jani {
class JaniLocationExpander {
   public:
    explicit JaniLocationExpander(Model const& original);

    struct NewIndices {
        // Maps each old location index to a map that maps every variable value to the index of the (new) location that corresponds to the old location and
        // variable value
        std::map<uint64_t, std::map<int64_t, uint64_t>> locationVariableValueMap;
        // Contains all variable values of the expanded variable
        std::vector<storm::expressions::Expression> variableDomain;
        // Maps each excluded location index (cf. excludeLocation(...)) to its new index.
        std::map<uint64_t, uint64_t> excludedLocationsToNewIndices;
    };

    struct ReturnType {
        Model newModel;
        NewIndices newIndices;
    };
    ReturnType transform(std::string const& automatonName, std::string const& variableName);

    // Excludes a location from the expansion process -- it will not be duplicated for every value in the variable domain. This only works if the
    // location has no outgoing edges. It may be useful to exclude sink locations using this method to reduce the number of locations in the resulting
    // model.
    void excludeLocation(uint64_t index);

   private:
    Model const& original;
    Model newModel;

    std::set<uint64_t> excludedLocations;

    struct AutomatonAndIndices {
        Automaton newAutomaton;
        NewIndices newIndices;
    };
    AutomatonAndIndices transformAutomaton(Automaton const& automaton, std::string const& variableName, bool useTransientVariables = true);
};
}  // namespace jani

}  // namespace storm
