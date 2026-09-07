#pragma once

#include <set>
#include <string>
#include <vector>

#include "storm/storage/jani/Model.h"

// UnfoldDependencyGraph stores how variables depend on each other. For example, if an edge contains the assignment x=y+z, then x depends on y and z.
// Unfolding x is only possible if y and z are already unfolded. This graph models these dependencies. It also supports cyclical dependencies (which have
// to be unfolded together).
// The graph is usually construction once in the beginning of the elimination process and then updated whenever a variable is unfolded.

namespace storm {
namespace jani {
namespace elimination_actions {
class UnfoldDependencyGraph {
   public:
    class VariableInfo {
       public:
        std::string expressionVariableName;
        std::string janiVariableName;
        bool isGlobal;
        std::string automatonName;
        bool isConstBoundedInteger;
        int domainSize;

        VariableInfo(std::string expressionVariableName, std::string janiVariableName, bool isGlobal, std::string automatonName, bool isConstBoundedInteger,
                     int domainSize);
    };

    class VariableGroup {
       public:
        std::vector<VariableInfo> variables;
        uint32_t domainSize;
        bool allVariablesUnfoldable;
        bool unfolded;
        bool allDependenciesUnfolded;
        std::set<uint32_t> dependencies;

        VariableGroup();
        void addVariable(VariableInfo variable);
        std::string getVariablesAsString();
    };

    std::vector<VariableGroup> variableGroups;

    explicit UnfoldDependencyGraph(Model &model);

    void markUnfolded(uint32_t groupIndex);
    uint32_t findGroupIndex(std::string variableName);

    std::vector<uint32_t> getOrderedDependencies(uint32_t groupIndex, bool includeSelf = false);
    uint32_t getTotalBlowup(std::vector<uint32_t> groups);
    bool areDependenciesUnfoldable(uint32_t groupIndex);

    std::set<uint32_t> getGroupsWithNoDependencies();
    void printGroups();
    std::string toString();

   private:
    void buildGroups(Model &model);
};
}  // namespace elimination_actions
}  // namespace jani
}  // namespace storm
