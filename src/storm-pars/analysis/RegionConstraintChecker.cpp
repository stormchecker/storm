#include "storm-pars/analysis/RegionConstraintChecker.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/expressions/Expressions.h"
#include "storm/expressions/LinearCoefficientVisitor.h"
#include "storm/utility/constants.h"

namespace storm::pars::analysis {

namespace {

using RelationType = storm::expressions::RelationType;
using BaseExpression = storm::expressions::BaseExpression;
using BinaryRelationExpression = storm::expressions::BinaryRelationExpression;

using Form = storm::expressions::LinearCoefficientVisitor<CoefficientType>::VariableCoefficients;
using VariablesByName = std::map<std::string, RationalFunctionRegion::VariableType>;

// The exact extrema of an affine form over a region, with the points attaining them.
struct Extremes {
    CoefficientType minimum;
    CoefficientType maximum;
    RegionWitness minimizingPoint;
    RegionWitness maximizingPoint;
};

// An affine form is monotone in each variable, so its extrema are attained where each variable is at the bound given by the sign of its coefficient.
Extremes computeExtremes(Form const& form, VariablesByName const& variables, RationalFunctionRegion const& region) {
    std::set<RationalFunctionRegion::VariableType> increasing;
    std::set<RationalFunctionRegion::VariableType> decreasing;
    for (auto const& [variable, coefficient] : form) {
        (coefficient > storm::utility::zero<CoefficientType>() ? increasing : decreasing).insert(variables.at(variable.getName()));
    }
    RegionWitness minimizingPoint(region.getPoint(storm::solver::OptimizationDirection::Minimize, increasing, decreasing));
    RegionWitness maximizingPoint(region.getPoint(storm::solver::OptimizationDirection::Maximize, increasing, decreasing));

    auto evaluate = [&form, &variables](RegionWitness const& point) {
        CoefficientType value = form.getConstantPart();
        for (auto const& [variable, coefficient] : form) {
            value += coefficient * point.at(variables.at(variable.getName()));
        }
        return value;
    };
    return Extremes{evaluate(minimizingPoint), evaluate(maximizingPoint), std::move(minimizingPoint), std::move(maximizingPoint)};
}

// The outcome of checking a single constraint.
struct SingleVerdict {
    ConstraintVerdict verdict = ConstraintVerdict::Unknown;
    std::optional<RegionWitness> witness;
};

// Decides `f <relation> 0` from the extrema of the affine form f; constructs a witness if it fails.
SingleVerdict checkAffineConstraint(RelationType relation, Extremes const& extremes) {
    CoefficientType const zero = storm::utility::zero<CoefficientType>();

    bool satisfiedThroughout;
    switch (relation) {
        case RelationType::Less:
            satisfiedThroughout = extremes.maximum < zero;
            break;
        case RelationType::LessOrEqual:
            satisfiedThroughout = extremes.maximum <= zero;
            break;
        case RelationType::Greater:
            satisfiedThroughout = extremes.minimum > zero;
            break;
        case RelationType::GreaterOrEqual:
            satisfiedThroughout = extremes.minimum >= zero;
            break;
        case RelationType::Equal:
            satisfiedThroughout = extremes.minimum >= zero && extremes.maximum <= zero;
            break;
        case RelationType::NotEqual:
            satisfiedThroughout = extremes.maximum < zero || extremes.minimum > zero;
            break;
        default:
            return SingleVerdict{};
    }

    if (satisfiedThroughout) {
        return SingleVerdict{ConstraintVerdict::Sat, std::nullopt};
    }

    switch (relation) {
        case RelationType::Less:
        case RelationType::LessOrEqual:
            return SingleVerdict{ConstraintVerdict::Unsat, extremes.maximizingPoint};
        case RelationType::Greater:
        case RelationType::GreaterOrEqual:
            return SingleVerdict{ConstraintVerdict::Unsat, extremes.minimizingPoint};
        case RelationType::Equal:
            // The form is not identically zero, so one of the two extremizing points witnesses the violation.
            if (extremes.maximum != zero) {
                return SingleVerdict{ConstraintVerdict::Unsat, extremes.maximizingPoint};
            }
            return SingleVerdict{ConstraintVerdict::Unsat, extremes.minimizingPoint};
        case RelationType::NotEqual: {
            if (extremes.minimum == zero) {
                return SingleVerdict{ConstraintVerdict::Unsat, extremes.minimizingPoint};
            }
            if (extremes.maximum == zero) {
                return SingleVerdict{ConstraintVerdict::Unsat, extremes.maximizingPoint};
            }
            // minimum < 0 < maximum: f is zero at the interpolated point for t = minimum / (minimum - maximum).
            CoefficientType const t = extremes.minimum / (extremes.minimum - extremes.maximum);
            RegionWitness witness;
            for (auto const& [variable, value] : extremes.minimizingPoint) {
                witness.emplace(variable, value + t * (extremes.maximizingPoint.at(variable) - value));
            }
            return SingleVerdict{ConstraintVerdict::Unsat, std::move(witness)};
        }
        default:
            return SingleVerdict{};
    }
}

}  // namespace

std::ostream& operator<<(std::ostream& out, RegionWitness const& witness) {
    bool first = true;
    for (auto const& [variable, value] : witness) {
        out << (first ? "" : ", ") << variable.name() << " = " << value;
        first = false;
    }
    return out;
}

RegionConstraintChecker::RegionConstraintChecker(storm::pars::analysis::ConstraintCollector const& collector) : collector(collector) {}

RegionCheckResult RegionConstraintChecker::checkWellformedness(RationalFunctionRegion const& region) const {
    return checkConstraints(this->collector.getWellformedConstraints(), region, "well-formedness");
}

RegionCheckResult RegionConstraintChecker::checkGraphPreservation(RationalFunctionRegion const& region) const {
    // By definition, a model that is not well-formed is not graph-preserving, so the well-formedness constraints are part of graph preservation.
    auto constraints = this->collector.getWellformedConstraints();
    auto const& graphPreservingConstraints = this->collector.getGraphPreservingConstraints();
    constraints.insert(constraints.end(), graphPreservingConstraints.begin(), graphPreservingConstraints.end());
    return checkConstraints(constraints, region, "graph-preserving");
}

bool RegionConstraintChecker::isWellDefined(RationalFunctionRegion const& region) const {
    return checkWellformedness(region).holdsThroughout();
}

bool RegionConstraintChecker::isGraphPreserving(RationalFunctionRegion const& region) const {
    return checkGraphPreservation(region).holdsThroughout();
}

RegionCheckResult RegionConstraintChecker::checkConstraints(storm::pars::analysis::ConstraintCollector::ConstraintSet const& constraints,
                                                            RationalFunctionRegion const& region, std::string const& label) {
    RegionCheckResult result;
    VariablesByName variables;
    for (auto const& variable : region.getVariables()) {
        variables.emplace(variable.name(), variable);
    }
    storm::expressions::LinearCoefficientVisitor<CoefficientType> coefficientVisitor;
    bool violated = false;
    bool undecided = false;

    for (storm::expressions::Expression const& constraint : constraints) {
        BaseExpression const& baseExpression = constraint.getBaseExpression();

        SingleVerdict singleVerdict;
        if (baseExpression.isBooleanLiteralExpression()) {
            // The collector emits `false` for rejected constant entries, e.g. a fixed probability outside [0, 1].
            if (baseExpression.asBooleanLiteralExpression().getValue()) {
                singleVerdict = SingleVerdict{ConstraintVerdict::Sat, std::nullopt};
            } else {
                singleVerdict = SingleVerdict{ConstraintVerdict::Unsat, RegionWitness(region.getCenterPoint())};
            }
        } else if (baseExpression.isBinaryRelationExpression()) {
            BinaryRelationExpression const& relationExpression = baseExpression.asBinaryRelationExpression();
            auto form = coefficientVisitor.tryGetLinearCoefficients(constraint.getOperand(0));
            auto rhs = coefficientVisitor.tryGetLinearCoefficients(constraint.getOperand(1));

            if (form.has_value() && rhs.has_value()) {
                // The relation is compared against zero for the form of `lhs - rhs`.
                *form -= std::move(*rhs);

                for (auto const& [variable, coefficient] : *form) {
                    STORM_LOG_THROW(variables.count(variable.getName()) != 0, storm::exceptions::InvalidArgumentException,
                                    "Cannot check " << label << " constraint '" << constraint.toString()
                                                    << "' over the given region, because it constrains the variable '" << variable.getName()
                                                    << "', which is not part of the region. Please provide a region covering all constrained variables.");
                }

                Extremes const extremes = computeExtremes(*form, variables, region);
                singleVerdict = checkAffineConstraint(relationExpression.getRelationType(), extremes);
            }
        }

        switch (singleVerdict.verdict) {
            case ConstraintVerdict::Sat:
                break;
            case ConstraintVerdict::Unsat:
                violated = true;
                if (!result.violatedConstraint.has_value()) {
                    result.witness = singleVerdict.witness;
                    result.violatedConstraint = constraint;
                }
                break;
            case ConstraintVerdict::Unknown:
                undecided = true;
                result.undecidedConstraints.push_back(constraint);
                break;
        }
    }

    if (violated) {
        result.verdict = ConstraintVerdict::Unsat;
    } else if (undecided) {
        result.verdict = ConstraintVerdict::Unknown;
    } else {
        result.verdict = ConstraintVerdict::Sat;
    }

    return result;
}

}  // namespace storm::pars::analysis
