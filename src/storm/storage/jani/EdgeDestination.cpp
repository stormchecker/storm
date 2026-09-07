#include "EdgeDestination.h"

#include "storm/exceptions/WrongFormatException.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace jani {

EdgeDestination::EdgeDestination(uint64_t locationIndex, storm::expressions::Expression const& probability,
                                 TemplateEdgeDestination const& templateEdgeDestination)
    : locationIndex(locationIndex), probability(probability), templateEdgeDestination(templateEdgeDestination) {
    // Intentionally left empty.
}

uint64_t EdgeDestination::getLocationIndex() const {
    return locationIndex;
}

storm::expressions::Expression const& EdgeDestination::getProbability() const {
    return probability;
}

void EdgeDestination::setProbability(storm::expressions::Expression const& probability) {
    this->probability = probability;
}

std::map<storm::expressions::Variable, storm::expressions::Expression> EdgeDestination::getAsVariableToExpressionMap() const {
    std::map<storm::expressions::Variable, storm::expressions::Expression> result;

    for (auto const& assignment : this->getOrderedAssignments()) {
        result[assignment.getExpressionVariable()] = assignment.getAssignedExpression();
    }

    return result;
}

OrderedAssignments const& EdgeDestination::getOrderedAssignments() const {
    return templateEdgeDestination.get().getOrderedAssignments();
}

void EdgeDestination::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                                 bool const substituteTranscendentalNumbers) {
    this->setProbability(substituteJaniExpression(this->getProbability(), substitution, substituteTranscendentalNumbers));
}

bool EdgeDestination::hasAssignment(Assignment const& assignment) const {
    return this->getOrderedAssignments().contains(assignment);
}

bool EdgeDestination::hasTransientAssignment() const {
    return !this->getOrderedAssignments().getTransientAssignments().empty();
}

bool EdgeDestination::usesAssignmentLevels() const {
    if (this->getOrderedAssignments().empty()) {
        return false;
    }
    return this->getOrderedAssignments().getLowestLevel() != 0 || this->getOrderedAssignments().getHighestLevel() != 0;
}

TemplateEdgeDestination const& EdgeDestination::getTemplateEdgeDestination() const {
    return templateEdgeDestination.get();
}

void EdgeDestination::updateTemplateEdgeDestination(TemplateEdgeDestination const& newTed) {
    templateEdgeDestination = newTed;
}
}  // namespace jani
}  // namespace storm
