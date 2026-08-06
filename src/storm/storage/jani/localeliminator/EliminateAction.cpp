#include "EliminateAction.h"
#include <boost/format.hpp>
#include <boost/graph/adjacency_list.hpp>
#include "storm/exceptions/NotImplementedException.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace jani {
namespace elimination_actions {
EliminateAction::EliminateAction(const std::string& automatonName, const std::string& locationName) {
    this->automatonName = automatonName;
    this->locationName = locationName;
}

std::string EliminateAction::getDescription() {
    return (boost::format("EliminateAction (Automaton %s, Location %s)") % automatonName % locationName).str();
}

void EliminateAction::doAction(JaniLocalEliminator::Session& session) {
    STORM_LOG_THROW(!session.hasLoops(automatonName, locationName), storm::exceptions::InvalidArgumentException, "Locations with loops cannot be eliminated.");

    Automaton& automaton = session.getModel().getAutomaton(automatonName);
    uint64_t locIndex = automaton.getLocationIndex(locationName);

    // The basic idea is to iterate over the edges and eliminate those that are incident to the location.
    // There are a few complications:
    // - When we modify the edges, the edge container is rebuilt and we have to call automaton.getEdges()
    //   again instead of continuing with the current list of edges.
    // - Edges are ordered according to the index of the outgoing location. This means that if we add a new
    //   edge, that doesn't append it to the end of the edge list, but inserts it somewhere into the list.
    // - We only remove one destination per step. If an edge has two destinations leading to the location we
    //   want to eliminate, we eliminate one, add a new edge and then later visit the new edge to eliminate
    //   the other destination. This means that a single pass is not sufficient to perform elimination.
    // The basic idea is therefore to iterate over the edges until we've completed a full iteration without
    // performing any eliminations.

    uint64_t edgeIndex = 0;
    uint64_t stepsWithoutChange = 0;
    std::vector<Edge>& edges = automaton.getEdges();
    while (stepsWithoutChange <= edges.size()) {
        Edge edge = edges[edgeIndex];

        if (edge.getGuard().containsVariables() || edge.getGuard().evaluateAsBool()) {
            uint64_t destCount = edge.getNumberOfDestinations();
            for (uint64_t j = 0; j < destCount; j++) {
                const EdgeDestination& dest = edge.getDestination(j);
                if (dest.getLocationIndex() == locIndex) {
                    detail::Edges outgoingEdges = automaton.getEdgesFromLocation(locationName);
                    eliminateDestination(session, automaton, edges[edgeIndex], j, outgoingEdges);
                    // eliminateDestination adds new edges to the edge container, so we need to get the
                    // new list of edges:
                    edges = automaton.getEdges();
                    stepsWithoutChange = 0;
                    break;
                }
            }
        }

        edgeIndex++;
        stepsWithoutChange++;
        if (edgeIndex >= edges.size()) {
            edgeIndex -= edges.size();
        }
    }

    // The elimination is now complete. To make sure nothing went wrong, we go over all the edges one more
    // time. If any are still incident to the location we want to eliminate, something went wrong.
    for (Edge& edge : automaton.getEdges()) {
        if (!edge.getGuard().containsVariables() && !edge.getGuard().evaluateAsBool()) {
            continue;
        }
        for (const EdgeDestination& dest : edge.getDestinations()) {
            STORM_LOG_THROW(dest.getLocationIndex() != locIndex, storm::exceptions::IllegalArgumentException, "Could not eliminate location.");
        }
    }
}

void EliminateAction::eliminateDestination(JaniLocalEliminator::Session& session, Automaton& automaton, Edge& edge, const uint64_t destIndex,
                                           detail::Edges& outgoing) {
    uint64_t sourceIndex = edge.getSourceLocationIndex();
    uint64_t actionIndex = edge.getActionIndex();
    EdgeDestination dest = edge.getDestination(destIndex);

    std::vector<Edge>
        newEdges;  // Don't add the new edges immediately -- we cannot safely iterate over the outgoing edges while adding new edges to the structure

    for (Edge& outEdge : outgoing) {
        if (!outEdge.getGuard().containsVariables() && !outEdge.getGuard().evaluateAsBool()) {
            continue;
        }

        expressions::Expression newGuard = session.getNewGuard(edge, dest, outEdge);
        std::shared_ptr<storm::jani::TemplateEdge> templateEdge = std::make_shared<storm::jani::TemplateEdge>(newGuard);
        std::vector<std::pair<uint64_t, storm::expressions::Expression>> destinationLocationsAndProbabilities;
        for (const EdgeDestination& outDest : outEdge.getDestinations()) {
            expressions::Expression probability = session.getProbability(dest, outDest);

            OrderedAssignments oa = session.executeInSequence(dest, outDest, session.rewardModels);
            TemplateEdgeDestination templateEdgeDestination(oa);
            templateEdge->addDestination(templateEdgeDestination);

            destinationLocationsAndProbabilities.emplace_back(outDest.getLocationIndex(), probability);
        }

        // Add remaining destinations back to the edge:
        uint64_t destCount = edge.getNumberOfDestinations();
        for (uint64_t i = 0; i < destCount; i++) {
            if (i == destIndex) {
                continue;
            }
            const EdgeDestination& unchangedDest = edge.getDestination(i);
            OrderedAssignments oa(unchangedDest.getOrderedAssignments().clone());
            TemplateEdgeDestination templateEdgeDestination(oa);
            templateEdge->addDestination(templateEdgeDestination);
            destinationLocationsAndProbabilities.emplace_back(unchangedDest.getLocationIndex(), unchangedDest.getProbability());
        }

        STORM_LOG_THROW(!edge.hasRate() && !outEdge.hasRate(), storm::exceptions::NotImplementedException, "Edge Rates are not implemented.");
        newEdges.emplace_back(Edge(sourceIndex, actionIndex, boost::none, templateEdge, destinationLocationsAndProbabilities));
    }

    edge.setGuard(edge.getGuard().getManager().boolean(false));  // Instead of deleting the edge

    for (const Edge& newEdge : newEdges) {
        automaton.addEdge(newEdge);
    }
}
}  // namespace elimination_actions
}  // namespace jani
}  // namespace storm
