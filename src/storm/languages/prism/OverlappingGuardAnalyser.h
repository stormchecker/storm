#pragma once
#include <cstdint>
#include <memory>

namespace storm {
namespace utility {
namespace solver {
class SmtSolverFactory;
}
}  // namespace utility

namespace solver {
class SmtSolver;
}

namespace prism {
class Program;
class Module;

class OverlappingGuardAnalyser {
   public:
    OverlappingGuardAnalyser(Program const& program, std::shared_ptr<storm::utility::solver::SmtSolverFactory>& smtSolverFactory);
    /*!
     * returns true iff there are two commands that
     *  * are contained in the same module,
     *  * either have the same action label or are both unlabeled, and
     *  * have overlapping guards, i.e., can be enabled simultaneously.
     */
    bool hasModuleWithInnerActionOverlap();

   private:
    Program const& program;
    std::unique_ptr<storm::solver::SmtSolver> smtSolver;
    bool initializedWithStateConstraints;
};
}  // namespace prism
}  // namespace storm
