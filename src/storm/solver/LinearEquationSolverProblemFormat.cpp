#include "LinearEquationSolverProblemFormat.h"

namespace storm {
namespace solver {

std::ostream& operator<<(std::ostream& out, LinearEquationSolverProblemFormat const& format) {
    switch (format) {
        case LinearEquationSolverProblemFormat::EquationSystem:
            out << "equation system";
            break;
        case LinearEquationSolverProblemFormat::FixedPointSystem:
            out << "fixed point system";
            break;
    }
    return out;
}

}  // namespace solver
}  // namespace storm
