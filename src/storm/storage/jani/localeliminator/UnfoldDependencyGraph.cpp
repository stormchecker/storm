#include "UnfoldDependencyGraph.h"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/strong_components.hpp>
#include <utility>
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace jani {
namespace elimination_actions {
UnfoldDependencyGraph::VariableGroup::VariableGroup() : domainSize(1), allVariablesUnfoldable(true), unfolded(false), allDependenciesUnfolded(false) {}

void UnfoldDependencyGraph::VariableGroup::addVariable(UnfoldDependencyGraph::VariableInfo variable) {
    variables.push_back(variable);
    this->domainSize *= variable.domainSize;
    if (!variable.isConstBoundedInteger) {
        allVariablesUnfoldable = false;
    }
}

std::string UnfoldDependencyGraph::VariableGroup::getVariablesAsString() {
    std::string res = "";
    for (auto var : variables) {
        if (res != "")
            res += ", ";
        res += var.janiVariableName;
    }
    return res;
}

UnfoldDependencyGraph::UnfoldDependencyGraph(Model &model) {
    buildGroups(model);
}

void UnfoldDependencyGraph::markUnfolded(uint32_t groupIndex) {
    variableGroups[groupIndex].unfolded = true;

    // Now that one group has been unfolded, update which groups can be unfolded
    for (uint64_t i = 0; i < variableGroups.size(); i++) {
        if (variableGroups[i].allDependenciesUnfolded)
            continue;
        if (variableGroups[i].dependencies.count(i) != 0) {
            bool allUnfolded = true;
            for (uint32_t dep : variableGroups[i].dependencies) {
                if (!variableGroups[dep].unfolded)
                    allUnfolded = false;
            }
            variableGroups[i].allDependenciesUnfolded = allUnfolded;
        }
    }
}

uint32_t UnfoldDependencyGraph::findGroupIndex(std::string expressionVariableName) {
    for (uint32_t i = 0; i < variableGroups.size(); i++)
        for (auto variable : variableGroups[i].variables)
            if (variable.expressionVariableName == expressionVariableName)
                return i;

    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException,
                    "The UnfoldDependencyGraph does not contain the variable " + expressionVariableName + ".");
}

std::vector<uint32_t> UnfoldDependencyGraph::getOrderedDependencies(uint32_t groupIndex, bool includeSelf) {
    // This method creates a topological sort of all the dependencies of groupIndex.

    std::vector<uint32_t> res;
    std::set<uint32_t> closedSet;  // Contains the same vertices as res, but provides easier and faster lookup

    while (closedSet.count(groupIndex) == 0) {
        uint32_t current = groupIndex;
        bool isLeaf = false;
        while (!isLeaf) {
            isLeaf = true;
            // Find dependency that is not yet listed in res:
            for (auto dep : variableGroups[current].dependencies) {
                if (variableGroups[dep].unfolded)
                    continue;
                if (closedSet.count(dep) == 0 && current != dep) {
                    isLeaf = false;
                    current = dep;
                    break;
                }
            }
        }
        closedSet.insert(current);
        if (includeSelf || current != groupIndex)
            res.push_back(current);
    }

    return res;
}

uint32_t UnfoldDependencyGraph::getTotalBlowup(std::vector<uint32_t> groups) {
    uint32_t res = 1;
    for (uint32_t group : groups) {
        res *= variableGroups[group].domainSize;
    }
    return res;
}

std::set<uint32_t> UnfoldDependencyGraph::getGroupsWithNoDependencies() {
    std::set<uint32_t> res;
    for (uint64_t i = 0; i < variableGroups.size(); i++)
        if (!variableGroups[i].unfolded && variableGroups[i].allVariablesUnfoldable && variableGroups[i].allDependenciesUnfolded)
            res.insert(i);

    return res;
}

void UnfoldDependencyGraph::buildGroups(Model &model) {
    std::vector<std::pair<std::string, VariableSet *>> variableSets;
    // Use "" for the global set, as automata must not have an empty name according to the Jani spec
    variableSets.emplace_back("", &model.getGlobalVariables());
    for (auto &automaton : model.getAutomata()) variableSets.emplace_back(automaton.getName(), &automaton.getVariables());

    std::vector<VariableInfo> variables;

    for (const auto &variableSet : variableSets) {
        for (auto &var : *variableSet.second) {
            bool isConstBounded = false;
            if (var.getType().isBoundedType() && var.getType().asBoundedType().isIntegerType()) {
                auto biVar = var.getType().asBoundedType();
                if (biVar.hasLowerBound() && biVar.hasUpperBound() && !biVar.getLowerBound().containsVariables() &&
                    !biVar.getUpperBound().containsVariables()) {
                    int lowerBound = biVar.getLowerBound().evaluateAsInt();
                    int upperBound = biVar.getUpperBound().evaluateAsInt();

                    variables.emplace_back(var.getExpressionVariable().getName(), var.getName(), variableSet.first == "", variableSet.first, true,
                                           upperBound - lowerBound + 1);

                    isConstBounded = true;
                }
            } else if (var.getType().isBasicType() && var.getType().asBasicType().isBooleanType()) {
                variables.emplace_back(var.getExpressionVariable().getName(), var.getName(), variableSet.first == "", variableSet.first, true, 2);

                isConstBounded = true;
            }
            if (!isConstBounded) {
                // Still add the variable (so that dependencies are computed correctly). The
                // "isConstBoundedInteger" parameter ensures that this variable is not unfolded later
                variables.emplace_back(var.getExpressionVariable().getName(), var.getName(), variableSet.first == "", variableSet.first, false, 0);
            }
        }
    }

    boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS> graph(variables.size());

    for (auto &automaton : model.getAutomata()) {
        for (auto &janiEdge : automaton.getEdges()) {
            for (auto &dest : janiEdge.getDestinations()) {
                for (auto &asg : dest.getOrderedAssignments().getAllAssignments()) {
                    std::string leftName = asg.getExpressionVariable().getName();
                    uint64_t lIndex = std::distance(variables.begin(), std::find_if(variables.begin(), variables.end(), [leftName](VariableInfo &v) {
                                                        return v.expressionVariableName == leftName;
                                                    }));

                    for (auto &rightVar : asg.getAssignedExpression().getVariables()) {
                        const std::string &rightName = rightVar.getName();
                        uint64_t rIndex = std::distance(variables.begin(), std::find_if(variables.begin(), variables.end(), [rightName](VariableInfo &v) {
                                                            return v.expressionVariableName == rightName;
                                                        }));
                        if (rIndex == lIndex)
                            continue;
                        if (rIndex !=
                            variables.size()) {  // TODO If the condition is false, we're probably dealing with a constant. This should be handled properly.
                            if (!edge(lIndex, rIndex, graph).second) {
                                add_edge(lIndex, rIndex, graph);
                            }
                        }
                    }
                }
            }
        }
    }

    std::vector<int> sccs(variables.size());

    int num = strong_components(graph, boost::make_iterator_property_map(sccs.begin(), get(boost::vertex_index, graph), sccs[0]));

    for (int i = 0; i < num; i++) {
        variableGroups.emplace_back();
    }

    std::vector<int>::iterator i;
    for (i = sccs.begin(); i != sccs.end(); ++i) {
        int index = i - sccs.begin();
        int component = *i;
        variableGroups[component].addVariable(variables[index]);
    }

    for (uint64_t i = 0; i < graph.m_vertices.size(); i++) {
        for (const auto &outEdge : graph.m_vertices[i].m_out_edges) {
            unsigned long target = findGroupIndex(variables[outEdge.m_target].expressionVariableName);
            if (variableGroups[sccs[i]].dependencies.count(target) == 0) {
                variableGroups[sccs[i]].dependencies.insert(target);
            }
        }
    }

    for (uint64_t i = 0; i < variableGroups.size(); i++) {
        if (variableGroups[i].dependencies.empty()) {
            variableGroups[i].allDependenciesUnfolded = true;
        }
    }
}

UnfoldDependencyGraph::VariableInfo::VariableInfo(std::string expressionVariableName, std::string janiVariableName, bool isGlobal, std::string automatonName,
                                                  bool isConstBoundedInteger, int domainSize)
    : expressionVariableName(expressionVariableName),
      janiVariableName(janiVariableName),
      isGlobal(isGlobal),
      automatonName(automatonName),
      isConstBoundedInteger(isConstBoundedInteger),
      domainSize(domainSize) {}

void UnfoldDependencyGraph::printGroups() {
    int groupCounter = 0;
    for (const auto &group : variableGroups) {
        std::cout << "\nVariable Group " << groupCounter << '\n';
        if (group.allVariablesUnfoldable) {
            std::cout << "\tDomain size: " << group.domainSize << '\n';
            std::cout << "\tCan be unfolded\n";
        } else {
            std::cout << "\tCan not be unfolded\n";
        }
        std::cout << "\tVariables:\n";
        for (const auto &var : group.variables) {
            std::cout << "\t\t" << var.expressionVariableName;
            if (var.isConstBoundedInteger)
                std::cout << " (const-bounded integer with domain size " << var.domainSize << ")\n";
            else
                std::cout << " (not a const-bounded integer)\n";
        }
        if (group.dependencies.empty())
            std::cout << "\tNo Dependencies\n";
        else
            std::cout << "\tDependencies:\n";
        for (auto dep : group.dependencies) {
            std::cout << "\t\t" << dep << '\n';
        }

        groupCounter++;
    }
}

bool UnfoldDependencyGraph::areDependenciesUnfoldable(uint32_t groupIndex) {
    auto dependencies = getOrderedDependencies(groupIndex, false);
    return std::all_of(dependencies.begin(), dependencies.end(), [this](uint32_t dep) { return this->variableGroups[dep].allVariablesUnfoldable; });
}

std::string UnfoldDependencyGraph::toString() {
    std::string res;

    for (uint32_t i = 0; i < variableGroups.size(); i++) {
        auto group = variableGroups[i];
        std::vector<uint32_t> allDependencies = getOrderedDependencies(i, false);

        res += "{" + group.getVariablesAsString() + "}:";
        if (!group.dependencies.empty()) {
            res += "\n\tDepends on ";
            for (uint32_t dep : group.dependencies) res += "{" + variableGroups[dep].getVariablesAsString() + "}, ";
            if (allDependencies.size() > group.dependencies.size()) {
                res += "(";
                for (uint32_t dep : allDependencies) {
                    if (group.dependencies.count(dep) == 0)
                        res += "{" + variableGroups[dep].getVariablesAsString() + "}, ";
                }
                res = res.substr(0, res.length() - 2);  // Remove trailing comma
                res += ")";
            } else {
                res = res.substr(0, res.length() - 2);  // Remove trailing comma
            }
        }
        res += "\n\t";
        if (group.unfolded)
            res += "Unfolded\n";
        else if (group.allVariablesUnfoldable && areDependenciesUnfoldable(i))
            res += "Can be unfolded\n";
        else {
            res += "Can't be unfolded:\n";
            for (const auto &var : group.variables) {
                if (!var.isConstBoundedInteger)
                    res += "\t\tVariable " + var.expressionVariableName + " is not a const-bounded integer\n";
            }
            for (auto dep : allDependencies) {
                if (!variableGroups[dep].allVariablesUnfoldable) {
                    res += "\t\tDependency {" + variableGroups[dep].getVariablesAsString() + "} can't be unfolded\n";
                }
            }
        }
    }

    return res;
}
}  // namespace elimination_actions
}  // namespace jani
}  // namespace storm
