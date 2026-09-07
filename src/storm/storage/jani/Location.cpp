#include "Location.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidJaniException.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace jani {

Location::Location(std::string const& name, std::vector<Assignment> const& transientAssignments) : name(name), assignments(transientAssignments) {
    // Intentionally left empty.
}

Location::Location(std::string const& name, OrderedAssignments const& assignments) : name(name), assignments(assignments) {
    // Intentionally left empty.
}

std::string const& Location::getName() const {
    return name;
}

OrderedAssignments const& Location::getAssignments() const {
    return assignments;
}

OrderedAssignments& Location::getAssignments() {
    return assignments;
}

void Location::addTransientAssignment(storm::jani::Assignment const& assignment) {
    STORM_LOG_THROW(assignment.isTransient(), storm::exceptions::InvalidArgumentException, "Must not add non-transient assignment to location.");
    assignments.add(assignment);
}

bool Location::hasTimeProgressInvariant() const {
    return timeProgressInvariant.isInitialized();
}

storm::expressions::Expression const& Location::getTimeProgressInvariant() const {
    return timeProgressInvariant;
}

void Location::setTimeProgressInvariant(storm::expressions::Expression const& expression) {
    timeProgressInvariant = expression;
}

void Location::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                          bool const substituteTranscendentalNumbers) {
    for (auto& assignment : assignments) {
        assignment.substitute(substitution, substituteTranscendentalNumbers);
    }
    if (hasTimeProgressInvariant()) {
        setTimeProgressInvariant(substituteJaniExpression(getTimeProgressInvariant(), substitution, substituteTranscendentalNumbers));
    }
}

void Location::changeAssignmentVariables(std::map<Variable const*, std::reference_wrapper<Variable const>> const& remapping) {
    assignments.changeAssignmentVariables(remapping);
}

void Location::checkValid() const {
    // Intentionally left empty.
}

bool Location::isLinear() const {
    return assignments.areLinear();
}

}  // namespace jani
}  // namespace storm
