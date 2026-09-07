#include "AssignmentsFinder.h"

#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace jani {

AssignmentsFinder::ResultType AssignmentsFinder::find(Model const& model, storm::jani::Variable const& variable) {
    return find(model, variable.getExpressionVariable());
}

AssignmentsFinder::ResultType AssignmentsFinder::find(Automaton const& automaton, storm::jani::Variable const& variable) {
    return find(automaton, variable.getExpressionVariable());
}

AssignmentsFinder::ResultType AssignmentsFinder::find(Model const& model, storm::expressions::Variable const& variable) {
    ResultType res;
    res.hasLocationAssignment = false;
    res.hasEdgeAssignment = false;
    res.hasEdgeDestinationAssignment = false;
    ConstJaniTraverser::traverse(model, std::make_pair(&variable, &res));
    return res;
}

AssignmentsFinder::ResultType AssignmentsFinder::find(Automaton const& automaton, storm::expressions::Variable const& variable) {
    ResultType res;
    res.hasLocationAssignment = false;
    res.hasEdgeAssignment = false;
    res.hasEdgeDestinationAssignment = false;
    ConstJaniTraverser::traverse(automaton, std::make_pair(&variable, &res));
    return res;
}

void AssignmentsFinder::traverse(Location const& location, boost::any const& data) {
    auto resVar = boost::any_cast<std::pair<storm::expressions::Variable const*, ResultType*>>(data);
    if (!resVar.second->hasLocationAssignment) {
        for (auto const& assignment : location.getAssignments()) {
            storm::jani::Variable const& assignedVariable = assignment.getVariable();
            if (assignedVariable.getExpressionVariable() == *resVar.first) {
                resVar.second->hasLocationAssignment = true;
                break;
            }
        }
    }
}

void AssignmentsFinder::traverse(TemplateEdge const& templateEdge, boost::any const& data) {
    auto resVar = boost::any_cast<std::pair<storm::expressions::Variable const*, ResultType*>>(data);
    if (!resVar.second->hasEdgeAssignment) {
        for (auto const& assignment : templateEdge.getAssignments()) {
            storm::jani::Variable const& assignedVariable = assignment.getVariable();
            if (assignedVariable.getExpressionVariable() == *resVar.first) {
                resVar.second->hasEdgeAssignment = true;
                break;
            }
        }
    }
    for (auto const& dest : templateEdge.getDestinations()) {
        traverse(dest, data);
    }
}

void AssignmentsFinder::traverse(TemplateEdgeDestination const& templateEdgeDestination, boost::any const& data) {
    auto resVar = boost::any_cast<std::pair<storm::expressions::Variable const*, ResultType*>>(data);
    if (!resVar.second->hasEdgeDestinationAssignment) {
        for (auto const& assignment : templateEdgeDestination.getOrderedAssignments()) {
            storm::jani::Variable const& assignedVariable = assignment.getVariable();
            if (assignedVariable.getExpressionVariable() == *resVar.first) {
                resVar.second->hasEdgeDestinationAssignment = true;
                break;
            }
        }
    }
}
}  // namespace jani
}  // namespace storm
