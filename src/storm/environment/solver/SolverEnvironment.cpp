#include "storm/environment/solver/SolverEnvironment.h"

#include "storm/environment/solver/AllSolverEnvironments.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/DebugSettings.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/solver/SolverSelectionOptions.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/InvalidEnvironmentException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {

SolverEnvironment::SolverEnvironment() {
    auto generalSettings = storm::settings::getModule<storm::settings::modules::GeneralSettings>();
    forceSoundness = generalSettings.isSoundSet();
    forceExact = generalSettings.isExactSet() || generalSettings.isExactFinitePrecisionSet();
    linearEquationSolverType = storm::settings::getModule<storm::settings::modules::CoreSettings>().getEquationSolver();
    linearEquationSolverTypeSetFromDefault = storm::settings::getModule<storm::settings::modules::CoreSettings>().isEquationSolverSetFromDefaultValue();
    lpSolverType = storm::settings::getModule<storm::settings::modules::CoreSettings>().getLpSolver();
    lpSolverTypeSetFromDefault = storm::settings::getModule<storm::settings::modules::CoreSettings>().isLpSolverSetFromDefaultValue();
    debug = storm::settings::getModule<storm::settings::modules::DebugSettings>().isDebugSet();
}

SolverEnvironment::~SolverEnvironment() {
    // Intentionally left empty
}

LongRunAverageSolverEnvironment& SolverEnvironment::lra() {
    return longRunAverageSolverEnvironment.get();
}

LongRunAverageSolverEnvironment const& SolverEnvironment::lra() const {
    return longRunAverageSolverEnvironment.get();
}

TimeBoundedSolverEnvironment& SolverEnvironment::timeBounded() {
    return timeBoundedSolverEnvironment.get();
}

TimeBoundedSolverEnvironment const& SolverEnvironment::timeBounded() const {
    return timeBoundedSolverEnvironment.get();
}

MinMaxSolverEnvironment& SolverEnvironment::minMax() {
    return minMaxSolverEnvironment.get();
}

MinMaxSolverEnvironment const& SolverEnvironment::minMax() const {
    return minMaxSolverEnvironment.get();
}

MultiplierEnvironment& SolverEnvironment::multiplier() {
    return multiplierEnvironment.get();
}

MultiplierEnvironment const& SolverEnvironment::multiplier() const {
    return multiplierEnvironment.get();
}

EigenSolverEnvironment& SolverEnvironment::eigen() {
    return eigenSolverEnvironment.get();
}

EigenSolverEnvironment const& SolverEnvironment::eigen() const {
    return eigenSolverEnvironment.get();
}

GmmxxSolverEnvironment& SolverEnvironment::gmmxx() {
    return gmmxxSolverEnvironment.get();
}

GmmxxSolverEnvironment const& SolverEnvironment::gmmxx() const {
    return gmmxxSolverEnvironment.get();
}

NativeSolverEnvironment& SolverEnvironment::native() {
    return nativeSolverEnvironment.get();
}

NativeSolverEnvironment const& SolverEnvironment::native() const {
    return nativeSolverEnvironment.get();
}

GameSolverEnvironment& SolverEnvironment::game() {
    return gameSolverEnvironment.get();
}

GameSolverEnvironment const& SolverEnvironment::game() const {
    return gameSolverEnvironment.get();
}

TopologicalSolverEnvironment& SolverEnvironment::topological() {
    return topologicalSolverEnvironment.get();
}

TopologicalSolverEnvironment const& SolverEnvironment::topological() const {
    return topologicalSolverEnvironment.get();
}

EliminationSolverEnvironment& SolverEnvironment::elimination() {
    return eliminationSolverEnvironment.get();
}

EliminationSolverEnvironment const& SolverEnvironment::elimination() const {
    return eliminationSolverEnvironment.get();
}

OviSolverEnvironment& SolverEnvironment::ovi() {
    return oviSolverEnvironment.get();
}

OviSolverEnvironment const& SolverEnvironment::ovi() const {
    return oviSolverEnvironment.get();
}

GurobiSolverEnvironment& SolverEnvironment::gurobi() {
    return gurobiSolverEnvironment.get();
}

GurobiSolverEnvironment const& SolverEnvironment::gurobi() const {
    return gurobiSolverEnvironment.get();
}

GlpkSolverEnvironment& SolverEnvironment::glpk() {
    return glpkSolverEnvironment.get();
}

GlpkSolverEnvironment const& SolverEnvironment::glpk() const {
    return glpkSolverEnvironment.get();
}

bool SolverEnvironment::isForceSoundness() const {
    return forceSoundness;
}

void SolverEnvironment::setForceSoundness(bool value) {
    SolverEnvironment::forceSoundness = value;
}

bool SolverEnvironment::isForceExact() const {
    return forceExact;
}

void SolverEnvironment::setForceExact(bool value) {
    SolverEnvironment::forceExact = value;
}

bool SolverEnvironment::isDebugSet() const {
    return debug;
}

void SolverEnvironment::setDebug(bool value) {
    SolverEnvironment::debug = value;
}

storm::solver::EquationSolverType const& SolverEnvironment::getLinearEquationSolverType() const {
    return linearEquationSolverType;
}

void SolverEnvironment::setLinearEquationSolverType(storm::solver::EquationSolverType const& value, bool isSetFromDefault) {
    linearEquationSolverTypeSetFromDefault = isSetFromDefault;
    linearEquationSolverType = value;
}

bool SolverEnvironment::isLinearEquationSolverTypeSetFromDefaultValue() const {
    return linearEquationSolverTypeSetFromDefault;
}

storm::solver::LpSolverType const& SolverEnvironment::getLpSolverType() const {
    return lpSolverType;
}

void SolverEnvironment::setLpSolverType(storm::solver::LpSolverType const& value, bool isSetFromDefault) {
    lpSolverTypeSetFromDefault = isSetFromDefault;
    lpSolverType = value;
}

bool SolverEnvironment::isLpSolverTypeSetFromDefaultValue() const {
    return lpSolverTypeSetFromDefault;
}

std::pair<boost::optional<storm::RationalNumber>, boost::optional<bool>> SolverEnvironment::getPrecisionOfLinearEquationSolver(
    storm::solver::EquationSolverType const& solverType) const {
    std::pair<boost::optional<storm::RationalNumber>, boost::optional<bool>> result;
    switch (solverType) {
        case storm::solver::EquationSolverType::Gmmxx:
            result.first = gmmxx().getPrecision();
            break;
        case storm::solver::EquationSolverType::Eigen:
            result.first = eigen().getPrecision();
            break;
        case storm::solver::EquationSolverType::Native:
            result.first = native().getPrecision();
            result.second = native().getRelativeTerminationCriterion();
            break;
        case storm::solver::EquationSolverType::Elimination:
            break;
        case storm::solver::EquationSolverType::Topological:
            result = getPrecisionOfLinearEquationSolver(topological().getUnderlyingEquationSolverType());
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "The selected solver type is unknown.");
    }
    return result;
}

void SolverEnvironment::setLinearEquationSolverPrecision(boost::optional<storm::RationalNumber> const& newPrecision,
                                                         boost::optional<bool> const& relativePrecision) {
    // Assert that each solver type is handled in this method.
    STORM_LOG_ASSERT(getLinearEquationSolverType() == storm::solver::EquationSolverType::Native ||
                         getLinearEquationSolverType() == storm::solver::EquationSolverType::Gmmxx ||
                         getLinearEquationSolverType() == storm::solver::EquationSolverType::Eigen ||
                         getLinearEquationSolverType() == storm::solver::EquationSolverType::Elimination ||
                         getLinearEquationSolverType() == storm::solver::EquationSolverType::Topological,
                     "The current solver type is not respected in this method.");
    if (newPrecision) {
        native().setPrecision(newPrecision.get());
        gmmxx().setPrecision(newPrecision.get());
        eigen().setPrecision(newPrecision.get());
        // Elimination and Topological solver do not have a precision
    }
    if (relativePrecision) {
        native().setRelativeTerminationCriterion(relativePrecision.get());
        // gmm, eigen, elimination, and topological solvers do not have a precision
    }
}
}  // namespace storm
