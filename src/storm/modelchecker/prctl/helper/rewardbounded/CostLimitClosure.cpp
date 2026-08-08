#include "storm/modelchecker/prctl/helper/rewardbounded/CostLimitClosure.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/solver/SmtSolver.h"
#include "storm/utility/macros.h"
#include "storm/utility/solver.h"

namespace storm {
namespace modelchecker {
namespace helper {
namespace rewardbounded {
CostLimit::CostLimit(uint64_t const& costLimit) : value(costLimit) {
    // Intentionally left empty
}

bool CostLimit::isInfinity() const {
    return value == std::numeric_limits<uint64_t>::max();
}

uint64_t const& CostLimit::get() const {
    STORM_LOG_ASSERT(!isInfinity(), "Tried to get an infinite cost limit as int.");
    return value;
}

uint64_t& CostLimit::get() {
    STORM_LOG_ASSERT(!isInfinity(), "Tried to get an infinite cost limit as int.");
    return value;
}

bool CostLimit::operator<(CostLimit const& other) const {
    // Since infinity is represented by the max integer, we can compare this way.
    return value < other.value;
}

bool CostLimit::operator==(CostLimit const& other) const {
    return value == other.value;
}

CostLimit CostLimit::infinity() {
    return CostLimit(std::numeric_limits<uint64_t>::max());
}

bool CostLimitClosure::CostLimitsCompare::operator()(storm::modelchecker::helper::rewardbounded::CostLimits const& lhs,
                                                     storm::modelchecker::helper::rewardbounded::CostLimits const& rhs) const {
    for (uint64_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i] < rhs[i]) {
            return true;
        } else if (rhs[i] < lhs[i]) {
            return false;
        }
    }
    return false;
}

CostLimitClosure::CostLimitClosure(storm::storage::BitVector const& downwardDimensions) : downwardDimensions(downwardDimensions) {
    // Intentionally left empty
}

bool CostLimitClosure::insert(CostLimits const& costLimits) {
    // Iterate over all points in the generator and check whether they dominate the given point or vice versa
    // TODO: make this more efficient by exploiting the order of the generator set.
    std::vector<CostLimits> pointsToErase;
    for (auto const& b : generator) {
        if (dominates(b, costLimits)) {
            // The given point is already contained in this closure.
            // Since domination is transitive, this should not happen:
            STORM_LOG_ASSERT(pointsToErase.empty(), "Inconsistent generator of CostLimitClosure.");
            return false;
        }
        if (dominates(costLimits, b)) {
            // b will be dominated by the new point so we erase it later.
            // Note that b != newPoint holds if we reach this point
            pointsToErase.push_back(b);
        }
    }
    for (auto const& b : pointsToErase) {
        generator.erase(b);
    }
    generator.insert(std::move(costLimits));
    return true;
}

bool CostLimitClosure::contains(CostLimits const& costLimits) const {
    // Iterate over all points in the generator and check whether they dominate the given point.
    // TODO: make this more efficient by exploiting the order of the generator set.
    for (auto const& b : generator) {
        if (dominates(b, costLimits)) {
            return true;
        }
    }
    return false;
}

bool CostLimitClosure::containsUpwardClosure(CostLimits const& costLimits) const {
    CostLimits infinityProjection(costLimits);
    for (uint64_t dim : downwardDimensions) {
        infinityProjection[dim] = CostLimit::infinity();
    }
    return contains(infinityProjection);
}

bool CostLimitClosure::empty() const {
    return generator.empty();
}

bool CostLimitClosure::full() const {
    CostLimits p(dimension(), CostLimit(0));
    for (uint64_t dim : downwardDimensions) {
        p[dim] = CostLimit::infinity();
    }
    return contains(p);
}

bool CostLimitClosure::dominates(CostLimits const& lhs, CostLimits const& rhs) const {
    for (uint64_t i = 0; i < lhs.size(); ++i) {
        if (downwardDimensions.get(i)) {
            if (lhs[i] < rhs[i]) {
                return false;
            }
        } else {
            if (rhs[i] < lhs[i]) {
                return false;
            }
        }
    }
    return true;
}

std::vector<CostLimits> CostLimitClosure::getDominatingCostLimits(CostLimits const& costLimits) const {
    std::vector<CostLimits> result;
    for (auto const& b : generator) {
        if (dominates(b, costLimits)) {
            result.push_back(b);
        }
    }
    return result;
}

typename CostLimitClosure::GeneratorType const& CostLimitClosure::getGenerator() const {
    return generator;
}

uint64_t CostLimitClosure::dimension() const {
    return downwardDimensions.size();
}

bool CostLimitClosure::unionFull(CostLimitClosure const& first, CostLimitClosure const& second) {
    assert(first.dimension() == second.dimension());
    uint64_t dimension = first.dimension();
    auto manager = std::make_shared<storm::expressions::ExpressionManager>();
    auto solver = storm::utility::solver::getSmtSolver(*manager);

    std::vector<storm::expressions::Expression> point;
    storm::expressions::Expression zero = manager->integer(0);
    for (uint64_t i = 0; i < dimension; ++i) {
        point.push_back(manager->declareIntegerVariable("x" + std::to_string(i)).getExpression());
        solver->add(point.back() >= zero);
    }
    for (auto const& cl : {first, second}) {
        for (auto const& q : cl.getGenerator()) {
            storm::expressions::Expression pointNotDominated;
            for (uint64_t i = 0; i < point.size(); ++i) {
                if (!cl.downwardDimensions.get(i) || !q[i].isInfinity()) {
                    assert(!q[i].isInfinity());
                    storm::expressions::Expression qi = manager->integer(q[i].get());
                    storm::expressions::Expression piNotDominated = cl.downwardDimensions.get(i) ? point[i] > qi : point[i] < qi;
                    if (piNotDominated.isInitialized()) {
                        pointNotDominated = pointNotDominated || piNotDominated;
                    } else {
                        pointNotDominated = piNotDominated;
                    }
                }
            }
            if (pointNotDominated.isInitialized()) {
                solver->add(pointNotDominated);
            } else {
                solver->add(manager->boolean(false));
            }
        }
    }
    return solver->check() == storm::solver::SmtSolver::CheckResult::Unsat;
}
}  // namespace rewardbounded
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm