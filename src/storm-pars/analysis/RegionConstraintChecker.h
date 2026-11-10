#pragma once

#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "storm-pars/analysis/ConstraintCollector.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm/expressions/Expression.h"
#include "storm/solver/SmtSolver.h"

namespace storm::pars::analysis {

using RationalFunctionRegion = storm::storage::ParameterRegion<storm::RationalFunction>;
using CoefficientType = RationalFunctionRegion::CoefficientType;

/*!
 * Tri-state outcome of a check: `Sat` means the constraint holds on the whole region, `Unsat` means it is
 * violated somewhere (a witness is available), and `Unknown` means neither could be established.
 */
using ConstraintVerdict = storm::solver::SmtSolver::CheckResult;

/*!
 * A point of a parameter region, mapping the variables to exact values.
 */
struct RegionWitness : RationalFunctionRegion::Valuation {
    RegionWitness() = default;
    explicit RegionWitness(RationalFunctionRegion::Valuation valuation) : RationalFunctionRegion::Valuation(std::move(valuation)) {}
};

/*!
 * Prints the witness as a comma-separated list of assignments, e.g. `x = 1/2, y = 1`.
 */
std::ostream& operator<<(std::ostream& out, RegionWitness const& witness);

struct RegionCheckResult {
    ConstraintVerdict verdict = ConstraintVerdict::Unknown;

    /*! A point at which a constraint is violated. Only set if the verdict is Unsat. */
    std::optional<RegionWitness> witness;

    /*! The constraint violated at the witness. Only set if the verdict is Unsat. */
    std::optional<storm::expressions::Expression> violatedConstraint;

    /*! All constraints that could not be decided. */
    std::vector<storm::expressions::Expression> undecidedConstraints;

    bool holdsThroughout() const {
        return verdict == ConstraintVerdict::Sat;
    }
};

/*!
 * Checks whether the constraints collected for a parametric model hold over an entire rectangular region.
 *
 * Affine constraints are decided exactly from the extrema of the affine form, which lie at the vertices of
 * the region. Non-affine constraints are not supported and are reported as undecided.
 */
class RegionConstraintChecker {
   public:
    explicit RegionConstraintChecker(storm::pars::analysis::ConstraintCollector const& collector);

    RegionCheckResult checkWellformedness(RationalFunctionRegion const& region) const;

    /*! Checks the graph-preserving constraints together with the well-formedness constraints, as a model that is not well-formed is not graph-preserving. */
    RegionCheckResult checkGraphPreservation(RationalFunctionRegion const& region) const;

    /*! Returns true iff all well-formedness constraints were proven to hold throughout the region. */
    bool isWellDefined(RationalFunctionRegion const& region) const;

    /*! Returns true iff all graph-preserving and well-formedness constraints were proven to hold throughout the region. */
    bool isGraphPreserving(RationalFunctionRegion const& region) const;

    /*!
     * Checks an arbitrary constraint set over a region. The label is used in exception messages.
     */
    static RegionCheckResult checkConstraints(storm::pars::analysis::ConstraintCollector::ConstraintSet const& constraints,
                                              RationalFunctionRegion const& region, std::string const& label);

   private:
    storm::pars::analysis::ConstraintCollector const& collector;
};

}  // namespace storm::pars::analysis
