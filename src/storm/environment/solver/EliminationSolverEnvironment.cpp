#include "storm/environment/solver/EliminationSolverEnvironment.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/EliminationSettings.h"

namespace storm {

EliminationSolverEnvironment::EliminationSolverEnvironment() {
    auto const& eliminationSettings = storm::settings::getModule<storm::settings::modules::EliminationSettings>();

    order = eliminationSettings.getEliminationOrder();
    method = eliminationSettings.getEliminationMethod();
    maximalSccSize = eliminationSettings.getMaximalSccSize();
    eliminateEntryStatesLast = eliminationSettings.isEliminateEntryStatesLastSet();
}

EliminationSolverEnvironment::~EliminationSolverEnvironment() {
    // Intentionally left empty
}

storm::solver::stateelimination::EliminationOrder const& EliminationSolverEnvironment::getOrder() const {
    return order;
}

void EliminationSolverEnvironment::setOrder(storm::solver::stateelimination::EliminationOrder value) {
    order = value;
}

storm::solver::stateelimination::EliminationMethod const& EliminationSolverEnvironment::getMethod() const {
    return method;
}

void EliminationSolverEnvironment::setMethod(storm::solver::stateelimination::EliminationMethod value) {
    method = value;
}

uint64_t const& EliminationSolverEnvironment::getMaximalSccSize() const {
    return maximalSccSize;
}

void EliminationSolverEnvironment::setMaximalSccSize(uint64_t value) {
    maximalSccSize = value;
}

bool const& EliminationSolverEnvironment::isEliminateEntryStatesLastSet() const {
    return eliminateEntryStatesLast;
}

void EliminationSolverEnvironment::setEliminateEntryStatesLast(bool value) {
    eliminateEntryStatesLast = value;
}

}  // namespace storm
