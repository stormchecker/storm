#include "storm/environment/solver/LongRunAverageSolverEnvironment.h"

#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {

LongRunAverageSolverEnvironment::LongRunAverageSolverEnvironment()
    : detMethod(storm::solver::LraMethod::GainBiasEquations),
      detMethodSetFromDefault(true),
      nondetMethod(storm::solver::LraMethod::ValueIteration),
      nondetMethodSetFromDefault(true),
      precision(storm::utility::convertNumber<storm::RationalNumber>(1e-06)),
      relative(true),
      maxIters(boost::none),
      aperiodicFactor(storm::utility::convertNumber<storm::RationalNumber>(0.125)) {
    // Intentionally left empty.
}

LongRunAverageSolverEnvironment::~LongRunAverageSolverEnvironment() {
    // Intentionally left empty
}

storm::solver::LraMethod const& LongRunAverageSolverEnvironment::getDetLraMethod() const {
    return detMethod;
}

bool const& LongRunAverageSolverEnvironment::isDetLraMethodSetFromDefault() const {
    return detMethodSetFromDefault;
}

void LongRunAverageSolverEnvironment::setDetLraMethod(storm::solver::LraMethod value, bool isSetFromDefault) {
    detMethod = value;
    detMethodSetFromDefault = isSetFromDefault;
}

storm::solver::LraMethod const& LongRunAverageSolverEnvironment::getNondetLraMethod() const {
    return nondetMethod;
}

bool const& LongRunAverageSolverEnvironment::isNondetLraMethodSetFromDefault() const {
    return nondetMethodSetFromDefault;
}

void LongRunAverageSolverEnvironment::setNondetLraMethod(storm::solver::LraMethod value, bool isSetFromDefault) {
    nondetMethod = value;
    nondetMethodSetFromDefault = isSetFromDefault;
}

storm::RationalNumber const& LongRunAverageSolverEnvironment::getPrecision() const {
    return precision;
}

void LongRunAverageSolverEnvironment::setPrecision(storm::RationalNumber value) {
    precision = value;
}

bool const& LongRunAverageSolverEnvironment::getRelativeTerminationCriterion() const {
    return relative;
}

void LongRunAverageSolverEnvironment::setRelativeTerminationCriterion(bool value) {
    relative = value;
}

bool LongRunAverageSolverEnvironment::isMaximalIterationCountSet() const {
    return maxIters.is_initialized();
}

uint64_t LongRunAverageSolverEnvironment::getMaximalIterationCount() const {
    return maxIters.get();
}

void LongRunAverageSolverEnvironment::setMaximalIterationCount(uint64_t value) {
    maxIters = value;
}

void LongRunAverageSolverEnvironment::unsetMaximalIterationCount() {
    maxIters = boost::none;
}

storm::RationalNumber const& LongRunAverageSolverEnvironment::getAperiodicFactor() const {
    return aperiodicFactor;
}

void LongRunAverageSolverEnvironment::setAperiodicFactor(storm::RationalNumber value) {
    aperiodicFactor = value;
}

}  // namespace storm
