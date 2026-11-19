#pragma once

#include <array>
#include <string>

#include "storm/solver/SolverRequirement.h"

namespace storm::solver {

/*!
 * Features that a caller requires from an LP solver.
 */
class LpSolverRequirements {
   public:
    // The different requirements a solver can have.
    enum class Element {
        // Support for integer and binary variables (MILP).
        IntegerVariables,
        // Support for incremental solving via push() and pop().
        Incremental,
        // Support for indicator constraints.
        IndicatorConstraints,
        // Exact support for strict inequalities (< and >).
        // Some solvers accept them but only approximate them by shifting the bound by a small margin; these do not count as supporting this feature.
        StrictRelations,
        // Support for exact computations in rational arithmetic.
        // Excludes solvers that convert the values to floating point numbers internally.
        ExactArithmetic
    };

    /*!
     * Returns all elements. Must list every Element.
     */
    static constexpr std::array<Element, 5> allElements() {
        return {Element::IntegerVariables, Element::Incremental, Element::IndicatorConstraints, Element::StrictRelations, Element::ExactArithmetic};
    }

    LpSolverRequirements() = default;

    /*!
     * Enables the requirement for the respective feature.
     * @param critical If false, the requirement is optional (nice to have). The requirement is enabled either way.
     */
    LpSolverRequirements& requireIntegerVariables(bool critical = true);
    LpSolverRequirements& requireIncremental(bool critical = true);
    LpSolverRequirements& requireIndicatorConstraints(bool critical = true);
    LpSolverRequirements& requireStrictRelations(bool critical = true);
    LpSolverRequirements& requireExactArithmetic(bool critical = true);
    LpSolverRequirements& require(Element const& element, bool critical = true);

    SolverRequirement const& integerVariables() const;
    SolverRequirement const& incremental() const;
    SolverRequirement const& indicatorConstraints() const;
    SolverRequirement const& strictRelations() const;
    SolverRequirement const& exactArithmetic() const;
    SolverRequirement const& get(Element const& element) const;

    bool hasEnabledRequirement() const;
    bool hasEnabledCriticalRequirement() const;

    std::string getEnabledRequirementsAsString() const;

   private:
    SolverRequirement integerVariablesRequirement;
    SolverRequirement incrementalRequirement;
    SolverRequirement indicatorConstraintsRequirement;
    SolverRequirement strictRelationsRequirement;
    SolverRequirement exactArithmeticRequirement;
};

}  // namespace storm::solver
