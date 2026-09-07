#include "EquivalenceChecker.h"

#include "storm/solver/SmtSolver.h"
#include "storm/storage/expressions/Expression.h"

namespace storm {
namespace expressions {

EquivalenceChecker::EquivalenceChecker(std::unique_ptr<storm::solver::SmtSolver>&& smtSolver, boost::optional<storm::expressions::Expression> const& constraint)
    : smtSolver(std::move(smtSolver)) {
    if (constraint) {
        this->smtSolver->add(constraint.get());
    }
}

void EquivalenceChecker::addConstraints(std::vector<storm::expressions::Expression> const& constraints) {
    for (auto const& constraint : constraints) {
        this->smtSolver->add(constraint);
    }
}

bool EquivalenceChecker::areEquivalent(storm::expressions::Expression const& first, storm::expressions::Expression const& second) {
    this->smtSolver->push();
    this->smtSolver->add(!storm::expressions::iff(first, second));
    bool equivalent = smtSolver->check() == storm::solver::SmtSolver::CheckResult::Unsat;
    this->smtSolver->pop();
    return equivalent;
}

bool EquivalenceChecker::areEquivalentModuloNegation(storm::expressions::Expression const& first, storm::expressions::Expression const& second) {
    this->smtSolver->push();
    this->smtSolver->add(!storm::expressions::iff(first, second));
    bool equivalent = smtSolver->check() == storm::solver::SmtSolver::CheckResult::Unsat;
    if (equivalent) {
        this->smtSolver->pop();
        return true;
    }
    this->smtSolver->pop();
    this->smtSolver->push();
    this->smtSolver->add(!storm::expressions::iff(first, !second));
    equivalent = smtSolver->check() == storm::solver::SmtSolver::CheckResult::Unsat;

    this->smtSolver->pop();
    return equivalent;
}

}  // namespace expressions
}  // namespace storm
