#include "storm/solver/SmtSolver.h"

#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

SmtSolver::ModelReference::ModelReference(storm::expressions::ExpressionManager const& manager) : manager(manager) {
    // Intentionally left empty.
}

storm::expressions::ExpressionManager const& SmtSolver::ModelReference::getManager() const {
    return manager;
}

SmtSolver::SmtSolver(storm::expressions::ExpressionManager& manager) : manager(manager) {
    // Intentionally left empty.
}

SmtSolver::~SmtSolver() {
    // Intentionally left empty.
}

void SmtSolver::add(std::set<storm::expressions::Expression> const& assertions) {
    for (storm::expressions::Expression assertion : assertions) {
        this->add(assertion);
    }
}

void SmtSolver::add(std::initializer_list<storm::expressions::Expression> const& assertions) {
    for (storm::expressions::Expression assertion : assertions) {
        this->add(assertion);
    }
}

void SmtSolver::pop(uint_fast64_t n) {
    for (uint_fast64_t i = 0; i < n; ++i) {
        this->pop();
    }
}

storm::expressions::SimpleValuation SmtSolver::getModelAsValuation() {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This solver does not support model generation.");
}

std::shared_ptr<SmtSolver::ModelReference> SmtSolver::getModel() {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This solver does not support model generation.");
}

std::vector<storm::expressions::SimpleValuation> SmtSolver::allSat(std::vector<storm::expressions::Variable> const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This solver does not support model generation.");
}

uint_fast64_t SmtSolver::allSat(std::vector<storm::expressions::Variable> const&, std::function<bool(storm::expressions::SimpleValuation&)> const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This solver does not support model generation.");
}

uint_fast64_t SmtSolver::allSat(std::vector<storm::expressions::Variable> const&, std::function<bool(ModelReference&)> const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This solver does not support model generation.");
}

std::vector<storm::expressions::Expression> SmtSolver::getUnsatCore() {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This solver does not support generation of unsatisfiable cores.");
}

std::vector<storm::expressions::Expression> SmtSolver::getUnsatAssumptions() {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This solver does not support generation of unsatisfiable cores.");
}

void SmtSolver::setInterpolationGroup(uint_fast64_t) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This solver does not support generation of interpolants.");
}

storm::expressions::Expression SmtSolver::getInterpolant(std::vector<uint_fast64_t> const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This solver does not support generation of interpolants.");
}

storm::expressions::ExpressionManager const& SmtSolver::getManager() const {
    return manager;
}

storm::expressions::ExpressionManager& SmtSolver::getManager() {
    return manager;
}

bool SmtSolver::setTimeout(uint_fast64_t) {
    return false;
}

bool SmtSolver::unsetTimeout() {
    return false;
}

std::string SmtSolver::getSmtLibString() const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This solver does not support exporting the assertions in the SMT-LIB format.");
    return "ERROR";
}

void SmtSolver::addNotCurrentModel(bool performSolverReset) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "This solver does not support model generation.");
}

}  // namespace solver
}  // namespace storm
