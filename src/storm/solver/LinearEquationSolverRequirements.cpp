#include "storm/solver/LinearEquationSolverRequirements.h"

#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

LinearEquationSolverRequirements::LinearEquationSolverRequirements() {
    // Intentionally left empty.
}

LinearEquationSolverRequirements& LinearEquationSolverRequirements::requireAcyclic(bool critical) {
    acyclicRequirement.enable(critical);
    return *this;
}

LinearEquationSolverRequirements& LinearEquationSolverRequirements::requireLowerBounds(bool critical) {
    lowerBoundsRequirement.enable(critical);
    return *this;
}

LinearEquationSolverRequirements& LinearEquationSolverRequirements::requireUpperBounds(bool critical) {
    upperBoundsRequirement.enable(critical);
    return *this;
}

LinearEquationSolverRequirements& LinearEquationSolverRequirements::requireBounds(bool critical) {
    requireLowerBounds(critical);
    requireUpperBounds(critical);
    return *this;
}

SolverRequirement const& LinearEquationSolverRequirements::acyclic() const {
    return acyclicRequirement;
}

SolverRequirement const& LinearEquationSolverRequirements::lowerBounds() const {
    return lowerBoundsRequirement;
}

SolverRequirement const& LinearEquationSolverRequirements::upperBounds() const {
    return upperBoundsRequirement;
}

SolverRequirement const& LinearEquationSolverRequirements::get(Element const& element) const {
    switch (element) {
        case Element::Acyclic:
            return acyclic();
        case Element::LowerBounds:
            return lowerBounds();
        case Element::UpperBounds:
            return upperBounds();
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Unknown ElementType.");
}

void LinearEquationSolverRequirements::clearAcyclic() {
    acyclicRequirement.clear();
}

void LinearEquationSolverRequirements::clearLowerBounds() {
    lowerBoundsRequirement.clear();
}

void LinearEquationSolverRequirements::clearUpperBounds() {
    upperBoundsRequirement.clear();
}

bool LinearEquationSolverRequirements::hasEnabledRequirement() const {
    return acyclicRequirement || lowerBoundsRequirement || upperBoundsRequirement;
}

bool LinearEquationSolverRequirements::hasEnabledCriticalRequirement() const {
    return acyclicRequirement.isCritical() || lowerBoundsRequirement.isCritical() || upperBoundsRequirement.isCritical();
}

std::string LinearEquationSolverRequirements::getEnabledRequirementsAsString() const {
    std::string res = "[";
    bool first = true;
    if (acyclic()) {
        if (!first) {
            res += ", ";
        } else {
            first = false;
        }
        res += "acyclic";
        if (acyclic().isCritical()) {
            res += "(mandatory)";
        }
    }
    if (lowerBounds()) {
        if (!first) {
            res += ", ";
        } else {
            first = false;
        }
        res += "lowerBounds";
        if (lowerBounds().isCritical()) {
            res += "(mandatory)";
        }
    }
    if (upperBounds()) {
        if (!first) {
            res += ", ";
        } else {
            first = false;
        }
        res += "upperBounds";
        if (upperBounds().isCritical()) {
            res += "(mandatory)";
        }
    }
    res += "]";
    return res;
}

}  // namespace solver
}  // namespace storm
