#include "storm/environment/solver/EliminationSolverEnvironment.h"

namespace storm {

EliminationSolverEnvironment::EliminationSolverEnvironment()
    : order(storm::solver::stateelimination::EliminationOrder::ForwardReversed),
      method(storm::solver::stateelimination::EliminationMethod::State),
      maximalSccSize(20),
      eliminateEntryStatesLast(false) {
    // Intentionally left empty.
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
