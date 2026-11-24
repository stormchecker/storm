#include "storm/environment/solver/TimeBoundedSolverEnvironment.h"

#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {

TimeBoundedSolverEnvironment::TimeBoundedSolverEnvironment()
    : maMethod(storm::solver::MaBoundedReachabilityMethod::UnifPlus),
      maMethodSetFromDefault(true),
      precision(storm::utility::convertNumber<storm::RationalNumber>(1e-06)),
      relative(true),
      unifPlusKappa(storm::utility::convertNumber<storm::RationalNumber>(0.05)) {
    // Intentionally left empty.
}

TimeBoundedSolverEnvironment::~TimeBoundedSolverEnvironment() {
    // Intentionally left empty
}

storm::solver::MaBoundedReachabilityMethod const& TimeBoundedSolverEnvironment::getMaMethod() const {
    return maMethod;
}

bool const& TimeBoundedSolverEnvironment::isMaMethodSetFromDefault() const {
    return maMethodSetFromDefault;
}

void TimeBoundedSolverEnvironment::setMaMethod(storm::solver::MaBoundedReachabilityMethod value, bool isSetFromDefault) {
    maMethod = value;
    maMethodSetFromDefault = isSetFromDefault;
}

storm::RationalNumber const& TimeBoundedSolverEnvironment::getPrecision() const {
    return precision;
}

void TimeBoundedSolverEnvironment::setPrecision(storm::RationalNumber value) {
    precision = value;
}

bool const& TimeBoundedSolverEnvironment::getRelativeTerminationCriterion() const {
    return relative;
}

void TimeBoundedSolverEnvironment::setRelativeTerminationCriterion(bool value) {
    relative = value;
}

storm::RationalNumber const& TimeBoundedSolverEnvironment::getUnifPlusKappa() const {
    return unifPlusKappa;
}

void TimeBoundedSolverEnvironment::setUnifPlusKappa(storm::RationalNumber value) {
    unifPlusKappa = value;
}

}  // namespace storm
