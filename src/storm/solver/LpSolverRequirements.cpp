#include "LpSolverRequirements.h"

#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/utility/macros.h"

namespace storm::solver {

LpSolverRequirements& LpSolverRequirements::requireIntegerVariables(bool critical) {
    integerVariablesRequirement.enable(critical);
    return *this;
}

LpSolverRequirements& LpSolverRequirements::requireIncremental(bool critical) {
    incrementalRequirement.enable(critical);
    return *this;
}

LpSolverRequirements& LpSolverRequirements::requireIndicatorConstraints(bool critical) {
    indicatorConstraintsRequirement.enable(critical);
    return *this;
}

LpSolverRequirements& LpSolverRequirements::requireStrictRelations(bool critical) {
    strictRelationsRequirement.enable(critical);
    return *this;
}

LpSolverRequirements& LpSolverRequirements::require(Element const& element, bool critical) {
    switch (element) {
        case Element::IntegerVariables:
            return requireIntegerVariables(critical);
        case Element::Incremental:
            return requireIncremental(critical);
        case Element::IndicatorConstraints:
            return requireIndicatorConstraints(critical);
        case Element::StrictRelations:
            return requireStrictRelations(critical);
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Unknown Element type.");
}

SolverRequirement const& LpSolverRequirements::integerVariables() const {
    return integerVariablesRequirement;
}

SolverRequirement const& LpSolverRequirements::incremental() const {
    return incrementalRequirement;
}

SolverRequirement const& LpSolverRequirements::indicatorConstraints() const {
    return indicatorConstraintsRequirement;
}

SolverRequirement const& LpSolverRequirements::strictRelations() const {
    return strictRelationsRequirement;
}

SolverRequirement const& LpSolverRequirements::get(Element const& element) const {
    switch (element) {
        case Element::IntegerVariables:
            return integerVariables();
        case Element::Incremental:
            return incremental();
        case Element::IndicatorConstraints:
            return indicatorConstraints();
        case Element::StrictRelations:
            return strictRelations();
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Unknown Element type.");
}

bool LpSolverRequirements::hasEnabledRequirement() const {
    return integerVariablesRequirement || incrementalRequirement || indicatorConstraintsRequirement || strictRelationsRequirement;
}

bool LpSolverRequirements::hasEnabledCriticalRequirement() const {
    return integerVariablesRequirement.isCritical() || incrementalRequirement.isCritical() || indicatorConstraintsRequirement.isCritical() ||
           strictRelationsRequirement.isCritical();
}

std::string LpSolverRequirements::getEnabledRequirementsAsString() const {
    std::string res = "[";
    bool first = true;
    auto append = [&res, &first](SolverRequirement const& requirement, std::string const& name) {
        if (requirement) {
            if (!first) {
                res += ", ";
            }
            first = false;
            res += name;
            if (requirement.isCritical()) {
                res += "(mandatory)";
            }
        }
    };
    append(integerVariables(), "integerVariables");
    append(incremental(), "incremental");
    append(indicatorConstraints(), "indicatorConstraints");
    append(strictRelations(), "strictRelations");
    res += "]";
    return res;
}

}  // namespace storm::solver
