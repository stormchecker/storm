#pragma once

#include <boost/any.hpp>

#include "storm/languages/jani/Model.h"

namespace storm {
namespace jani {
class JaniTraverser {
   public:
    virtual ~JaniTraverser() = default;

    virtual void traverse(Model& model, boost::any const& data);

    virtual void traverse(Action const& action, boost::any const& data);
    virtual void traverse(Automaton& automaton, boost::any const& data);
    virtual void traverse(Constant& constant, boost::any const& data);
    virtual void traverse(FunctionDefinition& functionDefinition, boost::any const& data);
    virtual void traverse(VariableSet& variableSet, boost::any const& data);
    virtual void traverse(Location& location, boost::any const& data);
    virtual void traverse(Variable& variable, boost::any const& data);
    virtual void traverse(JaniType& type, boost::any const& data);
    virtual void traverse(EdgeContainer& edgeContainer, boost::any const& data);
    virtual void traverse(TemplateEdge& templateEdge, boost::any const& data);
    virtual void traverse(TemplateEdgeDestination& templateEdgeDestination, boost::any const& data);
    virtual void traverse(Edge& edge, boost::any const& data);
    virtual void traverse(EdgeDestination& edgeDestination, boost::any const& data);
    virtual void traverse(OrderedAssignments& orderedAssignments, boost::any const& data);
    virtual void traverse(Assignment& assignment, boost::any const& data);
    virtual void traverse(LValue& lValue, boost::any const& data);
    virtual void traverse(storm::expressions::Expression const& expression, boost::any const& data);
};

class ConstJaniTraverser {
   public:
    virtual ~ConstJaniTraverser() = default;

    virtual void traverse(Model const& model, boost::any const& data);

    virtual void traverse(Action const& action, boost::any const& data);
    virtual void traverse(Automaton const& automaton, boost::any const& data);
    virtual void traverse(Constant const& constant, boost::any const& data);
    virtual void traverse(FunctionDefinition const& functionDefinition, boost::any const& data);
    virtual void traverse(VariableSet const& variableSet, boost::any const& data);
    virtual void traverse(Location const& location, boost::any const& data);
    virtual void traverse(Variable const& variable, boost::any const& data);
    virtual void traverse(JaniType const& type, boost::any const& data);
    virtual void traverse(EdgeContainer const& edgeContainer, boost::any const& data);
    virtual void traverse(TemplateEdge const& templateEdge, boost::any const& data);
    virtual void traverse(TemplateEdgeDestination const& templateEdgeDestination, boost::any const& data);
    virtual void traverse(Edge const& edge, boost::any const& data);
    virtual void traverse(EdgeDestination const& edgeDestination, boost::any const& data);
    virtual void traverse(OrderedAssignments const& orderedAssignments, boost::any const& data);
    virtual void traverse(Assignment const& assignment, boost::any const& data);
    virtual void traverse(LValue const& lValue, boost::any const& data);
    virtual void traverse(storm::expressions::Expression const& expression, boost::any const& data);
};
}  // namespace jani
}  // namespace storm
