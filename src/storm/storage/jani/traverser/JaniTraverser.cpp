#include "JaniTraverser.h"

namespace storm {
namespace jani {

void JaniTraverser::traverse(Model& model, boost::any const& data) {
    for (auto& act : model.getActions()) {
        traverse(act, data);
    }
    for (auto& c : model.getConstants()) {
        traverse(c, data);
    }
    for (auto& f : model.getGlobalFunctionDefinitions()) {
        traverse(f.second, data);
    }
    traverse(model.getGlobalVariables(), data);
    for (auto& aut : model.getAutomata()) {
        traverse(aut, data);
    }
    if (model.hasInitialStatesRestriction()) {
        traverse(model.getInitialStatesRestriction(), data);
    }
    for (auto& nonTrivRew : model.getNonTrivialRewardExpressions()) {
        traverse(nonTrivRew.second, data);
    }
}

void JaniTraverser::traverse(Action const&, boost::any const&) {
    // Intentionally left empty.
}

void JaniTraverser::traverse(Automaton& automaton, boost::any const& data) {
    traverse(automaton.getVariables(), data);
    for (auto& f : automaton.getFunctionDefinitions()) {
        traverse(f.second, data);
    }
    for (auto& loc : automaton.getLocations()) {
        traverse(loc, data);
    }
    traverse(automaton.getEdgeContainer(), data);
    if (automaton.hasInitialStatesRestriction()) {
        traverse(automaton.getInitialStatesRestriction(), data);
    }
}

void JaniTraverser::traverse(Constant& constant, boost::any const& data) {
    if (constant.isDefined()) {
        traverse(constant.getExpression(), data);
    }
    if (constant.hasConstraint()) {
        traverse(constant.getConstraintExpression(), data);
    }
}

void JaniTraverser::traverse(FunctionDefinition& functionDefinition, boost::any const& data) {
    traverse(functionDefinition.getFunctionBody(), data);
}

void JaniTraverser::traverse(VariableSet& variableSet, boost::any const& data) {
    for (auto& v : variableSet) {
        traverse(v, data);
    }
}

void JaniTraverser::traverse(Location& location, boost::any const& data) {
    traverse(location.getAssignments(), data);
    if (location.hasTimeProgressInvariant()) {
        traverse(location.getTimeProgressInvariant(), data);
    }
}

void JaniTraverser::traverse(Variable& variable, boost::any const& data) {
    if (variable.hasInitExpression()) {
        traverse(variable.getInitExpression(), data);
    }
    traverse(variable.getType(), data);
}

void JaniTraverser::traverse(JaniType& type, boost::any const& data) {
    if (type.isBoundedType()) {
        auto& boundedType = type.asBoundedType();
        if (boundedType.hasLowerBound()) {
            traverse(boundedType.getLowerBound(), data);
        }
        if (boundedType.hasUpperBound()) {
            traverse(boundedType.getUpperBound(), data);
        }
    } else if (type.isArrayType()) {
        traverse(type.asArrayType().getBaseType(), data);
    }
}

void JaniTraverser::traverse(EdgeContainer& edgeContainer, boost::any const& data) {
    for (auto& templateEdge : edgeContainer.getTemplateEdges()) {
        traverse(*templateEdge, data);
    }
    for (auto& concreteEdge : edgeContainer.getConcreteEdges()) {
        traverse(concreteEdge, data);
    }
}

void JaniTraverser::traverse(TemplateEdge& templateEdge, boost::any const& data) {
    traverse(templateEdge.getGuard(), data);
    for (auto& dest : templateEdge.getDestinations()) {
        traverse(dest, data);
    }
    traverse(templateEdge.getAssignments(), data);
}

void JaniTraverser::traverse(TemplateEdgeDestination& templateEdgeDestination, boost::any const& data) {
    traverse(templateEdgeDestination.getOrderedAssignments(), data);
}

void JaniTraverser::traverse(Edge& edge, boost::any const& data) {
    if (edge.hasRate()) {
        traverse(edge.getRate(), data);
    }
    for (auto& dest : edge.getDestinations()) {
        traverse(dest, data);
    }
}

void JaniTraverser::traverse(EdgeDestination& edgeDestination, boost::any const& data) {
    traverse(edgeDestination.getProbability(), data);
}

void JaniTraverser::traverse(OrderedAssignments& orderedAssignments, boost::any const& data) {
    for (auto& assignment : orderedAssignments) {
        traverse(assignment, data);
    }
    STORM_LOG_ASSERT(orderedAssignments.checkOrder(), "Order of ordered assignment\n" << orderedAssignments << "\nhas been violated.");
}

void JaniTraverser::traverse(Assignment& assignment, boost::any const& data) {
    traverse(assignment.getAssignedExpression(), data);
    traverse(assignment.getLValue(), data);
}

void JaniTraverser::traverse(LValue& lValue, boost::any const& data) {
    if (lValue.isArrayAccess()) {
        for (auto& index : lValue.getArrayIndexVector()) {
            traverse(index, data);
        }
    }
}

void JaniTraverser::traverse(storm::expressions::Expression const&, boost::any const&) {
    // intentionally left empty.
}

void ConstJaniTraverser::traverse(Model const& model, boost::any const& data) {
    for (auto const& act : model.getActions()) {
        traverse(act, data);
    }
    for (auto const& c : model.getConstants()) {
        traverse(c, data);
    }
    for (auto const& f : model.getGlobalFunctionDefinitions()) {
        traverse(f.second, data);
    }
    traverse(model.getGlobalVariables(), data);
    for (auto const& aut : model.getAutomata()) {
        traverse(aut, data);
    }
    if (model.hasInitialStatesRestriction()) {
        traverse(model.getInitialStatesRestriction(), data);
    }
    for (auto const& nonTrivRew : model.getNonTrivialRewardExpressions()) {
        traverse(nonTrivRew.second, data);
    }
}

void ConstJaniTraverser::traverse(Action const&, boost::any const&) {
    // Intentionally left empty.
}

void ConstJaniTraverser::traverse(Automaton const& automaton, boost::any const& data) {
    traverse(automaton.getVariables(), data);
    for (auto const& f : automaton.getFunctionDefinitions()) {
        traverse(f.second, data);
    }
    for (auto const& loc : automaton.getLocations()) {
        traverse(loc, data);
    }
    traverse(automaton.getEdgeContainer(), data);
    if (automaton.hasInitialStatesRestriction()) {
        traverse(automaton.getInitialStatesRestriction(), data);
    }
}

void ConstJaniTraverser::traverse(Constant const& constant, boost::any const& data) {
    if (constant.isDefined()) {
        traverse(constant.getExpression(), data);
    }
    if (constant.hasConstraint()) {
        traverse(constant.getConstraintExpression(), data);
    }
}

void ConstJaniTraverser::traverse(FunctionDefinition const& functionDefinition, boost::any const& data) {
    traverse(functionDefinition.getFunctionBody(), data);
}

void ConstJaniTraverser::traverse(VariableSet const& variableSet, boost::any const& data) {
    for (auto const& v : variableSet) {
        traverse(v, data);
    }
}

void ConstJaniTraverser::traverse(Location const& location, boost::any const& data) {
    traverse(location.getAssignments(), data);
    if (location.hasTimeProgressInvariant()) {
        traverse(location.getTimeProgressInvariant(), data);
    }
}

void ConstJaniTraverser::traverse(Variable const& variable, boost::any const& data) {
    if (variable.hasInitExpression()) {
        traverse(variable.getInitExpression(), data);
    }
    traverse(variable.getType(), data);
}

void ConstJaniTraverser::traverse(JaniType const& type, boost::any const& data) {
    if (type.isBoundedType()) {
        auto const& boundedType = type.asBoundedType();
        if (boundedType.hasLowerBound()) {
            traverse(boundedType.getLowerBound(), data);
        }
        if (boundedType.hasUpperBound()) {
            traverse(boundedType.getUpperBound(), data);
        }
    } else if (type.isArrayType()) {
        traverse(type.asArrayType().getBaseType(), data);
    }
}

void ConstJaniTraverser::traverse(EdgeContainer const& edgeContainer, boost::any const& data) {
    for (auto const& templateEdge : edgeContainer.getTemplateEdges()) {
        traverse(*templateEdge, data);
    }
    for (auto const& concreteEdge : edgeContainer.getConcreteEdges()) {
        traverse(concreteEdge, data);
    }
}

void ConstJaniTraverser::traverse(TemplateEdge const& templateEdge, boost::any const& data) {
    traverse(templateEdge.getGuard(), data);
    for (auto const& dest : templateEdge.getDestinations()) {
        traverse(dest, data);
    }
    traverse(templateEdge.getAssignments(), data);
}

void ConstJaniTraverser::traverse(TemplateEdgeDestination const& templateEdgeDestination, boost::any const& data) {
    traverse(templateEdgeDestination.getOrderedAssignments(), data);
}

void ConstJaniTraverser::traverse(Edge const& edge, boost::any const& data) {
    if (edge.hasRate()) {
        traverse(edge.getRate(), data);
    }
    for (auto const& dest : edge.getDestinations()) {
        traverse(dest, data);
    }
}

void ConstJaniTraverser::traverse(EdgeDestination const& edgeDestination, boost::any const& data) {
    traverse(edgeDestination.getProbability(), data);
}

void ConstJaniTraverser::traverse(OrderedAssignments const& orderedAssignments, boost::any const& data) {
    for (auto const& assignment : orderedAssignments) {
        traverse(assignment, data);
    }
}

void ConstJaniTraverser::traverse(Assignment const& assignment, boost::any const& data) {
    traverse(assignment.getAssignedExpression(), data);
    traverse(assignment.getLValue(), data);
}

void ConstJaniTraverser::traverse(LValue const& lValue, boost::any const& data) {
    if (lValue.isArrayAccess()) {
        for (auto const& index : lValue.getArrayIndexVector()) {
            traverse(index, data);
        }
    }
}

void ConstJaniTraverser::traverse(storm::expressions::Expression const&, boost::any const&) {
    // intentionally left empty.
}

}  // namespace jani
}  // namespace storm
