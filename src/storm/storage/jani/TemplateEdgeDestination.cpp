#include "TemplateEdgeDestination.h"

namespace storm {
namespace jani {

TemplateEdgeDestination::TemplateEdgeDestination(OrderedAssignments const& assignments) : assignments(assignments) {
    // Intentionally left empty.
}

TemplateEdgeDestination::TemplateEdgeDestination(Assignment const& assignment) : assignments(assignment) {
    // Intentionally left empty.
}

TemplateEdgeDestination::TemplateEdgeDestination(std::vector<Assignment> const& assignments) : assignments(assignments) {
    // Intentionally left empty.
}

void TemplateEdgeDestination::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                                         bool const substituteTranscendentalNumbers) {
    assignments.substitute(substitution, substituteTranscendentalNumbers);
}

void TemplateEdgeDestination::changeAssignmentVariables(std::map<Variable const*, std::reference_wrapper<Variable const>> const& remapping) {
    assignments.changeAssignmentVariables(remapping);
}

OrderedAssignments const& TemplateEdgeDestination::getOrderedAssignments() const {
    return assignments;
}

OrderedAssignments& TemplateEdgeDestination::getOrderedAssignments() {
    return assignments;
}

bool TemplateEdgeDestination::removeAssignment(Assignment const& assignment) {
    return assignments.remove(assignment);
}

void TemplateEdgeDestination::addAssignment(Assignment const& assignment, bool addToExisting) {
    assignments.add(assignment, addToExisting);
}

bool TemplateEdgeDestination::hasAssignment(Assignment const& assignment) const {
    return assignments.contains(assignment);
}

bool TemplateEdgeDestination::hasTransientAssignment() const {
    return assignments.hasTransientAssignment();
}

bool TemplateEdgeDestination::usesAssignmentLevels(bool onlyTransient) const {
    return assignments.hasMultipleLevels(onlyTransient);
}

bool TemplateEdgeDestination::isLinear() const {
    return assignments.areLinear();
}

bool TemplateEdgeDestination::hasAssignments() const {
    return !(assignments.empty());
}

TemplateEdgeDestination TemplateEdgeDestination::simplifyIndexedAssignments(bool syncronized, VariableSet const& localVars) const {
    return TemplateEdgeDestination(assignments.simplifyLevels(syncronized, localVars));
}
}  // namespace jani
}  // namespace storm
