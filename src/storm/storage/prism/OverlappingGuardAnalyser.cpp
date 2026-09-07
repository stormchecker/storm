#include "OverlappingGuardAnalyser.h"

#include "storm/solver/SmtSolver.h"
#include "storm/storage/prism/Program.h"

namespace storm {
namespace prism {
OverlappingGuardAnalyser::OverlappingGuardAnalyser(Program const& program, std::shared_ptr<storm::utility::solver::SmtSolverFactory>& smtSolverFactory)
    : program(program), smtSolver(smtSolverFactory->create(program.getManager())), initializedWithStateConstraints(false) {
    // Intentionally left empty.
}

bool OverlappingGuardAnalyser::hasModuleWithInnerActionOverlap() {
    if (!initializedWithStateConstraints) {
        std::vector<storm::expressions::Expression> expressions = program.getAllRangeExpressions();
        if (!expressions.empty()) {
            smtSolver->add(storm::expressions::conjunction(expressions));
        }
    }

    for (auto const& module : program.getModules()) {
        for (auto const& actionIndex : module.getSynchronizingActionIndices()) {
            auto const& commandIndices = module.getCommandIndicesByActionIndex(actionIndex);
            if (commandIndices.size() == 1) {
                continue;
            } else {
                for (uint64_t commandIndexA : commandIndices) {
                    for (uint64_t commandIndexB : commandIndices) {
                        if (commandIndexA <= commandIndexB) {
                            continue;
                        }
                        smtSolver->push();
                        smtSolver->add(module.getCommand(commandIndexA).getGuardExpression());
                        smtSolver->add(module.getCommand(commandIndexB).getGuardExpression());
                        auto smtCheckResult = smtSolver->check();
                        smtSolver->pop();
                        if (smtCheckResult == storm::solver::SmtSolver::CheckResult::Sat) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

}  // namespace prism
}  // namespace storm