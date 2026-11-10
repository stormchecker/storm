#pragma once

#include <boost/any.hpp>

#include "storm/languages/jani/traverser/JaniTraverser.h"

namespace storm {

namespace expressions {
class Variable;
}

namespace jani {
class AssignmentsFinder : public ConstJaniTraverser {
   public:
    struct ResultType {
        bool hasLocationAssignment, hasEdgeAssignment, hasEdgeDestinationAssignment;
    };

    AssignmentsFinder() = default;

    ResultType find(Model const& model, storm::jani::Variable const& variable);
    ResultType find(Automaton const& automaton, storm::jani::Variable const& variable);
    ResultType find(Model const& model, storm::expressions::Variable const& variable);
    ResultType find(Automaton const& automaton, storm::expressions::Variable const& variable);

    virtual ~AssignmentsFinder() = default;

    virtual void traverse(Location const& location, boost::any const& data) override;
    virtual void traverse(TemplateEdge const& templateEdge, boost::any const& data) override;
    virtual void traverse(TemplateEdgeDestination const& templateEdgeDestination, boost::any const& data) override;
};
}  // namespace jani
}  // namespace storm
