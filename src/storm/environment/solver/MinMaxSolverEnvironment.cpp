#include "storm/environment/solver/MinMaxSolverEnvironment.h"

#include "storm/environment/solver/MinMaxLpSolverEnvironment.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/MinMaxEquationSolverSettings.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {

MinMaxSolverEnvironment::MinMaxSolverEnvironment() {
    auto const& minMaxSettings = storm::settings::getModule<storm::settings::modules::MinMaxEquationSolverSettings>();

    minMaxMethod = minMaxSettings.getMinMaxEquationSolvingMethod();
    methodSetFromDefault = minMaxSettings.isMinMaxEquationSolvingMethodSetFromDefaultValue();
    if (minMaxSettings.isMaximalIterationCountSet()) {
        maxIterationCount = minMaxSettings.getMaximalIterationCount();
    } else {
        maxIterationCount = std::numeric_limits<uint_fast64_t>::max();
    }
    precision = storm::utility::convertNumber<storm::RationalNumber>(minMaxSettings.getPrecision());
    considerRelativeTerminationCriterion =
        minMaxSettings.getConvergenceCriterion() == storm::settings::modules::MinMaxEquationSolverSettings::ConvergenceCriterion::Relative;
    STORM_LOG_ASSERT(considerRelativeTerminationCriterion ||
                         minMaxSettings.getConvergenceCriterion() == storm::settings::modules::MinMaxEquationSolverSettings::ConvergenceCriterion::Absolute,
                     "Unknown convergence criterion.");
    multiplicationStyle = minMaxSettings.getValueIterationMultiplicationStyle();
    forceRequireUnique = minMaxSettings.isForceUniqueSolutionRequirementSet();
}

MinMaxSolverEnvironment::~MinMaxSolverEnvironment() {
    // Intentionally left empty
}

storm::solver::MinMaxMethod const& MinMaxSolverEnvironment::getMethod() const {
    return minMaxMethod;
}

bool const& MinMaxSolverEnvironment::isMethodSetFromDefault() const {
    return methodSetFromDefault;
}

void MinMaxSolverEnvironment::setMethod(storm::solver::MinMaxMethod value, bool isSetFromDefault) {
    methodSetFromDefault = isSetFromDefault;
    minMaxMethod = value;
}

uint64_t const& MinMaxSolverEnvironment::getMaximalNumberOfIterations() const {
    return maxIterationCount;
}

void MinMaxSolverEnvironment::setMaximalNumberOfIterations(uint64_t value) {
    maxIterationCount = value;
}

storm::RationalNumber const& MinMaxSolverEnvironment::getPrecision() const {
    return precision;
}

void MinMaxSolverEnvironment::setPrecision(storm::RationalNumber value) {
    precision = value;
}

bool const& MinMaxSolverEnvironment::getRelativeTerminationCriterion() const {
    return considerRelativeTerminationCriterion;
}

void MinMaxSolverEnvironment::setRelativeTerminationCriterion(bool value) {
    considerRelativeTerminationCriterion = value;
}

storm::solver::MultiplicationStyle const& MinMaxSolverEnvironment::getMultiplicationStyle() const {
    return multiplicationStyle;
}

void MinMaxSolverEnvironment::setMultiplicationStyle(storm::solver::MultiplicationStyle value) {
    multiplicationStyle = value;
}

MinMaxLpSolverEnvironment const& MinMaxSolverEnvironment::lp() const {
    return lpEnvironment.get();
}
MinMaxLpSolverEnvironment& MinMaxSolverEnvironment::lp() {
    return lpEnvironment.get();
}

bool MinMaxSolverEnvironment::isForceRequireUnique() const {
    return forceRequireUnique;
}

void MinMaxSolverEnvironment::setForceRequireUnique(bool value) {
    forceRequireUnique = value;
}

}  // namespace storm
