#include "storm/storage/jani/localeliminator/AutomaticAction.h"

#include <boost/graph/strong_components.hpp>
#include "storm/exceptions/NotImplementedException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/localeliminator/EliminateAutomaticallyAction.h"
#include "storm/storage/jani/localeliminator/RebuildWithoutUnreachableAction.h"
#include "storm/storage/jani/localeliminator/UnfoldAction.h"

namespace storm {
namespace jani {
namespace elimination_actions {
AutomaticAction::AutomaticAction() : locationLimit(30), newTransitionLimit(20000), maxDomainSize(100), flatten(true) {}

AutomaticAction::AutomaticAction(uint64_t locationLimit, uint64_t newTransitionLimit, uint64_t maxDomainSize, bool flatten)
    : locationLimit(locationLimit), newTransitionLimit(newTransitionLimit), maxDomainSize(maxDomainSize), flatten(flatten) {}

std::string AutomaticAction::getDescription() {
    return "AutomaticAction";
}

void AutomaticAction::doAction(JaniLocalEliminator::Session &session) {
    if (flatten) {
        if (session.getModel().getAutomata().size() > 1) {
            STORM_LOG_TRACE("Flattening model");
            session.flatten_automata();
        }
        std::string const &autName = session.getModel().getAutomata()[0].getName();
        processAutomaton(session, autName);
    } else {
        for (uint64_t i = 0; i < session.getModel().getNumberOfAutomata(); i++) {
            std::string const &autName = session.getModel().getAutomata()[i].getName();
            STORM_LOG_TRACE("Processing automaton " + autName);
            processAutomaton(session, autName);
        }
    }

    STORM_LOG_TRACE("Finished automatic state-space reduction.");
    if (session.getModel().getNumberOfAutomata() == 1) {
        STORM_LOG_TRACE("Final model size: " + std::to_string(session.getModel().getAutomaton(0).getNumberOfEdges()) + " edges, " +
                        std::to_string(session.getModel().getAutomaton(0).getNumberOfLocations()) + " locations");
    }
}

void AutomaticAction::processAutomaton(JaniLocalEliminator::Session &session, const std::string &autName) {
    bool isOnlyAutomaton = session.getModel().getNumberOfAutomata() == 1;
    STORM_LOG_TRACE("Generating variable dependency graph");
    UnfoldDependencyGraph dependencyGraph(session.getModel());
    STORM_LOG_TRACE(dependencyGraph.toString());

    auto nextUnfold = chooseNextUnfold(session, autName, dependencyGraph, true);
    if (!nextUnfold) {
        STORM_LOG_TRACE("No property variable can be unfolded.");
        return;
    }
    unfoldGroupAndDependencies(session, autName, dependencyGraph, nextUnfold.get());
    STORM_LOG_TRACE("Performing automatic elimination");

    EliminateAutomaticallyAction eliminatePropertyAction(autName, EliminateAutomaticallyAction::EliminationOrder::NewTransitionCount, newTransitionLimit,
                                                         !isOnlyAutomaton);
    eliminatePropertyAction.doAction(session);

    RebuildWithoutUnreachableAction rebuildAfterPropertyAction;
    rebuildAfterPropertyAction.doAction(session);

    while (session.getModel().getAutomaton(0).getLocations().size() < locationLimit) {
        nextUnfold = chooseNextUnfold(session, autName, dependencyGraph, false);
        if (!nextUnfold) {
            break;
        }

        unfoldGroupAndDependencies(session, autName, dependencyGraph, nextUnfold.get());

        RebuildWithoutUnreachableAction rebuildAfterUnfoldingAction;
        rebuildAfterUnfoldingAction.doAction(session);

        EliminateAutomaticallyAction eliminateAction(autName, EliminateAutomaticallyAction::EliminationOrder::NewTransitionCount, newTransitionLimit,
                                                     !isOnlyAutomaton);
        eliminateAction.doAction(session);

        RebuildWithoutUnreachableAction rebuildAfterEliminationAction;
        rebuildAfterEliminationAction.doAction(session);
    }
}

void AutomaticAction::unfoldGroupAndDependencies(JaniLocalEliminator::Session &session, const std::string &autName, UnfoldDependencyGraph &dependencyGraph,
                                                 uint32_t groupIndex) {
    auto orderedDependencies = dependencyGraph.getOrderedDependencies(groupIndex, true);
    STORM_LOG_TRACE("Unfolding " + dependencyGraph.variableGroups[groupIndex].getVariablesAsString() + " and their dependencies");
    for (auto dependency : orderedDependencies) {
        auto variables = dependencyGraph.variableGroups[dependency].variables;
        STORM_LOG_THROW(variables.size() == 1, storm::exceptions::NotImplementedException,
                        "Unfolding variables with circular dependencies is currently not implemented.");
        for (const auto &variable : variables) {
            if (variable.isGlobal) {
                // We currently always have to specify an automaton name, regardless of whether the
                // variable is global or not. This isn't  really a problem, as there is just one automaton
                // due to the flattening done previously (and the name of that is stored in autName)
                STORM_LOG_TRACE("\tUnfolding global variable " + variable.janiVariableName);
                UnfoldAction unfoldAction(autName, variable.janiVariableName, variable.expressionVariableName);
                unfoldAction.doAction(session);
            } else {
                STORM_LOG_TRACE("\tUnfolding variable " + variable.janiVariableName + " (automaton: " + variable.automatonName + ")");
                UnfoldAction unfoldAction(variable.automatonName, variable.janiVariableName, variable.expressionVariableName);
                unfoldAction.doAction(session);
            }
        }
        dependencyGraph.markUnfolded(dependency);
    }
}

std::map<std::string, double> AutomaticAction::getAssignmentCountByVariable(JaniLocalEliminator::Session &session, std::string const &automatonName) {
    std::map<std::string, double> res;
    auto automaton = session.getModel().getAutomaton(automatonName);
    for (auto edge : automaton.getEdges()) {
        size_t numDest = edge.getNumberOfDestinations();

        // The factor is used to ensure all edges contribute equally. Otherwise, a single edge with hundreds of destinations may skew the scores
        // significantly and lead to a variable being unfolded that isn't used throughout the model (thus creating few eliminable locations)
        double factor = 1.0 / numDest;
        for (const auto &dest : edge.getDestinations()) {
            for (const auto &asg : dest.getOrderedAssignments()) {
                auto name = asg.getExpressionVariable().getName();
                if (res.count(name) == 0) {
                    res[name] = 0;
                }
                res[name] += factor;
            }
        }
    }
    return res;
}

boost::optional<uint32_t> AutomaticAction::chooseNextUnfold(JaniLocalEliminator::Session &session, std::string const &automatonName,
                                                            UnfoldDependencyGraph &dependencyGraph, bool onlyPropertyVariables) {
    std::map<std::string, double> variableOccurrenceCounts = getAssignmentCountByVariable(session, automatonName);

    auto propertyVariables = session.getProperty().getUsedVariablesAndConstants();
    STORM_LOG_TRACE("Choosing next unfold");
    if (onlyPropertyVariables) {
        STORM_LOG_TRACE("\tOnly groups containing a variable from the property will be considered");
    }
    STORM_LOG_TRACE(dependencyGraph.toString());

    std::set<uint32_t> groupsWithoutDependencies = dependencyGraph.getGroupsWithNoDependencies();

    STORM_LOG_TRACE("\tAnalysing groups without dependencies:");
    uint32_t bestValue = 0;
    uint32_t bestGroup = 0;
    for (auto groupIndex : groupsWithoutDependencies) {
        bool containsPropertyVariable = false;
        UnfoldDependencyGraph::VariableGroup &group = dependencyGraph.variableGroups[groupIndex];
        double totalOccurrences = 0;
        for (const auto &var : group.variables) {
            if (variableOccurrenceCounts.count(var.expressionVariableName) > 0) {
                totalOccurrences += variableOccurrenceCounts[var.expressionVariableName];
            }
            for (const auto &propertyVar : propertyVariables) {
                if (propertyVar.getName() == var.expressionVariableName) {
                    containsPropertyVariable = true;
                }
            }
        }
        if (onlyPropertyVariables && !containsPropertyVariable) {
            continue;
        }
        STORM_LOG_TRACE("\t\t{" + group.getVariablesAsString() + "}: " + std::to_string(totalOccurrences) + " occurrences");
        if (dependencyGraph.variableGroups[groupIndex].domainSize < maxDomainSize) {
            if (totalOccurrences > bestValue) {
                bestValue = totalOccurrences;
                bestGroup = groupIndex;
            }
        } else {
            STORM_LOG_TRACE("\t\t\tSkipped (domain size too large)");
        }
    }

    if (bestValue == 0) {
        STORM_LOG_TRACE("No unfoldable variable occurs in any edges.");
        return boost::none;
    }

    return bestGroup;
}
}  // namespace elimination_actions
}  // namespace jani
}  // namespace storm
